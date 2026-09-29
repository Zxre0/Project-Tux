#include "fol/core.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <csignal>

namespace fol {
static volatile std::sig_atomic_t interrupted = 0;
static void interrupt(int) { interrupted = 1; }
QString classifyFailure(const QString &text) {
    auto lower = text.toLower();
    if (lower.contains("c0000135") || lower.contains("import_dll") ||
        lower.contains("module could not be found"))
        return "missing-library";
    if (lower.contains("c0000005") || lower.contains("access violation"))
        return "access-violation";
    if (lower.contains("segmentation fault"))
        return "segmentation-fault";
    if (lower.contains("vk_error") || lower.contains("failed to create vulkan"))
        return "vulkan";
    if (lower.contains("vkd3d") && lower.contains("err"))
        return "vkd3d";
    if (lower.contains("dxvk") && lower.contains("err"))
        return "dxvk";
    return "unknown; inspect logs";
}
QJsonObject launch(const Paths &paths, const QString &id, bool printOnly, bool offlineProbe,
                   int timeoutSeconds) {
    require(timeoutSeconds >= 1 && timeoutSeconds <= 3600, "Probe timeout must be 1-3600 seconds");
    auto session = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd_HH-mm-ss") + "_" +
                   QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    auto directory = paths.state + "/logs/sessions/" + session;
    require(QDir().mkpath(directory), "Cannot create session directory");
    QJsonObject result{{"session_id", session},
                       {"logs", directory},
                       {"component", "launcher"},
                       {"launch_profile", "Private Development"},
                       {"eac_status", "not tested; no game started"},
                       {"status", "blocked"},
                       {"launcher_pid", QCoreApplication::applicationPid()}};
    try {
        auto build = inspectBuild(Registry(paths).resolve(id));
        result["build"] = build.json();
        require(build.valid(), "Installation validation failed: " + build.errors.join("; "));
        auto selectedProfile = Profile::forVersion(build.version);
        require(selectedProfile.has_value(), "Unsupported build: " + build.version);
        auto profile = *selectedProfile;
        require(offlineProbe || profile.privatePathVerified,
                "Private-development execution path is unverified. Only --offline-probe is "
                "available; it has no network access or authentication modifications.");
        auto prefix = prefixStatus(paths, id);
        result["prefix"] = prefix;
        auto wine = wineExecutable();
        auto unshare = QStandardPaths::findExecutable("unshare");
        require(!unshare.isEmpty(), "util-linux unshare is required for isolated startup probes");
        auto probe = run(unshare, {"--user", "--map-root-user", "--net", "true"});
        require(probe.exitCode == 0 && !probe.timedOut,
                "Network isolation unavailable: " + probe.output);
        auto executable = build.root + "/" + profile.executable;
        QStringList args{"--user", "--map-root-user", "--net", "--", wine, executable};
        args.append(profile.arguments);
        QStringList shellArgs;
        for (const auto &arg : args)
            shellArgs.append(quote(arg));
        QMap<QString, QString> overrides{{"WINEPREFIX", paths.prefix(id)},
                                         {"FOL_SESSION_ID", session},
                                         {"WINEDEBUG", "+timestamp,+pid,+tid,+seh,+loaddll"},
                                         {"DXVK_LOG_PATH", directory},
                                         {"VKD3D_LOG_FILE", directory + "/vkd3d.log"},
                                         {"WINEDLLOVERRIDES", "winemenubuilder.exe=d"}};
        QStringList assignments;
        for (auto it = overrides.begin(); it != overrides.end(); ++it)
            assignments.append(it.key() + "=" + quote(it.value()));
        QString command = "cd -- " + quote(build.root + "/" + profile.workingDirectory) +
                          " && env " + assignments.join(' ') + " " + quote(unshare) + " " +
                          shellArgs.join(' ') + " >" + quote(directory + "/wine.log") + " 2>&1";
        result["command"] = command;
        result["runtime"] = wine;
        result["process"] = executable;
        writeFile(directory + "/launch-command.txt", command.toUtf8() + '\n');
        if (printOnly) {
            result["status"] = "planned; not executed";
        } else {
            require(prefix["initialized"].toBool(),
                    "Dedicated prefix is not initialized; use prefix create " + id);
            QLockFile lock(paths.prefix(id) + ".lock");
            require(lock.tryLock(), "Prefix is in use by another launcher operation");
            require(ownedProcesses(paths.prefix(id), {}).isEmpty(),
                    "Dedicated prefix has active Wine processes");
            auto report = systemReport();
            writeFile(directory + "/system.json", QJsonDocument(report).toJson());
            require(report["vulkan"].toObject()["functional"].toBool(),
                    "Vulkan device enumeration failed; see system.json");
            auto env = QProcessEnvironment::systemEnvironment();
            for (auto it = overrides.begin(); it != overrides.end(); ++it)
                env.insert(it.key(), it.value());
            QProcess process;
            process.setProcessEnvironment(env);
            process.setWorkingDirectory(build.root + "/" + profile.workingDirectory);
            process.setStandardOutputFile(directory + "/wine.log");
            process.setProcessChannelMode(QProcess::MergedChannels);
            QFile snapshots(directory + "/processes.log");
            require(snapshots.open(QIODevice::WriteOnly), "Cannot open process log");
            interrupted = 0;
            auto oldInt = std::signal(SIGINT, interrupt);
            auto oldTerm = std::signal(SIGTERM, interrupt);
            process.start(unshare, args);
            bool started = process.waitForStarted(5000);
            bool eac = false, battleye = false, timedOut = false;
            QFile moduleTrace(directory + "/wine.log");
            bool traceReadable = moduleTrace.open(QIODevice::ReadOnly);
            QByteArray traceTail;
            QString moduleEvidence;
            QElapsedTimer elapsed;
            elapsed.start();
            QList<Process> observed;
            while (started) {
                auto all = processes();
                auto roots = ownedProcesses(paths.prefix(id), session);
                if (process.state() != QProcess::NotRunning && process.processId() > 1)
                    for (const auto &item : all)
                        if (item.pid == process.processId())
                            roots.append(item);
                QJsonArray snapshot;
                for (const auto &item : selectProcessTree(all, roots)) {
                    observed.append(item);
                    eac |= item.eac;
                    battleye |= item.battleye;
                    snapshot.append(item.json());
                }
                snapshots.write(QJsonDocument(QJsonObject{{"timestamp",
                                                           QDateTime::currentDateTimeUtc().toString(
                                                               Qt::ISODateWithMs)},
                                                          {"processes", snapshot}})
                                    .toJson(QJsonDocument::Compact) +
                                '\n');
                snapshots.flush();
                if (traceReadable) {
                    auto data = traceTail + moduleTrace.read(1024 * 1024);
                    moduleEvidence = antiCheatModuleEvidence(data);
                    traceTail = data.right(4096);
                    if (moduleTrace.error() != QFile::NoError)
                        traceReadable = false;
                }
                timedOut = elapsed.elapsed() >= timeoutSeconds * 1000LL;
                if (eac || battleye || !moduleEvidence.isEmpty() || !traceReadable || timedOut ||
                    interrupted)
                    break;
                if (process.state() == QProcess::NotRunning)
                    QThread::msleep(100);
                else
                    process.waitForFinished(100);
                if (process.state() == QProcess::NotRunning &&
                    ownedProcesses(paths.prefix(id), session).isEmpty() && moduleTrace.atEnd())
                    break;
            }
            // pidfds plus start-time checks prevent killing a process that reused an observed PID.
            for (const auto &item : ownedProcesses(paths.prefix(id), session))
                observed.append(item);
            for (const auto &item : observed)
                signalProcess(item, SIGTERM);
            if (process.state() != QProcess::NotRunning) {
                process.terminate();
                process.waitForFinished(2000);
            }
            for (const auto &item : observed)
                signalProcess(item, SIGKILL);
            if (process.state() != QProcess::NotRunning) {
                process.kill();
                process.waitForFinished(2000);
            }
            std::signal(SIGINT, oldInt);
            std::signal(SIGTERM, oldTerm);
            result["exit_code"] = started ? process.exitCode() : -1;
            result["status"] = !started ? "failed-to-start"
                               : eac    ? "stopped-eac-detected"
                               : battleye || !moduleEvidence.isEmpty()
                                   ? "stopped-anticheat-detected"
                               : !traceReadable ? "stopped-log-unreadable"
                               : timedOut       ? "probe-timeout"
                               : interrupted    ? "stopped"
                                                : "exited; gameplay-unverified";
            result["eac_status"] =
                eac       ? "detected; launch terminated"
                : started ? "not observed during sampled monitoring; not proof of absence"
                          : "not tested";
            result["battleye_process_observed"] = battleye;
            if (!moduleEvidence.isEmpty()) {
                result["anti_cheat_module_evidence"] = moduleEvidence;
                if (moduleEvidence.contains("EasyAntiCheat", Qt::CaseInsensitive))
                    result["eac_status"] = "module load observed; launch terminated";
            }
            if (!traceReadable)
                result["error"] = "Wine trace unreadable; probe stopped";
            QFile log(directory + "/wine.log");
            if (log.open(QIODevice::ReadOnly)) {
                log.seek(qMax(qint64(0), log.size() - 1024 * 1024));
                result["likely_subsystem"] = classifyFailure(QString::fromUtf8(log.readAll()));
            }
            if (!started)
                result["error"] = process.errorString();
        }
    } catch (const std::exception &error) {
        result["error"] = QString::fromUtf8(error.what());
    }
    writeFile(directory + "/result.json", QJsonDocument(result).toJson());
    writeFile(directory + "/launcher.log",
              QJsonDocument(QJsonObject{{"timestamp", QDateTime::currentDateTimeUtc().toString(
                                                          Qt::ISODateWithMs)},
                                        {"level", result.contains("error") ? "ERROR" : "INFO"},
                                        {"component", "launcher"},
                                        {"session_id", session},
                                        {"pid", QCoreApplication::applicationPid()},
                                        {"result", result}})
                      .toJson(QJsonDocument::Compact) +
                  '\n');
    return result;
}
} // namespace fol
