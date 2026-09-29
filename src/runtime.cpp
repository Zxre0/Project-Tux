#include "fol/core.h"
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>

namespace fol {
QString wineExecutable() {
    auto wine = QStandardPaths::findExecutable("wine");
    require(!wine.isEmpty(),
            "Wine not found in PATH; install a distribution-supported Wine package");
    return wine;
}
QJsonArray runtimes() {
    QJsonArray result;
    for (const QString name : {"wine", "wine64", "umu-run", "gamescope", "gamemoderun"}) {
        auto path = QStandardPaths::findExecutable(name);
        QJsonObject entry{{"name", name}, {"path", path}, {"available", !path.isEmpty()}};
        if (!path.isEmpty() && name.startsWith("wine")) {
            auto version = run(path, {"--version"});
            entry["version"] = version.output.trimmed();
            entry["probe_exit_code"] = version.exitCode;
        }
        result.append(entry);
    }
    QStringList roots{QDir::homePath() + "/.local/share/Steam/steamapps/common",
                      QDir::homePath() + "/.steam/root/steamapps/common",
                      QDir::homePath() + "/.steam/root/compatibilitytools.d",
                      QDir::homePath() + "/.local/share/Steam/compatibilitytools.d"};
    QSet<QString> seen;
    for (const auto &root : roots) {
        for (const auto &dir : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            auto proton = QFileInfo(dir.absoluteFilePath() + "/proton");
            if (!proton.isExecutable() || seen.contains(proton.canonicalFilePath()))
                continue;
            seen.insert(proton.canonicalFilePath());
            QJsonObject entry{{"name", dir.fileName()},
                              {"path", proton.canonicalFilePath()},
                              {"available", true},
                              {"type", "proton"},
                              {"launch_supported", false}};
            if (QFile::exists(dir.absoluteFilePath() + "/version"))
                entry["version"] =
                    QString::fromUtf8(readFile(dir.absoluteFilePath() + "/version", 4096))
                        .trimmed();
            result.append(entry);
        }
    }
    return result;
}
static void verifyPrefixLocation(const Paths &paths, const QString &id) {
    const auto prefix = paths.prefix(id);
    // Do not traverse symlinked project storage when creating or operating on a prefix.
    QString cursor = prefix;
    while (cursor != "/" && !cursor.isEmpty()) {
        require(!QFileInfo(cursor).isSymLink(), "Prefix path has a symlink component: " + cursor);
        require(!(QDir(cursor + "/FortniteGame").exists() && QDir(cursor + "/Engine").exists()),
                "Prefix must not be inside a Fortnite installation");
        cursor = QFileInfo(cursor).absolutePath();
    }
    for (const auto &build : Registry(paths).list()) {
        QString game = QFileInfo(build).canonicalFilePath();
        require(game.isEmpty() || (prefix != game && !prefix.startsWith(game + "/")),
                "Prefix must not be inside a registered game installation");
    }
}
QJsonObject prefixStatus(const Paths &paths, const QString &id) {
    verifyPrefixLocation(paths, id);
    const auto prefix = paths.prefix(id);
    bool owned = false;
    if (QFile::exists(prefix + "/.fol-prefix.json")) {
        auto marker = readJson(prefix + "/.fol-prefix.json");
        owned = marker["owner"] == "fortnite-on-linux" && marker["id"] == id &&
                marker["path"] == prefix;
    }
    QJsonArray components;
    if (owned) {
        for (const QString name :
             {"dxgi.dll", "d3d11.dll", "d3d12.dll", "vcruntime140.dll", "msvcp140.dll"}) {
            auto path = prefix + "/drive_c/windows/system32/" + name;
            auto pe = inspectPe(path);
            components.append(QJsonObject{
                {"name", name},
                {"present", QFileInfo::exists(path)},
                {"valid_x64_pe", pe.valid && pe.x64 && pe.dll},
                {"provider", "unverified; presence does not establish native VC++/DXVK/VKD3D"}});
        }
    }
    return {{"path", prefix},
            {"owned", owned},
            {"initialized", owned && QFile::exists(prefix + "/system.reg") &&
                                QDir(prefix + "/drive_c/windows").exists()},
            {"active_processes", ownedProcesses(prefix, {}).size()},
            {"components", components},
            {"dxvk", "unverified"},
            {"vkd3d", "unverified"},
            {"vcruntime", "unverified"}};
}
QJsonObject createPrefix(const Paths &paths, const QString &id) {
    verifyPrefixLocation(paths, id);
    QString prefix = paths.prefix(id);
    require(QDir().mkpath(paths.data + "/prefixes"), "Cannot create prefixes directory");
    QLockFile lock(prefix + ".lock");
    require(lock.tryLock(), "Prefix operation already in progress");
    auto status = prefixStatus(paths, id);
    require(status["active_processes"].toInt() == 0, "Prefix has active processes");
    if (status["initialized"].toBool())
        return status;
    require(!QFileInfo::exists(prefix) || status["owned"].toBool(),
            "Refusing to adopt an unowned prefix directory");
    auto wine = wineExecutable();
    require(QDir().mkpath(prefix), "Cannot create prefix");
    writeFile(
        prefix + "/.fol-prefix.json",
        QJsonDocument(QJsonObject{{"owner", "fortnite-on-linux"}, {"id", id}, {"path", prefix}})
            .toJson());
    QString session = "prefix-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", prefix);
    env.insert("WINEARCH", "win64");
    env.insert("WINEDLLOVERRIDES", "winemenubuilder.exe=d");
    env.insert("FOL_SESSION_ID", session);
    auto outcome = run(wine, {"wineboot", "--init"}, 60000, env);
    // wineboot can exit before wineserver flushes system.reg. Wait for the same
    // prefix's server; killing it here races initialization and loses evidence.
    auto server = QFileInfo(wine).absolutePath() + "/wineserver";
    auto drained = run(server, {"--wait"}, 30000, env);
    outcome.output += "\n[wineserver --wait]\n" + drained.output;
    auto log = paths.state + "/logs/" + session + ".log";
    writeFile(log, outcome.output.toUtf8());
    if (outcome.timedOut || drained.timedOut || drained.exitCode != 0) {
        for (auto &process : ownedProcesses(prefix, session))
            signalProcess(process, 15);
    }
    require(!outcome.timedOut && outcome.exitCode == 0,
            "Prefix initialization failed; exit=" + QString::number(outcome.exitCode) +
                "; log=" + log);
    require(!drained.timedOut && drained.exitCode == 0,
            "Prefix services did not finish; log=" + log);
    status = prefixStatus(paths, id);
    require(status["initialized"].toBool(), "Wine exited without initializing prefix; log=" + log);
    status["log"] = log;
    status["runtime"] = wine;
    return status;
}
QJsonObject stopPrefix(const Paths &paths, const QString &id) {
    auto status = prefixStatus(paths, id);
    require(status["owned"].toBool(), "Refusing to stop processes for an unowned prefix");
    QLockFile lock(paths.prefix(id) + ".lock");
    require(lock.tryLock(),
            "Prefix is supervised by another launcher; interrupt that launch first");
    auto members = ownedProcesses(paths.prefix(id), {});
    for (const auto &process : members)
        signalProcess(process, 15);
    QElapsedTimer timer;
    timer.start();
    while (!ownedProcesses(paths.prefix(id), {}).isEmpty() && timer.elapsed() < 2000)
        QThread::msleep(50);
    for (const auto &process : members)
        signalProcess(process, 9);
    timer.restart();
    while (!ownedProcesses(paths.prefix(id), {}).isEmpty() && timer.elapsed() < 2000)
        QThread::msleep(50);
    status = prefixStatus(paths, id);
    require(status["active_processes"].toInt() == 0,
            "Some owned prefix processes could not be stopped");
    status["stopped_process_count"] = members.size();
    return status;
}
QJsonObject testRuntime(const Paths &paths, const QString &id) {
    auto status = prefixStatus(paths, id);
    require(status["owned"].toBool() && status["initialized"].toBool(),
            "Create an owned prefix before testing the runtime");
    require(status["active_processes"].toInt() == 0, "Prefix has active processes");
    QLockFile lock(paths.prefix(id) + ".lock");
    require(lock.tryLock(), "Prefix operation already in progress");
    auto wine = wineExecutable();
    auto unshare = QStandardPaths::findExecutable("unshare");
    require(!unshare.isEmpty(), "util-linux unshare is required for the offline runtime test");
    auto session = "runtime-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto directory = paths.state + "/logs/sessions/" + session;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", paths.prefix(id));
    env.insert("FOL_SESSION_ID", session);
    env.insert("WINEDLLOVERRIDES", "winemenubuilder.exe=d");
    env.insert("WINEDEBUG", "+timestamp,+pid,+seh");
    auto server = QFileInfo(wine).absolutePath() + "/wineserver";
    // Wine's server socket identity depends on effective UID. Waiting outside
    // this user namespace can target a different server despite the same prefix.
    // The fixed script uses positional parameters; no user text is evaluated.
    QString worker = "\"$1\" cmd.exe /d /c ver; fol_wine_status=$?; "
                     "\"$2\" --wait; fol_wait_status=$?; "
                     "if [ \"$fol_wine_status\" -ne 0 ]; then exit \"$fol_wine_status\"; fi; "
                     "exit \"$fol_wait_status\"";
    QStringList args{"--user", "--map-root-user",  "--net", "--",  "/bin/sh", "-c",
                     worker,   "fol-runtime-test", wine,    server};
    auto outcome = run(unshare, args, 35000, env);
    auto remaining = ownedProcesses(paths.prefix(id), session);
    QJsonArray processLog;
    for (const auto &item : remaining)
        processLog.append(item.json());
    if (outcome.timedOut) {
        for (const auto &item : remaining)
            signalProcess(item, 15);
        QThread::msleep(100);
        for (const auto &item : remaining)
            signalProcess(item, 9);
    }
    QElapsedTimer cleanup;
    cleanup.start();
    while (!ownedProcesses(paths.prefix(id), session).isEmpty() && cleanup.elapsed() < 3000)
        QThread::msleep(50);
    bool ok = !outcome.timedOut && outcome.exitCode == 0 &&
              ownedProcesses(paths.prefix(id), session).isEmpty();
    QJsonObject report{{"session_id", session},
                       {"component", "runtime"},
                       {"runtime", wine},
                       {"prefix", paths.prefix(id)},
                       {"test", "network-isolated cmd.exe /d /c ver"},
                       {"passed", ok},
                       {"exit_code", outcome.exitCode},
                       {"timed_out", outcome.timedOut},
                       {"server_wait_scope", "same user and network namespace as Wine"},
                       {"logs", directory},
                       {"fortnite_tested", false}};
    writeFile(directory + "/wine.log", outcome.output.toUtf8());
    QStringList quotedArgs;
    for (const auto &arg : args)
        quotedArgs.append(quote(arg));
    QString command = "env WINEPREFIX=" + quote(paths.prefix(id)) +
                      " FOL_SESSION_ID=" + quote(session) +
                      " WINEDEBUG=" + quote(env.value("WINEDEBUG")) +
                      " WINEDLLOVERRIDES=" + quote(env.value("WINEDLLOVERRIDES")) + " " +
                      quote(unshare) + " " + quotedArgs.join(' ');
    writeFile(directory + "/launch-command.txt", command.toUtf8() + '\n');
    writeFile(directory + "/processes.log", QJsonDocument(processLog).toJson());
    writeFile(directory + "/result.json", QJsonDocument(report).toJson());
    writeFile(directory + "/launcher.log",
              QJsonDocument(QJsonObject{{"timestamp", QDateTime::currentDateTimeUtc().toString(
                                                          Qt::ISODateWithMs)},
                                        {"level", ok ? "INFO" : "ERROR"},
                                        {"component", "runtime"},
                                        {"session_id", session},
                                        {"result", report}})
                      .toJson(QJsonDocument::Compact) +
                  '\n');
    return report;
}
} // namespace fol
