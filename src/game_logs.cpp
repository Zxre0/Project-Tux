#include "fol/core.h"
#include <QDir>
#include <QFileInfo>

namespace fol {
QJsonObject captureGameLogs(const QString &prefix, const QString &directory,
                            const QDateTime &notBefore) {
    require(notBefore.isValid(), "Game log collection requires a launch timestamp");
    const auto canonicalPrefix = QFileInfo(prefix).canonicalFilePath();
    require(!canonicalPrefix.isEmpty() && canonicalPrefix != "/", "Invalid game log prefix");
    const auto usersPath = prefix + "/drive_c/users";
    QJsonArray files;
    if (!QDir(usersPath).exists())
        return {{"status", "not found"}, {"files", files}};
    const auto canonicalUsers = QFileInfo(usersPath).canonicalFilePath();
    require(canonicalUsers.startsWith(canonicalPrefix + "/"),
            "Game log user directory escapes prefix");
    const auto users =
        QDir(usersPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    require(users.size() <= 64, "Too many prefix users for bounded game log collection");
    int copied = 0;
    for (const auto &user : users) {
        const auto source =
            usersPath + "/" + user + "/AppData/Local/FortniteGame/Saved/Logs/FortniteGame.log";
        const QFileInfo info(source);
        if (!info.exists() && !info.isSymLink())
            continue;
        QJsonObject entry{{"source", source}};
        if (info.isSymLink() || !info.isFile() ||
            !info.canonicalFilePath().startsWith(canonicalUsers + "/")) {
            entry["status"] = "skipped: unsafe path";
        } else if (info.lastModified().toUTC() < notBefore) {
            entry["status"] = "skipped: predates launch";
        } else if (info.size() > 64 * 1024 * 1024) {
            entry["status"] = "skipped: exceeds 64 MiB limit";
        } else {
            const auto name =
                copied == 0 ? QString("fortnite.log") : QString("fortnite-%1.log").arg(copied);
            require(!QFileInfo::exists(directory + "/" + name),
                    "Game log destination already exists");
            const auto data = readFile(source, 64 * 1024 * 1024);
            writeFile(directory + "/" + name, data);
            entry["status"] = "captured";
            entry["destination"] = directory + "/" + name;
            entry["bytes"] = data.size();
            entry["modified_utc"] = info.lastModified().toUTC().toString(Qt::ISODateWithMs);
            ++copied;
        }
        files.append(entry);
    }
    return {{"status", copied ? "captured" : "no current game log captured"},
            {"scope",
             "prefix-local FortniteGame.log files modified since launch; metadata freshness check"},
            {"files", files}};
}
} // namespace fol
