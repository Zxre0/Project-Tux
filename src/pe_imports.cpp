#include "fol/core.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSet>
#include <QtEndian>

namespace fol {
namespace {
quint32 u32(const QByteArray &bytes, qsizetype offset) {
    require(offset >= 0 && offset + 4 <= bytes.size(), "Truncated PE integer");
    return qFromLittleEndian<quint32>(bytes.constData() + offset);
}
struct Section {
    quint64 address, span, raw, size;
};
class ImageReader {
    QFile file;
    QList<Section> sections;
    quint32 headers = 0;
    QByteArray optional;

  public:
    explicit ImageReader(const QString &path) : file(path) {
        auto info = inspectPe(path);
        require(info.valid && info.x64, "Imports require a valid x64 PE image: " + info.error);
        require(file.open(QIODevice::ReadOnly), "Cannot read PE import image");
        auto dos = at(0, 64);
        auto pe = u32(dos, 60);
        auto coff = at(pe, 24);
        const auto count = qFromLittleEndian<quint16>(coff.constData() + 6);
        const auto optionalSize = qFromLittleEndian<quint16>(coff.constData() + 20);
        require(optionalSize >= 112 && optionalSize <= 4096, "Invalid PE optional header size");
        optional = at(quint64(pe) + 24, optionalSize);
        headers = u32(optional, 60);
        const auto table = quint64(pe) + 24 + optionalSize;
        require(headers >= table + quint64(count) * 40 && headers <= quint64(file.size()),
                "Invalid PE header extent");
        require(u32(optional, 108) <= quint32((optional.size() - 112) / 8),
                "PE data directory count exceeds optional header");
        for (quint16 i = 0; i < count; ++i) {
            auto data = at(table + quint64(i) * 40, 40);
            Section section{u32(data, 12), qMax(u32(data, 8), u32(data, 16)), u32(data, 20),
                            u32(data, 16)};
            if (!section.span)
                continue;
            require(section.address >= headers && section.address + section.span <= (1ULL << 32),
                    "Invalid PE section virtual extent");
            for (const auto &other : sections)
                require(section.address + section.span <= other.address ||
                            other.address + other.span <= section.address,
                        "Overlapping PE virtual sections");
            sections.append(section);
        }
    }
    QByteArray at(quint64 offset, quint64 count) {
        require(count <= 1024 * 1024 && offset <= quint64(file.size()) &&
                    count <= quint64(file.size()) - offset && file.seek(qint64(offset)),
                "PE read exceeds file or bound");
        auto bytes = file.read(qint64(count));
        require(quint64(bytes.size()) == count && file.error() == QFile::NoError,
                "Truncated PE import data");
        return bytes;
    }
    QByteArray rva(quint64 address, quint64 count) {
        require(address + count <= (1ULL << 32), "PE RVA overflow");
        if (address < headers && address + count <= headers)
            return at(address, count);
        for (const auto &section : sections)
            if (address >= section.address && address - section.address < section.size &&
                count <= section.size - (address - section.address))
                return at(section.raw + address - section.address, count);
        throw Error("PE import RVA is not backed by file data");
    }
    QString name(quint32 address) {
        require(address != 0, "PE import name has null RVA");
        QByteArray bytes;
        for (quint64 i = 0; i <= 255; ++i) {
            const auto byte = rva(quint64(address) + i, 1)[0];
            if (!byte) {
                require(!bytes.isEmpty() && bytes != "." && bytes != ".." && !bytes.endsWith(' ') &&
                            !bytes.endsWith('.'),
                        "Invalid PE import basename");
                return QString::fromLatin1(bytes);
            }
            require(byte >= 32 && byte <= 126 && !QByteArray("<>:\"/\\|?*").contains(byte),
                    "Unsafe or non-ASCII PE import basename");
            bytes.append(byte);
        }
        throw Error("PE import basename exceeds 255 bytes");
    }
    QJsonArray imports(int index, bool delayed) {
        QJsonArray result;
        if (u32(optional, 108) <= quint32(index))
            return result;
        auto address = u32(optional, 112 + index * 8);
        auto size = u32(optional, 116 + index * 8);
        if (!address && !size)
            return result;
        const quint32 width = delayed ? 32 : 20;
        require(address && size >= width && size <= width * 4096,
                "Invalid PE import directory size/address");
        auto table = rva(address, size);
        QSet<QString> names;
        for (quint32 offset = 0; offset + width <= size; offset += width) {
            auto descriptor = table.mid(offset, width);
            if (descriptor == QByteArray(width, '\0'))
                return result;
            if (delayed)
                require(u32(descriptor, 0) == 1,
                        "Unsupported delayed-import flags; only RVA-based descriptors supported");
            const auto dll = name(u32(descriptor, delayed ? 4 : 12));
            require(!names.contains(dll.toCaseFolded()), "Duplicate PE import descriptor");
            names.insert(dll.toCaseFolded());
            result.append(QJsonObject{{"name", dll}, {"kind", delayed ? "delayed" : "immediate"}});
        }
        throw Error("PE import directory lacks null terminator");
    }
};
} // namespace

QJsonObject peImportReport(const QString &path, const QStringList &directories) {
    require(directories.size() <= 16, "At most 16 additional DLL inventory directories");
    ImageReader image(path);
    auto entries = image.imports(1, false);
    const auto delayed = image.imports(13, true);
    const auto immediateCount = entries.size();
    for (const auto &entry : delayed)
        entries.append(entry);
    QStringList roots{QFileInfo(path).absolutePath()};
    roots.append(directories);
    QStringList canonicalRoots;
    QMap<QString, QStringList> inventory;
    QSet<QString> required;
    for (const auto &entry : entries)
        required.insert(entry.toObject()["name"].toString().toCaseFolded());
    for (const auto &root : roots) {
        QFileInfo directory(root);
        const auto canonical = directory.canonicalFilePath();
        require(directory.isDir() && !canonical.isEmpty(),
                "Invalid DLL inventory directory: " + root);
        if (canonicalRoots.contains(canonical))
            continue;
        canonicalRoots.append(canonical);
        QDirIterator files(canonical, QDir::Files | QDir::Readable);
        int count = 0;
        while (files.hasNext()) {
            require(++count <= 100000, "DLL inventory directory exceeds entry bound");
            files.next();
            const auto name = files.fileName().toCaseFolded();
            if (required.contains(name))
                inventory[name].append(files.filePath());
        }
    }
    QJsonArray annotated;
    int unavailable = 0, noValidCandidate = 0;
    for (const auto &entry : entries) {
        auto item = entry.toObject();
        const auto dll = item["name"].toString().toCaseFolded();
        const bool contract = dll.startsWith("api-ms-") || dll.startsWith("ext-ms-");
        item["api_set_contract"] = contract;
        QJsonArray candidates;
        int validCandidates = 0;
        for (const auto &candidate : inventory.value(dll)) {
            auto pe = inspectPe(candidate);
            const bool valid = pe.valid && pe.x64 && pe.dll;
            validCandidates += valid;
            const auto error = !pe.valid ? pe.error
                               : !pe.x64 ? QString("PE architecture is not x64")
                               : !pe.dll ? QString("PE file is not a DLL")
                                         : QString{};
            candidates.append(
                QJsonObject{{"path", candidate}, {"valid_x64_dll", valid}, {"error", error}});
        }
        item["candidates"] = candidates;
        item["valid_x64_candidate_count"] = validCandidates;
        item["status"] = !candidates.isEmpty() ? "candidate-found; loader-resolution-unverified"
                         : contract            ? "api-set-contract; runtime-mapping-unverified"
                                               : "not-found-in-inventoried-directories";
        if (candidates.isEmpty() && !contract)
            ++unavailable;
        if (!validCandidates && !contract)
            ++noValidCandidate;
        annotated.append(item);
    }
    return {
        {"image", QFileInfo(path).absoluteFilePath()},
        {"immediate_count", qint64(immediateCount)},
        {"delayed_count", qint64(delayed.size())},
        {"directories", QJsonArray::fromStringList(canonicalRoots)},
        {"imports", annotated},
        {"imports_without_candidate", unavailable},
        {"imports_without_valid_x64_candidate", noValidCandidate},
        {"scope", "direct DLL names and explicit directory inventory only; no execution, "
                  "Wine search-order emulation, export checks or transitive/runtime loads"},
        {"warning", "Absent delayed imports or API-set files do not establish a startup failure"}};
}
} // namespace fol
