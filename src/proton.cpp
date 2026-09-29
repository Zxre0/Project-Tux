#include "fol/core.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QStandardPaths>
#include <QUuid>

namespace fol {
static void checkStorage(const Paths &paths, const QString &path) {
    require(QDir::isAbsolutePath(path) && path.startsWith(paths.data + "/"),
            "Proton storage must be under project XDG data");
    for (auto cursor = path; cursor != "/" && !cursor.isEmpty();
         cursor = QFileInfo(cursor).absolutePath()) {
        require(!QFileInfo(cursor).isSymLink(), "Proton storage has symlink component: " + cursor);
        require(!(QDir(cursor + "/FortniteGame").exists() && QDir(cursor + "/Engine").exists()),
                "Proton storage must not be inside a game installation");
    }
    for (const auto &build : Registry(paths).list()) {
        auto game = QFileInfo(build).canonicalFilePath();
        require(game.isEmpty() || (path != game && !path.startsWith(game + "/")),
                "Proton storage must not be inside a registered game installation");
    }
}
static QByteArray runtimeDigest(const QString &root) {
    return QCryptographicHash::hash(readFile(root + "/proton", 4 * 1024 * 1024) +
                                        readFile(root + "/version", 4096),
                                    QCryptographicHash::Sha256)
        .toHex();
}
static void checkRuntime(const QString &root) {
    require(!QFileInfo::exists(root + "/user_settings.py") &&
                !QFileInfo(root + "/user_settings.py").isSymLink(),
            "Custom Proton user_settings.py is not supported by the reproducible smoke test");
    for (const QString file : {"proton", "files/bin/wine", "files/bin/wineserver"})
        require(QFileInfo(root + "/" + file).isFile() &&
                    QFileInfo(root + "/" + file).isExecutable(),
                "Unsupported Proton layout: missing executable " + root + "/" + file);
    require(QDir(root + "/files/share/default_pfx").exists(),
            "Proton default prefix missing; complete runtime installation first");
}
QJsonObject importProton(const Paths &paths, const QString &id, const QString &source) {
    validateId(id);
    auto target = paths.data + "/runtimes/" + id;
    checkStorage(paths, target);
    auto root = QFileInfo(source).canonicalFilePath();
    require(!root.isEmpty() && QDir(root).exists(), "Proton source directory does not exist");
    require(target != root && !target.startsWith(root + "/"),
            "Runtime destination must not be inside its source");
    checkRuntime(root);
    require(QDir().mkpath(paths.data + "/runtimes"), "Cannot create runtimes directory");
    QLockFile lock(target + ".lock");
    require(lock.tryLock(), "Runtime import already in progress");
    require(!QFileInfo::exists(target), "Runtime ID already exists; use a new ID for updates");
    auto cp = QStandardPaths::findExecutable("cp");
    require(!cp.isEmpty(), "GNU coreutils cp required for runtime import");
    auto staging = target + ".partial-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto copied = run(cp, {"--archive", "--reflink=auto", "--", root, staging}, 300000);
    require(!copied.timedOut && copied.exitCode == 0,
            "Runtime import incomplete; source unchanged; retained " + staging + ": " +
                copied.output);
    checkRuntime(staging);
    require(QFile::setPermissions(staging, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
            "Cannot restrict runtime directory permissions");
    QJsonObject metadata{
        {"owner", "fortnite-on-linux"},
        {"id", id},
        {"path", target},
        {"source", root},
        {"version", QString::fromUtf8(readFile(staging + "/version", 4096)).trimmed()},
        {"script_version_sha256", QString::fromLatin1(runtimeDigest(staging))}};
    writeFile(staging + "/.fol-runtime.json", QJsonDocument(metadata).toJson());
    require(QDir().rename(staging, target), "Cannot publish runtime import; retained " + staging);
    return metadata;
}
QJsonArray importedProtonRuntimes(const Paths &paths) {
    auto root = paths.data + "/runtimes";
    checkStorage(paths, root);
    QJsonArray entries;
    for (const auto &entry :
         QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QJsonObject info{{"id", entry.fileName()},
                         {"path", entry.absoluteFilePath()},
                         {"type", "proton"},
                         {"game_launch_supported", false}};
        try {
            validateId(entry.fileName());
            checkStorage(paths, entry.absoluteFilePath());
            checkRuntime(entry.absoluteFilePath());
            auto metadata = readJson(entry.absoluteFilePath() + "/.fol-runtime.json");
            require(metadata["owner"] == "fortnite-on-linux" &&
                        metadata["id"] == entry.fileName() &&
                        metadata["path"] == entry.absoluteFilePath() &&
                        metadata["script_version_sha256"] ==
                            QString::fromLatin1(runtimeDigest(entry.absoluteFilePath())),
                    "Runtime ownership or script/version digest changed");
            info["version"] = metadata["version"];
            info["status"] = "imported; smoke test available";
        } catch (const std::exception &error) {
            info["status"] = "invalid or incomplete";
            info["error"] = QString::fromUtf8(error.what());
        }
        entries.append(info);
    }
    return entries;
}
QJsonObject testProton(const Paths &paths, const QString &id, const QString &runtimeId) {
    validateId(id);
    validateId(runtimeId);
    auto session = "proton-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto logs = paths.state + "/logs/sessions/" + session;
    QJsonObject result{{"session_id", session},
                       {"component", "runtime"},
                       {"logs", logs},
                       {"passed", false},
                       {"fortnite_tested", false}};
    try {
        auto runtime = paths.data + "/runtimes/" + runtimeId;
        auto compat = paths.data + "/proton-prefixes/" + id;
        auto prefix = compat + "/pfx";
        checkStorage(paths, runtime);
        checkStorage(paths, prefix);
        checkRuntime(runtime);
        require(!QFileInfo(runtime + "/.fol-runtime.json").isSymLink(), "Symlinked runtime marker");
        auto marker = readJson(runtime + "/.fol-runtime.json");
        require(marker["owner"] == "fortnite-on-linux" && marker["id"] == runtimeId &&
                    marker["path"] == runtime &&
                    marker["script_version_sha256"] == QString::fromLatin1(runtimeDigest(runtime)),
                "Runtime import ownership or script/version digest changed; import a fresh ID");
        auto unshare = QStandardPaths::findExecutable("unshare");
        require(!unshare.isEmpty(), "util-linux unshare required for offline Proton tests");
        auto probe = run(unshare, {"--user", "--map-root-user", "--net", "true"});
        require(!probe.timedOut && probe.exitCode == 0,
                "Network isolation unavailable: " + probe.output);
        require(QDir().mkpath(paths.data + "/proton-prefixes"), "Cannot create Proton storage");
        QLockFile prefixLock(compat + ".lock"), runtimeLock(runtime + ".lock");
        require(prefixLock.tryLock() && runtimeLock.tryLock(),
                "Proton prefix or runtime is in use");
        require(ownedProcesses(prefix, {}).isEmpty(), "Proton prefix has active processes");
        if (QFileInfo::exists(compat)) {
            require(!QFileInfo(compat + "/.fol-proton.json").isSymLink(),
                    "Symlinked Proton marker");
            auto owner = readJson(compat + "/.fol-proton.json");
            require(owner["owner"] == "fortnite-on-linux" && owner["id"] == id &&
                        owner["path"] == compat && owner["runtime"] == runtimeId,
                    "Refusing unowned or different-runtime Proton prefix");
        } else {
            writeFile(compat + "/.fol-proton.json",
                      QJsonDocument(QJsonObject{{"owner", "fortnite-on-linux"},
                                                {"id", id},
                                                {"path", compat},
                                                {"runtime", runtimeId}})
                          .toJson());
            require(QFile::setPermissions(compat,
                                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
                    "Cannot restrict Proton prefix permissions");
        }
        require(QDir().mkpath(logs) && QDir().mkpath(compat + "/empty-steam-client"),
                "Cannot create Proton log/client directories");
        // Do not read the user's Steam identity/configuration for a built-in cmd test.
        QMap<QString, QString> managed{
            {"STEAM_COMPAT_DATA_PATH", compat},
            {"STEAM_COMPAT_CLIENT_INSTALL_PATH", compat + "/empty-steam-client"},
            {"WINEPREFIX", prefix},
            {"FOL_SESSION_ID", session},
            {"SteamAppId", "0"},
            {"SteamGameId", "0"},
            {"PROTON_LOG", "1"},
            {"PROTON_LOG_DIR", logs},
            {"WINEDEBUG", "+timestamp,+pid,+seh,+loaddll"},
            {"WINEDLLOVERRIDES", "winemenubuilder.exe=d"}};
        auto env = QProcessEnvironment::systemEnvironment();
        QStringList unsetArgs;
        // Steam's media paths may trigger file migration/removal in Proton.
        // Do not inherit other games' runtime, media, injection or debug settings.
        for (const auto &key : env.keys()) {
            if (key.startsWith("STEAM_") || key.startsWith("Steam") || key.startsWith("PROTON_") ||
                key.startsWith("WINE") || key.startsWith("MEDIACONV_")) {
                env.remove(key);
                unsetArgs.append("-u " + quote(key));
            }
        }
        QStringList assignments;
        for (auto it = managed.begin(); it != managed.end(); ++it) {
            env.insert(it.key(), it.value());
            assignments.append(it.key() + "=" + quote(it.value()));
        }
        // A fresh receipt verifies execution even when Proton's Steam shim does
        // not relay console output. Session is launcher-generated, never user text.
        auto receiptName = session + ".txt";
        auto receipt = prefix + "/drive_c/" + receiptName;
        QString cmd = "echo " + session + ">C:\\" + receiptName;
        QString worker = "\"$1\" run cmd.exe /d /c \"$3\"; fol_cmd_status=$?; "
                         "\"$2\" --wait; fol_wait_status=$?; "
                         "if [ \"$fol_cmd_status\" -ne 0 ]; then exit \"$fol_cmd_status\"; fi; "
                         "exit \"$fol_wait_status\"";
        QStringList args{"--user",
                         "--map-root-user",
                         "--net",
                         "--",
                         "/bin/sh",
                         "-c",
                         worker,
                         "fol-proton-test",
                         runtime + "/proton",
                         runtime + "/files/bin/wineserver",
                         cmd};
        QStringList quoted;
        for (const auto &arg : args)
            quoted.append(quote(arg));
        auto command = "env " + unsetArgs.join(' ') + " " + assignments.join(' ') + " " +
                       quote(unshare) + " " + quoted.join(' ');
        writeFile(logs + "/launch-command.txt", command.toUtf8() + '\n');
        auto outcome = superviseRuntime(unshare, args, env, logs, "runtime.log", 90000);
        bool receiptOk = QFileInfo(receipt).isFile() && !QFileInfo(receipt).isSymLink() &&
                         QString::fromUtf8(readFile(receipt, 4096)).trimmed() == session;
        bool passed = outcome.started && !outcome.crashed && outcome.exitCode == 0 && receiptOk &&
                      !outcome.remaining && !outcome.eac && !outcome.timedOut &&
                      !outcome.interrupted && outcome.error.isEmpty();
        result["passed"] = passed;
        result["runtime"] = runtime;
        result["version"] = marker["version"];
        result["prefix"] = prefix;
        result["exit_code"] = outcome.exitCode;
        result["timed_out"] = outcome.timedOut;
        result["interrupted"] = outcome.interrupted;
        result["execution_receipt_verified"] = receiptOk;
        result["owned_processes_remaining"] = outcome.remaining;
        result["eac_status"] =
            outcome.eac ? "detected; terminated"
                        : "not observed in sampled process monitoring; not proof of absence";
        result["test"] = "network-isolated Proton run cmd.exe execution receipt and server drain";
        if (!outcome.error.isEmpty())
            result["error"] = outcome.error;
    } catch (const std::exception &error) {
        result["error"] = QString::fromUtf8(error.what());
    }
    writeFile(logs + "/result.json", QJsonDocument(result).toJson());
    writeFile(logs + "/launcher.log",
              QJsonDocument(QJsonObject{{"timestamp", QDateTime::currentDateTimeUtc().toString(
                                                          Qt::ISODateWithMs)},
                                        {"component", "runtime"},
                                        {"session_id", session},
                                        {"level", result["passed"].toBool() ? "INFO" : "ERROR"},
                                        {"result", result}})
                      .toJson(QJsonDocument::Compact) +
                  '\n');
    return result;
}
} // namespace fol
