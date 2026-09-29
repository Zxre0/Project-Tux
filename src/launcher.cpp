#include "fol/core.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

namespace fol {
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
    // Presence of a graphics library elsewhere in a Wine log is not evidence
    // that an unrelated service error originated there. Require same-line
    // component and severity evidence; these remain hints, not root causes.
    static const QRegularExpression severity(R"((?:^|[\s:\]])(?:err|error|fatal)(?:[\s:]|$))");
    static const QRegularExpression vkd3d(R"(\bvkd3d(?:-proton)?\b)");
    static const QRegularExpression dxvk(R"(\bdxvk\b)");
    for (const auto &line : lower.split('\n')) {
        if (!severity.match(line).hasMatch())
            continue;
        if (vkd3d.match(line).hasMatch())
            return "vkd3d";
        if (dxvk.match(line).hasMatch())
            return "dxvk";
    }
    return "unknown; inspect logs";
}
QJsonObject launch(const Paths &paths, const QString &id, bool printOnly, bool offlineProbe,
                   int timeoutSeconds, bool traceStartup, bool engineStdout) {
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
                       {"startup_trace", traceStartup},
                       {"engine_stdout_requested", engineStdout},
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
        // Logging-only experiment, kept separate from the unchanged baseline.
        // A shipping build may ignore these; requesting output does not prove it
        // was enabled or that startup reached engine logging initialization.
        if (engineStdout)
            args.append({"-stdout", "-FullStdOutLogOutput", "-forcelogflush"});
        QStringList shellArgs;
        for (const auto &arg : args)
            shellArgs.append(quote(arg));
        QMap<QString, QString> overrides{{"WINEPREFIX", paths.prefix(id)},
                                         {"FOL_SESSION_ID", session},
                                         {"WINEDEBUG", "+timestamp,+pid,+tid,+seh,+loaddll"},
                                         {"DXVK_LOG_PATH", directory},
                                         {"VKD3D_LOG_FILE", directory + "/vkd3d.log"},
                                         {"WINEDLLOVERRIDES", "winemenubuilder.exe=d"}};
        if (traceStartup)
            overrides["WINEDEBUG"] += ",+msgbox,+debugstr,+process,+exec";
        // A bundled Wine may require its own DLL/library search directories.
        // Preserve and record these specific inherited settings in the command;
        // do not dump the entire environment, which may contain credentials.
        QJsonObject inheritedRuntime;
        const auto inherited = QProcessEnvironment::systemEnvironment();
        for (const QString key : {"WINEDLLPATH", "LD_LIBRARY_PATH", "WINEARCH"}) {
            if (inherited.contains(key)) {
                overrides[key] = inherited.value(key);
                inheritedRuntime[key] = inherited.value(key);
            }
        }
        result["inherited_runtime_environment"] = inheritedRuntime;
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
            const auto outcome =
                superviseRuntime(unshare, args, env, directory, "wine.log", timeoutSeconds * 1000,
                                 {build.root + "/" + profile.workingDirectory, true});
            result["exit_code"] = outcome.exitCode;
            result["owned_processes_remaining"] = outcome.remaining;
            result["crashed"] = outcome.crashed;
            result["status"] = !outcome.started ? "failed-to-start"
                               : outcome.eac    ? "stopped-eac-detected"
                               : outcome.battleye || !outcome.moduleEvidence.isEmpty()
                                   ? "stopped-anticheat-detected"
                               : !outcome.traceReadable   ? "stopped-log-unreadable"
                               : !outcome.error.isEmpty() ? "stopped-monitor-error"
                               : outcome.remaining        ? "cleanup-incomplete"
                               : outcome.timedOut         ? "probe-timeout"
                               : outcome.interrupted      ? "stopped"
                                                          : "exited; gameplay-unverified";
            result["eac_status"] =
                outcome.eac       ? "detected; stop requested; inspect cleanup result"
                : outcome.started ? "not observed during sampled monitoring; not proof of absence"
                                  : "not tested";
            result["battleye_process_observed"] = outcome.battleye;
            if (!outcome.moduleEvidence.isEmpty()) {
                result["anti_cheat_module_evidence"] = outcome.moduleEvidence;
                if (outcome.moduleEvidence.contains("EasyAntiCheat", Qt::CaseInsensitive))
                    result["eac_status"] =
                        "module load observed; stop requested; inspect cleanup result";
            }
            if (!outcome.error.isEmpty())
                result["error"] = outcome.error;
            else if (outcome.remaining)
                result["error"] = "Owned processes remain after scoped cleanup";
            QFile log(directory + "/wine.log");
            if (log.open(QIODevice::ReadOnly)) {
                log.seek(qMax(qint64(0), log.size() - 1024 * 1024));
                result["likely_subsystem"] = classifyFailure(QString::fromUtf8(log.readAll()));
            }
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
