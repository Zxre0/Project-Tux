#include "fol/core.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QStandardPaths>
#include <QUuid>

namespace fol {
static QString backupRoot(const Paths &paths, const QString &id) {
    // Reuse prefix placement/registration checks before touching backup storage.
    prefixStatus(paths, id);
    auto root = paths.data + "/prefix-backups/" + id;
    QString cursor = root;
    while (cursor != "/" && !cursor.isEmpty()) {
        require(!QFileInfo(cursor).isSymLink(), "Backup path has a symlink component: " + cursor);
        cursor = QFileInfo(cursor).absolutePath();
    }
    return root;
}
static QJsonObject backupMetadata(const Paths &paths, const QString &id, const QString &token) {
    validateId(token);
    auto directory = backupRoot(paths, id) + "/" + token;
    require(!QFileInfo(directory).isSymLink() && !QFileInfo(directory + "/prefix").isSymLink() &&
                !QFileInfo(directory + "/backup.json").isSymLink() &&
                !QFileInfo(directory + "/prefix/.fol-prefix.json").isSymLink(),
            "Refusing symlinked backup metadata or prefix");
    auto metadata = readJson(directory + "/backup.json");
    require(metadata["owner"] == "fortnite-on-linux" && metadata["id"] == id &&
                metadata["original_path"] == paths.prefix(id) && metadata["token"] == token &&
                metadata["complete"].toBool(),
            "Backup ownership or completion metadata does not match");
    auto marker = readJson(directory + "/prefix/.fol-prefix.json");
    require(marker["owner"] == "fortnite-on-linux" && marker["id"] == id &&
                marker["path"] == paths.prefix(id),
            "Backup prefix ownership does not match");
    return metadata;
}
QJsonObject prefixBackups(const Paths &paths, const QString &id) {
    QJsonArray entries;
    auto root = backupRoot(paths, id);
    for (const auto &entry :
         QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        try {
            auto metadata = backupMetadata(paths, id, entry.fileName());
            metadata["path"] = entry.absoluteFilePath();
            entries.append(metadata);
        } catch (const Error &error) {
            entries.append(QJsonObject{{"token", entry.fileName()},
                                       {"complete", false},
                                       {"error", QString::fromUtf8(error.what())}});
        }
    }
    return {{"prefix_id", id}, {"path", root}, {"backups", entries}};
}
QJsonObject backupPrefix(const Paths &paths, const QString &id, bool archive) {
    auto root = backupRoot(paths, id);
    QLockFile lock(paths.prefix(id) + ".lock");
    require(lock.tryLock(), "Prefix operation already in progress");
    auto status = prefixStatus(paths, id);
    require(status["owned"].toBool(), "Refusing to back up an unowned prefix");
    require(status["active_processes"].toInt() == 0, "Stop prefix processes before backup");
    auto token = QDateTime::currentDateTimeUtc().toString("yyyyMMddTHHmmsszzz") + "-" +
                 QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto directory = root + "/" + token;
    require(!QFileInfo::exists(directory) && QDir().mkpath(directory),
            "Cannot create backup directory");
    require(
        QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
        "Cannot restrict backup permissions");
    QJsonObject metadata{
        {"owner", "fortnite-on-linux"},
        {"id", id},
        {"token", token},
        {"original_path", paths.prefix(id)},
        {"path", directory},
        {"mode", archive ? "archive" : "copy"},
        {"complete", false},
        {"created_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    writeFile(directory + "/backup.json", QJsonDocument(metadata).toJson());
    if (archive) {
        // Same-filesystem rename preserves everything, including Wine drive links.
        // Failure leaves the active prefix untouched; no recursive delete is used.
        require(QDir().rename(paths.prefix(id), directory + "/prefix"),
                "Cannot archive prefix; original preserved; backup=" + directory);
    } else {
        auto cp = QStandardPaths::findExecutable("cp");
        require(!cp.isEmpty(), "GNU coreutils cp is required for prefix backups");
        // -a preserves links without traversing dosdevices/z: into the host root.
        // Reflink is opportunistic; ordinary copies work on other filesystems.
        auto outcome =
            run(cp, {"--archive", "--reflink=auto", "--", paths.prefix(id), directory + "/prefix"},
                300000);
        writeFile(directory + "/copy.log", outcome.output.toUtf8());
        require(!outcome.timedOut && outcome.exitCode == 0,
                "Prefix backup incomplete; source preserved; inspect " + directory);
    }
    metadata["complete"] = true;
    writeFile(directory + "/backup.json", QJsonDocument(metadata).toJson());
    return metadata;
}
QJsonObject restorePrefix(const Paths &paths, const QString &id, const QString &token) {
    auto metadata = backupMetadata(paths, id, token);
    require(QDir().mkpath(paths.data + "/prefixes"), "Cannot create prefixes directory");
    QLockFile lock(paths.prefix(id) + ".lock");
    require(lock.tryLock(), "Prefix operation already in progress");
    require(!QFileInfo::exists(paths.prefix(id)) && !QFileInfo(paths.prefix(id)).isSymLink(),
            "Restore never overwrites an existing prefix; archive it first");
    require(ownedProcesses(paths.prefix(id), {}).isEmpty(), "Prefix has active processes");
    auto directory = backupRoot(paths, id) + "/" + token;
    require(QDir().rename(directory + "/prefix", paths.prefix(id)),
            "Cannot restore prefix; backup preserved at " + directory);
    metadata["complete"] = false;
    metadata["restored_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    writeFile(directory + "/backup.json", QJsonDocument(metadata).toJson());
    auto status = prefixStatus(paths, id);
    status["restored_from"] = token;
    status["backup_consumed"] = true;
    return status;
}
QJsonObject resetPrefix(const Paths &paths, const QString &id) {
    auto archived = backupPrefix(paths, id, true);
    try {
        auto status = createPrefix(paths, id);
        status["previous_prefix_backup"] = archived;
        return status;
    } catch (const Error &error) {
        throw Error("Reset initialization failed; original prefix preserved at " +
                    archived["path"].toString() + "; " + QString::fromUtf8(error.what()));
    }
}
} // namespace fol
