#include "fol/core.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QStringDecoder>
#include <QtEndian>
#include <zlib.h>

namespace fol {
namespace {
// Read-only subset of the legacy unencrypted BuildPatch wire format (17/18).
// Independent bounded reader; no downloader, decryption, launch or repair path.
class Cursor {
    QByteArray data;
    qsizetype position = 0;

  public:
    explicit Cursor(QByteArray bytes) : data(std::move(bytes)) {}
    qsizetype remaining() const { return data.size() - position; }
    QByteArray take(qsizetype count) {
        require(count >= 0 && count <= remaining(), "Truncated manifest field");
        auto result = data.mid(position, count);
        position += count;
        return result;
    }
    quint32 u32() { return qFromLittleEndian<quint32>(take(4).constData()); }
    quint8 u8() { return quint8(take(1)[0]); }
    QString string() {
        auto length = qint32(u32());
        require(length >= -65536 && length <= 65536, "Manifest string exceeds bound");
        if (!length)
            return {};
        bool wide = length < 0;
        auto raw = take(qsizetype(wide ? -length : length) * (wide ? 2 : 1));
        auto terminator = wide ? 2 : 1;
        require(raw.endsWith(QByteArray(terminator, '\0')), "Unterminated manifest string");
        raw.chop(terminator);
        QStringDecoder decoder(wide ? QStringDecoder::Utf16LE : QStringDecoder::Utf8);
        QString decoded = decoder(raw);
        require(!decoder.hasError() && !decoded.contains(QChar('\0')),
                "Invalid manifest string encoding");
        return decoded;
    }
    Cursor block() {
        auto length = u32();
        require(length >= 5, "Invalid manifest block size");
        return Cursor(take(length - 4));
    }
};
struct FileRecord {
    QString path;
    QByteArray sha1;
    QStringList tags;
    quint64 size = 0;
};
QString safePath(QString path) {
    path.replace('\\', '/');
    require(!path.isEmpty() && !QDir::isAbsolutePath(path) && !path.contains(':') &&
                !path.split('/').contains("..") && !path.split('/').contains(".") &&
                !path.split('/').contains(""),
            "Unsafe manifest file path");
    return path;
}
} // namespace
QJsonObject inspectEpicManifest(const QString &manifest, const QString &installation,
                                bool hashFiles) {
    require(!hashFiles || !installation.isEmpty(), "File hashing requires an installation root");
    constexpr quint32 limit = 64 * 1024 * 1024;
    require(QFileInfo(manifest).isFile(), "Manifest is not a regular file");
    Cursor header(readFile(manifest, limit));
    require(header.u32() == 0x44bec00c, "Not a binary BuildPatch manifest");
    auto headerSize = header.u32(), rawSize = header.u32(), storedSize = header.u32();
    auto digest = header.take(20);
    auto flags = header.u8();
    auto version = header.u32();
    require(version == 17 || version == 18,
            "Unsupported manifest version; only legacy 17/18 are supported");
    require(flags == 0 || flags == 1, "Encrypted or unknown manifest storage is unsupported");
    require(headerSize == 41 && rawSize > 0 && rawSize <= limit && storedSize <= limit &&
                header.remaining() == storedSize,
            "Invalid manifest sizes");
    auto stored = header.take(storedSize);
    QByteArray body;
    if (flags == 1) {
        body.resize(rawSize);
        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef *>(stored.data());
        stream.avail_in = storedSize;
        stream.next_out = reinterpret_cast<Bytef *>(body.data());
        stream.avail_out = rawSize;
        require(inflateInit(&stream) == Z_OK, "Cannot initialize manifest decompressor");
        auto status = inflate(&stream, Z_FINISH);
        bool complete =
            status == Z_STREAM_END && stream.total_in == storedSize && stream.total_out == rawSize;
        inflateEnd(&stream);
        require(complete, "Manifest decompression failed or size mismatched");
    } else {
        require(storedSize == rawSize, "Uncompressed manifest size mismatch");
        body = stored;
    }
    require(QCryptographicHash::hash(body, QCryptographicHash::Sha1) == digest,
            "Manifest payload SHA-1 mismatch");
    Cursor payload(body);
    auto metadata = payload.block();
    require(metadata.u8() <= 1, "Unsupported legacy manifest metadata");
    auto feature = metadata.u32();
    // Container serialization and metadata feature levels need not be identical.
    // The supplied 12.50 container is v18 with feature level 13. Chunk transport
    // fields vary across these levels but are deliberately not interpreted here.
    require(feature >= 13 && feature <= 18,
            "Unsupported metadata feature level: " + QString::number(feature));
    require(metadata.u8() == 0, "Legacy file-data manifests are unsupported");
    metadata.u32(); // Application number; not a launch/authentication identifier.
    auto app = metadata.string(), build = metadata.string(), executable = metadata.string();
    payload.block(); // Chunk transport data is not used for local file verification.
    auto files = payload.block();
    require(files.u8() == 0, "Unsupported file-list format");
    auto count = files.u32();
    require(count > 0 && count <= 100000 && count <= quint64(files.remaining()) / 33,
            "Manifest file count exceeds bounds");
    QList<FileRecord> entries;
    QSet<QString> paths;
    for (quint32 i = 0; i < count; ++i) {
        auto path = safePath(files.string());
        require(!paths.contains(path.toCaseFolded()), "Duplicate manifest file path");
        paths.insert(path.toCaseFolded());
        entries.append(FileRecord{path, {}, {}, 0});
    }
    for (quint32 i = 0; i < count; ++i)
        require(files.string().isEmpty(), "Manifest symlink entries are unsupported");
    for (auto &entry : entries)
        entry.sha1 = files.take(20);
    files.take(count); // File attribute flags do not authorize changes to local files.
    for (auto &entry : entries) {
        auto tags = files.u32();
        require(tags <= 1024, "Manifest install-tag count exceeds bound");
        while (tags--)
            entry.tags.append(files.string());
    }
    for (auto &entry : entries) {
        auto parts = files.u32();
        require(parts <= quint64(files.remaining()) / 28, "Invalid manifest chunk-part count");
        while (parts--) {
            require(files.u32() == 28, "Unsupported chunk-part record size");
            files.take(20); // GUID and source chunk offset; neither is a filesystem path.
            entry.size += files.u32();
        }
    }
    require(files.remaining() == 0, "Unexpected trailing file-list data");
    QJsonObject report{{"manifest", QFileInfo(manifest).absoluteFilePath()},
                       {"format_version", int(version)},
                       {"metadata_feature_level", int(feature)},
                       {"application", app},
                       {"build_version", build},
                       {"launch_executable_metadata", executable},
                       {"manifest_payload_hash_matches", true},
                       {"authenticity", "not established; local manifest is not authenticated"},
                       {"file_count", int(count)},
                       {"file_hashes_checked", hashFiles}};
    QString root;
    if (!installation.isEmpty()) {
        root = QFileInfo(installation).canonicalFilePath();
        require(!root.isEmpty() && QFileInfo(root).isDir(), "Installation root is not a directory");
    }
    QJsonArray results;
    quint64 total = 0, hashed = 0;
    int missing = 0, mismatched = 0, matched = 0;
    for (const auto &entry : entries) {
        total += entry.size;
        if (root.isEmpty())
            continue;
        QJsonObject file{{"path", entry.path},
                         {"expected_size", qint64(entry.size)},
                         {"install_tags", QJsonArray::fromStringList(entry.tags)}};
        QFileInfo info(root + "/" + entry.path);
        QString status;
        if (!info.exists()) {
            status = "missing";
            ++missing;
        } else {
            auto canonical = info.canonicalFilePath();
            require(info.isFile() && canonical.startsWith(root + "/"),
                    "Manifest file escapes root or is not regular: " + entry.path);
            if (quint64(info.size()) != entry.size) {
                status = "size-mismatch";
                ++mismatched;
            } else if (!hashFiles) {
                status = "size-matches; hash-not-checked";
                ++matched;
            } else {
                QFile input(info.absoluteFilePath());
                require(input.open(QIODevice::ReadOnly),
                        "Cannot read manifest-listed file: " + entry.path);
                QCryptographicHash hash(QCryptographicHash::Sha1);
                quint64 readBytes = 0;
                while (!input.atEnd()) {
                    auto block = input.read(4 * 1024 * 1024);
                    require(input.error() == QFile::NoError, "Read error hashing " + entry.path);
                    readBytes += block.size();
                    require(readBytes <= entry.size, "File grew while hashing " + entry.path);
                    hash.addData(block);
                }
                hashed += readBytes;
                bool match = readBytes == entry.size && hash.result() == entry.sha1;
                status = match ? "hash-matches" : "hash-mismatch";
                if (match)
                    ++matched;
                else
                    ++mismatched;
            }
        }
        file["status"] = status;
        results.append(file);
    }
    report["expected_bytes"] = qint64(total);
    if (!root.isEmpty()) {
        report["installation"] = root;
        report["files"] = results;
        report["missing"] = missing;
        report["mismatched"] = mismatched;
        report["matched"] = matched;
        report["hashed_bytes"] = qint64(hashed);
        report["all_listed_files_match"] = missing == 0 && mismatched == 0;
        report["scope"] = hashFiles
                              ? "sizes and SHA-1 of all manifest-listed files; not authenticity"
                              : "file presence and sizes only";
    }
    return report;
}
} // namespace fol
