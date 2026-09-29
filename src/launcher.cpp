#include "fol/core.h"
#include <QCoreApplication>
#include <QCryptographicHash>
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
                   int timeoutSeconds, bool traceStartup, bool engineStdout,
                   const QString &protonId, bool launcherContextProbe, BackendProbe backendProbe,
                   const QString &httpProbeHelper) {
    const bool localBackendProbe = backendProbe != BackendProbe::None;
    require(timeoutSeconds >= 1 && timeoutSeconds <= 3600, "Probe timeout must be 1-3600 seconds");
    auto session = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd_HH-mm-ss") + "_" +
                   QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    auto directory = paths.state + "/logs/sessions/" + session;
    require(QDir().mkpath(directory), "Cannot create session directory");
    require(QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                 QFileDevice::ExeOwner),
            "Cannot restrict session directory permissions");
    QJsonObject result{{"session_id", session},
                       {"logs", directory},
                       {"component", "launcher"},
                       {"launch_profile", "Private Development"},
                       {"eac_status", "not tested; no game started"},
                       {"status", "blocked"},
                       {"startup_trace", traceStartup},
                       {"engine_stdout_requested", engineStdout},
                       {"launcher_context_probe", launcherContextProbe},
                       {"local_backend_probe", localBackendProbe},
                       {"backend_probe_method", backendProbe == BackendProbe::ServiceIni
                                                    ? "service-ini"
                                                : localBackendProbe ? "selector"
                                                                    : "none"},
                       {"launcher_pid", QCoreApplication::applicationPid()}};
    try {
        require(
            httpProbeHelper.isEmpty() ||
                (offlineProbe && launcherContextProbe && !localBackendProbe),
            "HTTP interface probe requires isolated launcher context and no backend experiment");
        require(!localBackendProbe || (offlineProbe && launcherContextProbe),
                "Local-backend experiment requires --offline-probe and --launcher-context-probe");
        require(!launcherContextProbe || offlineProbe,
                "Launcher-context experiment requires --offline-probe network isolation");
        auto build = inspectBuild(Registry(paths).resolve(id));
        result["build"] = build.json();
        require(build.valid(), "Installation validation failed: " + build.errors.join("; "));
        auto selectedProfile = Profile::forVersion(build.version);
        require(selectedProfile.has_value(), "Unsupported build: " + build.version);
        auto profile = *selectedProfile;
        QString helper;
        bool requestObserver = false;
        if (!httpProbeHelper.isEmpty()) {
            require(profile.httpInterfaceProbeAllowed,
                    "HTTP interface probe is not enabled for this build");
            helper = QFileInfo(httpProbeHelper).canonicalFilePath();
            requestObserver = QFileInfo(helper).fileName() == "fol-http-observer.exe";
            const auto pe = inspectPe(helper);
            require(pe.valid && pe.x64 && !pe.dll,
                    "HTTP probe helper must be a valid x64 PE executable");
            result["http_interface_probe"] = QJsonObject{
                {"helper", helper},
                {"sha256",
                 QString::fromLatin1(QCryptographicHash::hash(readFile(helper, 8 * 1024 * 1024),
                                                              QCryptographicHash::Sha256)
                                         .toHex())},
                {"scope", "trusted supplied helper; owned-child read-only interface discovery; "
                          "no route or login is established by a candidate"}};
            if (requestObserver) {
                require(!profile.httpObserverHash.isEmpty() &&
                            !profile.httpObserverLayout.isEmpty(),
                        "No request observer is defined for this build");
                QFile game(build.root + "/" + profile.executable);
                QCryptographicHash hash(QCryptographicHash::Sha256);
                require(game.open(QIODevice::ReadOnly) && hash.addData(&game) &&
                            QString::fromLatin1(hash.result().toHex()) == profile.httpObserverHash,
                        "Request observer executable hash mismatch; no debugger started");
                result["request_observer"] = true;
            }
        }
        const auto backendArguments = backendProbe == BackendProbe::ServiceIni
                                          ? profile.serviceIniArguments
                                          : profile.localBackendArguments;
        require(!localBackendProbe || !backendArguments.isEmpty(),
                "No local-backend experiment is defined for this build");
        require(!launcherContextProbe || !profile.launcherContextArguments.isEmpty(),
                "No launcher-context experiment is defined for this build");
        require(offlineProbe || profile.privatePathVerified,
                "Private-development execution path is unverified. Only --offline-probe is "
                "available; it has no network access or authentication modifications.");
        std::optional<ProtonSession> proton;
        QJsonObject prefix;
        QString runtime, prefixPath;
        auto environment = QProcessEnvironment::systemEnvironment();
        require(!environment.contains("FOL_HTTP_OBSERVER_LAYOUT"),
                "Observer layout must come from the checked compatibility profile");
        QMap<QString, QString> overrides;
        QStringList unsetArgs;
        if (protonId.isEmpty()) {
            prefix = prefixStatus(paths, id);
            prefixPath = paths.prefix(id);
            runtime = wineExecutable();
        } else {
            proton.emplace(prepareProtonSession(paths, id, protonId, session, directory, false));
            prefixPath = proton->prefix;
            runtime = proton->runtime + "/proton";
            environment = proton->environment;
            overrides = proton->managed;
            // Proton otherwise redirects Wine into steam-0.log, out of the
            // module monitor's view. Explicit WINEDEBUG still enables traces;
            // PROTON_LOG=0 retains stdout/stderr in the supervised wine.log.
            overrides["PROTON_LOG"] = "0";
            overrides["DXVK_LOG_LEVEL"] = "info";
            overrides["VKD3D_DEBUG"] = "warn";
            for (const auto &key : proton->unsetKeys)
                unsetArgs.append("-u " + quote(key));
            prefix = {{"path", prefixPath}, {"runtime_id", protonId}, {"type", "proton"}};
            result["runtime_version"] = proton->version;
            result["exit_code_scope"] = "Proton launcher; not independently verified game exit";
            result["runtime_mode"] = "Proton script on host; no Steam Linux Runtime container";
        }
        result["prefix"] = prefix;
        if (requestObserver)
            overrides["FOL_HTTP_OBSERVER_LAYOUT"] = profile.httpObserverLayout;
        if (environment.contains("FOL_HTTP_OBSERVER_DELAY_MS")) {
            bool validDelay;
            const auto delay = environment.value("FOL_HTTP_OBSERVER_DELAY_MS").toInt(&validDelay);
            require(requestObserver && validDelay && delay >= 0 && delay <= 30000,
                    "Observer delay must be 0-30000 milliseconds with the request observer");
            overrides["FOL_HTTP_OBSERVER_DELAY_MS"] = QString::number(delay);
        }
        if (environment.contains("FOL_HTTP_OBSERVER_MODE")) {
            const auto mode = environment.value("FOL_HTTP_OBSERVER_MODE");
            require(requestObserver && (mode == "hold-only" || mode == "attach-only"),
                    "Unknown request-observer control mode");
            overrides["FOL_HTTP_OBSERVER_MODE"] = mode;
        }
        if (environment.contains("FOL_HTTP_PROBE_CODE")) {
            require(!helper.isEmpty() && environment.value("FOL_HTTP_PROBE_CODE") == "1",
                    "FOL_HTTP_PROBE_CODE=1 requires the isolated HTTP probe helper");
            overrides["FOL_HTTP_PROBE_CODE"] = "1";
            result["http_code_observation_requested"] = true;
        }
        auto unshare = QStandardPaths::findExecutable("unshare");
        require(!unshare.isEmpty(), "util-linux unshare is required for isolated startup probes");
        auto probe = run(unshare, {"--user", "--map-root-user", "--net", "true"});
        require(probe.exitCode == 0 && !probe.timedOut,
                "Network isolation unavailable: " + probe.output);
        auto executable = build.root + "/" + profile.executable;
        QStringList args{"--user", "--map-root-user", "--net", "--", runtime};
        if (proton)
            args.append("run");
        if (helper.isEmpty()) {
            args.append(executable);
        } else {
            require(
                QFileInfo(prefixPath + "/dosdevices/z:").symLinkTarget() == "/",
                "HTTP interface probe requires initialized prefix with standard Z drive mapping");
            QString target = "Z:" + executable;
            target.replace('/', '\\');
            args.append({helper, target});
            result["exit_code_scope"] =
                "Runtime wrapper around HTTP probe helper; inspect mandatory helper report";
            QString reportPath = "Z:" + directory + "/http-probe.jsonl";
            reportPath.replace('/', '\\');
            overrides["FOL_HTTP_PROBE_LOG"] = reportPath;
        }
        args.append(profile.arguments);
        if (launcherContextProbe)
            args.append(profile.launcherContextArguments);
        if (localBackendProbe)
            args.append(backendArguments);
        // Logging-only experiment, kept separate from the unchanged baseline.
        // A shipping build may ignore these; requesting output does not prove it
        // was enabled or that startup reached engine logging initialization.
        if (engineStdout)
            args.append({"-stdout", "-FullStdOutLogOutput", "-forcelogflush"});
        QStringList shellArgs;
        for (const auto &arg : args)
            shellArgs.append(quote(arg));
        const QMap<QString, QString> common{{"WINEPREFIX", prefixPath},
                                            {"FOL_SESSION_ID", session},
                                            {"WINEDEBUG", "+timestamp,+pid,+tid,+seh,+loaddll"},
                                            {"DXVK_LOG_PATH", directory},
                                            {"VKD3D_LOG_FILE", directory + "/vkd3d.log"},
                                            {"WINEDLLOVERRIDES", "winemenubuilder.exe=d"}};
        for (auto it = common.begin(); it != common.end(); ++it)
            overrides[it.key()] = it.value();
        if (traceStartup)
            overrides["WINEDEBUG"] += ",+msgbox,+debugstr,+process,+exec,+reg";
        // A bundled Wine may require its own DLL/library search directories.
        // Record an explicitly supplied OpenSSL CPU-code-path diagnostic too;
        // do not enable any capability mask by default.
        // Preserve and record these specific inherited settings in the command;
        // do not dump the entire environment, which may contain credentials.
        QJsonObject inheritedRuntime;
        const auto inherited = QProcessEnvironment::systemEnvironment();
        for (const QString key :
             {"WINEDLLPATH", "LD_LIBRARY_PATH", "WINEARCH", "OPENSSL_ia32cap"}) {
            if (inherited.contains(key) &&
                (!proton || key == "LD_LIBRARY_PATH" || key == "OPENSSL_ia32cap")) {
                overrides[key] = inherited.value(key);
                inheritedRuntime[key] = inherited.value(key);
            }
        }
        result["inherited_runtime_environment"] = inheritedRuntime;
        QStringList assignments;
        for (auto it = overrides.begin(); it != overrides.end(); ++it)
            assignments.append(it.key() + "=" + quote(it.value()));
        QString command = "cd -- " + quote(build.root + "/" + profile.workingDirectory) +
                          " && env " + unsetArgs.join(' ') + " " + assignments.join(' ') + " " +
                          quote(unshare) + " " + shellArgs.join(' ') + " >" +
                          quote(directory + "/wine.log") + " 2>&1";
        result["command"] = command;
        result["runtime"] = runtime;
        result["process"] = executable;
        writeFile(directory + "/launch-command.txt", command.toUtf8() + '\n');
        if (printOnly) {
            result["status"] = "planned; not executed";
        } else {
            std::unique_ptr<QLockFile> wineLock;
            if (!proton) {
                require(prefix["initialized"].toBool(),
                        "Dedicated prefix is not initialized; use prefix create " + id);
                wineLock = std::make_unique<QLockFile>(prefixPath + ".lock");
                require(wineLock->tryLock(), "Prefix is in use by another launcher operation");
                require(ownedProcesses(prefixPath, {}).isEmpty(),
                        "Dedicated prefix has active Wine processes");
            }
            auto report = systemReport();
            writeFile(directory + "/system.json", QJsonDocument(report).toJson());
            require(report["vulkan"].toObject()["functional"].toBool(),
                    "Vulkan device enumeration failed; see system.json");
            if (proton)
                proton.emplace(prepareProtonSession(paths, id, protonId, session, directory, true));
            auto env = environment;
            for (auto it = overrides.begin(); it != overrides.end(); ++it)
                env.insert(it.key(), it.value());
            const auto launchStarted = QDateTime::currentDateTimeUtc();
            const auto outcome =
                superviseRuntime(unshare, args, env, directory, "wine.log", timeoutSeconds * 1000,
                                 {build.root + "/" + profile.workingDirectory, true});
            result["exit_code"] = outcome.exitCode;
            result["owned_processes_remaining"] = outcome.remaining;
            result["crashed"] = outcome.crashed;
            result["stop_requested"] = outcome.stopRequested;
            result["crash_diagnosis"] = outcome.stopRequested
                                            ? "indeterminate: launcher requested termination; raw "
                                              "crashed flag is QProcess status"
                                        : outcome.crashed ? "runtime signal exit; inspect logs"
                                                          : "no runtime signal exit observed";
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
            if (!helper.isEmpty()) {
                QFile receipt(directory + "/http-probe.jsonl");
                const auto evidence = analyzeHttpProbe(
                    receipt.open(QIODevice::ReadOnly) ? receipt.read(65537) : QByteArray{},
                    session);
                result["http_probe_report"] = evidence;
                if (requestObserver && !evidence["request_observation_completed"].toBool() &&
                    !result.contains("error"))
                    result["error"] = "No completed request observation; inspect probe report";
                if (!evidence["complete"].toBool() || !evidence["candidate_observed"].toBool()) {
                    if (!result.contains("error"))
                        result["error"] = evidence.contains("error")
                                              ? evidence["error"]
                                              : QJsonValue("HTTP interface candidate not found");
                }
            }
            QFile log(directory + "/wine.log");
            if (log.open(QIODevice::ReadOnly)) {
                log.seek(qMax(qint64(0), log.size() - 1024 * 1024));
                result["likely_subsystem"] = classifyFailure(QString::fromUtf8(log.readAll()));
            }
            if (outcome.started && !outcome.remaining) {
                result["game_logs"] = captureGameLogs(prefixPath, directory, launchStarted);
                const auto files = result["game_logs"].toObject()["files"].toArray();
                for (const auto &file : files) {
                    const auto observations = file.toObject()["observations"].toObject();
                    if (observations["backend_rejected_logged"].toBool())
                        result["game_reported_blocker"] =
                            "Backend configuration rejected; see game_logs observations and "
                            "source line numbers. This is separate from runtime exit status.";
                }
            } else
                result["game_logs"] =
                    QJsonObject{{"status", "not collected: no start or incomplete cleanup"}};
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
