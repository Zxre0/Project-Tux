#include "fol/core.h"
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QtEndian>

namespace fol {
PeInfo inspectPe(const QString &path) {
    PeInfo result;
    try {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "Unreadable PE: " + path);
        auto dos = file.read(64);
        require(dos.size() == 64 && dos.startsWith("MZ"), "Missing DOS header");
        quint32 offset = qFromLittleEndian<quint32>(dos.constData() + 60);
        require(offset >= 64 && offset <= file.size() - 24 && file.seek(offset),
                "Invalid PE offset");
        auto header = file.read(24);
        require(header.startsWith(QByteArray("PE\0\0", 4)), "Missing PE signature");
        auto machine = qFromLittleEndian<quint16>(header.constData() + 4);
        auto sections = qFromLittleEndian<quint16>(header.constData() + 6);
        auto optionalSize = qFromLittleEndian<quint16>(header.constData() + 20);
        auto characteristics = qFromLittleEndian<quint16>(header.constData() + 22);
        require(sections > 0 && sections <= 96 && optionalSize >= 112, "Invalid PE header sizes");
        auto optional = file.read(optionalSize);
        require(optional.size() == optionalSize, "Truncated optional header");
        result.x64 = machine == 0x8664 && qFromLittleEndian<quint16>(optional.constData()) == 0x20b;
        result.dll = characteristics & 0x2000;
        require(characteristics & 2, "PE is not executable");
        for (quint16 i = 0; i < sections; ++i) {
            auto section = file.read(40);
            require(section.size() == 40, "Truncated section table");
            quint64 size = qFromLittleEndian<quint32>(section.constData() + 16);
            quint64 start = qFromLittleEndian<quint32>(section.constData() + 20);
            require(!size || (start && start + size <= quint64(file.size())),
                    "Section exceeds file size");
        }
        result.valid = true;
    } catch (const Error &error) {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}
static void extract(const QByteArray &bytes, const QString &source, Build &build,
                    QSet<QString> &versions, QSet<QString> &engines) {
    // Preserve ASCII bytes from UTF-16LE metadata as well as ordinary UTF-8 build strings.
    QStringList texts{QString::fromLatin1(bytes)};
    for (int alignment = 0; alignment < 2; ++alignment) {
        QByteArray ascii;
        ascii.reserve(bytes.size() / 2);
        for (qsizetype i = alignment; i + 1 < bytes.size(); i += 2)
            ascii.append(bytes[i + 1] == 0 ? bytes[i] : '\0');
        texts.append(QString::fromLatin1(ascii));
    }
    static const QRegularExpression release(
        "\\+\\+Fortnite\\+Release-([0-9]{1,2}\\.[0-9]{1,2})(?![0-9.])");
    static const QRegularExpression engine(
        "([45]\\.[0-9]{1,2}\\.[0-9]+)-[0-9]+\\+{2,3}Fortnite\\+Release-");
    for (const auto &text : texts) {
        auto matches = release.globalMatch(text);
        while (matches.hasNext()) {
            auto match = matches.next();
            versions.insert(match.captured(1));
            QString evidence = source + ": " + match.captured();
            if (!build.evidence.contains(evidence))
                build.evidence.append(evidence);
        }
        auto engineMatches = engine.globalMatch(text);
        while (engineMatches.hasNext())
            engines.insert(engineMatches.next().captured(1));
    }
}
Build inspectBuild(const QString &path) {
    Build build;
    build.root = QFileInfo(path).canonicalFilePath();
    if (build.root.isEmpty() || !QFileInfo(build.root).isDir()) {
        build.errors.append("Installation directory does not exist: " + path);
        return build;
    }
    for (const QString folder : {"FortniteGame", "Engine", "FortniteGame/Content/Paks"})
        if (!QFileInfo(build.root + "/" + folder).isDir())
            build.errors.append("Missing folder: " + folder);
    auto profile = Profile::load();
    auto pe = inspectPe(build.root + "/" + profile.executable);
    if (!pe.valid || !pe.x64 || pe.dll)
        build.errors.append("Main executable must be a valid x64 PE application: " + pe.error);
    QDir pakDir(build.root + "/FortniteGame/Content/Paks");
    auto paks = pakDir.entryInfoList({"*.pak"}, QDir::Files);
    if (paks.isEmpty())
        build.errors.append("No .pak content files");
    for (auto &pak : paks) {
        QFile file(pak.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly) || file.size() < 64)
            build.errors.append("Unreadable or truncated content: " + pak.fileName());
    }
    QSet<QString> versions, engines;
    QStringList metadata{profile.executable, "Engine/Binaries/Win64/CrashReportClient.exe",
                         "Engine/Build/Build.version", "FortniteGame/Build/Build.version"};
    for (const auto &relative : metadata) {
        QFile file(build.root + "/" + relative);
        if (!file.exists())
            continue;
        if (!file.open(QIODevice::ReadOnly)) {
            build.errors.append("Unreadable metadata: " + relative);
            continue;
        }
        if (file.size() > 1024LL * 1024 * 1024) {
            build.errors.append("Metadata file exceeds 1 GiB scan limit: " + relative);
            continue;
        }
        QByteArray tail;
        while (!file.atEnd()) {
            auto block = file.read(1024 * 1024);
            if (block.isEmpty() && file.error() != QFile::NoError) {
                build.errors.append("Metadata read failed: " + relative);
                break;
            }
            auto data = tail + block;
            extract(data, relative, build, versions, engines);
            tail = data.right(256);
        }
    }
    if (versions.size() == 1)
        build.version = *versions.begin();
    else
        build.errors.append(versions.isEmpty()
                                ? "Version unknown: no embedded Fortnite release metadata"
                                : "Conflicting Fortnite release metadata");
    if (engines.size() == 1)
        build.engine = *engines.begin();
    else if (engines.size() > 1)
        build.errors.append("Conflicting engine metadata");
    build.warnings.append(
        "Structural validation is not authenticity or full content-integrity verification");
    if (build.engine.isEmpty())
        build.warnings.append("Engine version not detected");
    return build;
}
QJsonObject Build::json() const {
    auto profile = Profile::load();
    return {{"path", root},
            {"version", version.isEmpty() ? "unknown" : version},
            {"engine", engine.isEmpty() ? "unknown" : engine},
            {"valid", valid()},
            {"compatibility_profile", version == profile.version ? profile.version : "unsupported"},
            {"linux_status", "experimental-unverified"},
            {"reboot", version == profile.version ? profile.rebootStatus : "unverified"},
            {"evidence", QJsonArray::fromStringList(evidence)},
            {"errors", QJsonArray::fromStringList(errors)},
            {"warnings", QJsonArray::fromStringList(warnings)}};
}
QStringList discoverBuilds(const QStringList &roots, int maxDepth) {
    QStringList found;
    QList<QPair<QString, int>> pending;
    QSet<QString> visited;
    for (const auto &root : roots)
        pending.append({root, 0});
    int examined = 0;
    while (!pending.isEmpty() && examined++ < 100000) {
        auto [path, depth] = pending.takeLast();
        auto canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty() || visited.contains(canonical))
            continue;
        visited.insert(canonical);
        QDir dir(canonical);
        if (dir.exists("FortniteGame") && dir.exists("Engine")) {
            found.append(canonical);
            continue;
        }
        if (depth >= maxDepth)
            continue;
        for (auto &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
            pending.append({entry.absoluteFilePath(), depth + 1});
    }
    found.sort();
    return found;
}
} // namespace fol
