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
                   const QString &httpProbeHelper, const QString &httpModule, bool privateBackend,
                   bool developmentLogin, const QString &lawinConfig,
                   const LauncherContextExperiments &context, WorldProbe worldProbe,
                   const QString &sdkProbeModule) {
    const bool launcherPresenceProbe = context.presence, launcherIdentityProbe = context.identity;
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
                       {"world_probe", worldProbe == WorldProbe::Standalone ? "standalone"
                                       : worldProbe == WorldProbe::Listen   ? "listen"
                                                                            : "none"},
                       {"launcher_context_probe", launcherContextProbe},
                       {"launcher_presence_probe", launcherPresenceProbe},
                       {"launcher_identity_probe", launcherIdentityProbe},
                       {"reboot_bootstrap_context_probe", context.rebootBootstrap},
                       {"local_backend_probe", localBackendProbe},
                       {"backend_probe_method", backendProbe == BackendProbe::ServiceIni
                                                    ? "service-ini"
                                                : localBackendProbe ? "selector"
                                                                    : "none"},
                       {"launcher_pid", QCoreApplication::applicationPid()}};
    try {
        require(sdkProbeModule.isEmpty() ||
                    (worldProbe == WorldProbe::None && offlineProbe && launcherContextProbe &&
                     privateBackend && developmentLogin && !lawinConfig.isEmpty() &&
                     context.identity),
                "SDK probe requires isolated local Lawin login/identity and excludes world probes");
        require(worldProbe == WorldProbe::None || worldProbe == WorldProbe::Standalone ||
                    worldProbe == WorldProbe::Listen,
                "Unknown world probe");
        require(worldProbe == WorldProbe::None ||
                    (offlineProbe && launcherContextProbe && privateBackend && developmentLogin &&
                     !lawinConfig.isEmpty() && context.identity),
                "World probe requires isolated launcher context, local Lawin login and identity");
        require(!context.rebootBootstrap ||
                    (offlineProbe && launcherContextProbe && privateBackend && context.identity &&
                     !localBackendProbe),
                "Reboot bootstrap context requires isolated private backend and identity probe");
        require(!launcherIdentityProbe ||
                    (offlineProbe && launcherContextProbe && privateBackend && !localBackendProbe),
                "Launcher identity requires isolated launcher context and private backend, "
                "without alternate backend-selection arguments");
        require(!launcherPresenceProbe || (offlineProbe && launcherContextProbe && privateBackend &&
                                           !httpProbeHelper.isEmpty() && !httpModule.isEmpty()),
                "Launcher presence requires isolated launcher context, module bootstrap and "
                "private backend");
        require(lawinConfig.isEmpty() || privateBackend,
                "Lawin deployment requires --private-backend");
        require(!developmentLogin || (privateBackend && !traceStartup && !engineStdout),
                "Development login requires the isolated private backend and excludes verbose "
                "argument tracing");
        require(httpModule.isEmpty() || !httpProbeHelper.isEmpty(),
                "HTTP module requires the owned-child module bootstrap helper");
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
        QString worldUrl;
        if (worldProbe != WorldProbe::None) {
            worldUrl =
                worldProbe == WorldProbe::Standalone ? profile.soloWorldUrl : profile.soloListenUrl;
            require(!worldUrl.isEmpty(), "No world probe is defined for this build");
            result["world_probe_url"] = worldUrl;
            result["world_probe_scope"] =
                "engine URL request only; world/server support unverified; "
                "no security-check or game-module changes";
        }
        require(!launcherIdentityProbe || !profile.launcherIdentityArguments.isEmpty(),
                "No launcher identity experiment is defined for this build");
        require(!context.rebootBootstrap || !profile.rebootBootstrapArguments.isEmpty(),
                "No Reboot bootstrap context experiment is defined for this build");
        QString helper, checkedModule;
        bool requestObserver = false, moduleBootstrap = false, moduleObserver = false,
             moduleRouter = false;
        if (!httpProbeHelper.isEmpty()) {
            require(profile.httpInterfaceProbeAllowed,
                    "HTTP interface probe is not enabled for this build");
            helper = QFileInfo(httpProbeHelper).canonicalFilePath();
            requestObserver = QFileInfo(helper).fileName() == "fol-http-observer.exe";
            moduleBootstrap = QFileInfo(helper).fileName() == "fol-module-bootstrap.exe";
            require(moduleBootstrap == !httpModule.isEmpty(),
                    "HTTP module and fol-module-bootstrap.exe must be supplied together");
            const auto pe = inspectPe(helper);
            require(pe.valid && pe.x64 && !pe.dll,
                    "HTTP probe helper must be a valid x64 PE executable");
            result["http_interface_probe"] = QJsonObject{
                {"helper", helper},
                {"sha256",
                 QString::fromLatin1(QCryptographicHash::hash(readFile(helper, 8 * 1024 * 1024),
                                                              QCryptographicHash::Sha256)
                                         .toHex())},
                {"scope", "trusted supplied helper; owned-child diagnostic; "
                          "no route or login is established by a candidate"}};
            if (requestObserver || moduleBootstrap) {
                require(!profile.httpObserverHash.isEmpty() &&
                            !profile.httpObserverLayout.isEmpty(),
                        "No request observer is defined for this build");
                QFile game(build.root + "/" + profile.executable);
                QCryptographicHash hash(QCryptographicHash::Sha256);
                require(game.open(QIODevice::ReadOnly) && hash.addData(&game) &&
                            QString::fromLatin1(hash.result().toHex()) == profile.httpObserverHash,
                        "Request observer executable hash mismatch; no diagnostic child started");
                result["request_observer"] = requestObserver;
            }
            if (moduleBootstrap) {
                checkedModule = QFileInfo(httpModule).canonicalFilePath();
                moduleRouter = QFileInfo(checkedModule).fileName() == "fol-http-local-router.dll";
                moduleObserver = moduleRouter || QFileInfo(checkedModule).fileName() ==
                                                     "fol-http-request-observer.dll";
                require(moduleObserver ||
                            QFileInfo(checkedModule).fileName() == "fol-http-adapter.dll",
                        "Only the explicitly built diagnostic modules are supported");
                const auto module =
                    moduleReport(checkedModule, checkedModule + ".toml", build.version);
                require(module["status"] == "manifest-matches; runtime-compatibility-unverified" &&
                            module["role"] == (moduleRouter     ? "http-local-router"
                                               : moduleObserver ? "http-request-observer"
                                                                : "http-lifecycle-probe"),
                        "Module architecture, version, role or SHA-256 manifest check failed");
                result["http_module"] = module;
            }
        }
        require(!privateBackend || moduleRouter,
                "Private backend requires the isolated local router module");
        QString checkedSdk;
        if (!sdkProbeModule.isEmpty()) {
            checkedSdk = QFileInfo(sdkProbeModule).canonicalFilePath();
            require(moduleRouter && QFileInfo(checkedSdk).fileName() == "fol-solo-sdk-probe.dll",
                    "Only the built SDK-only diagnostic with the local router is supported");
            const auto module = moduleReport(checkedSdk, checkedSdk + ".toml", build.version);
            require(module["status"] == "manifest-matches; runtime-compatibility-unverified" &&
                        module["role"] == "solo-sdk-probe",
                    "SDK probe architecture, version, role or SHA-256 check failed");
            result["sdk_probe_module"] = module;
        }
        QString presenceLauncher;
        if (launcherPresenceProbe) {
            require(moduleBootstrap, "Launcher presence requires the owned module bootstrap");
            const QFileInfo candidate(build.root +
                                      "/FortniteGame/Binaries/Win64/FortniteLauncher.exe");
            presenceLauncher = candidate.canonicalFilePath();
            require(
                candidate.isFile() && !candidate.isSymLink() &&
                    presenceLauncher.startsWith(QFileInfo(build.root).canonicalFilePath() + "/"),
                "Launcher presence requires the installation's regular FortniteLauncher.exe");
            const auto pe = inspectPe(presenceLauncher);
            require(pe.valid && pe.x64 && !pe.dll,
                    "Presence launcher must be an x64 PE executable");
            result["launcher_presence"] = QJsonObject{
                {"path", presenceLauncher},
                {"sha256", QString::fromLatin1(QCryptographicHash::hash(
                                                   readFile(presenceLauncher, 64 * 1024 * 1024),
                                                   QCryptographicHash::Sha256)
                                                   .toHex())},
                {"scope", "supplied launcher created suspended and never resumed; spoofed "
                          "presence experiment, not authentic Epic authorization"}};
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
        require(!environment.contains("FOL_HTTP_MODULE") &&
                    !environment.contains("FOL_HTTP_MODULE_REPORT"),
                "Module path and report must come from the checked launch options");
        require(!environment.contains("FOL_HTTP_OBSERVER_LAYOUT"),
                "Observer layout must come from the checked compatibility profile");
        require(!environment.contains("FOL_HTTP_ROUTER_LAYOUT"),
                "Router layout must come from the checked compatibility profile");
        require(!environment.contains("FOL_HTTP_MODULE_MS"),
                "Module lifetime is managed by the launcher");
        require(!environment.contains("FOL_DEV_LOGIN_FILE"),
                "Development login file is managed by the launcher");
        require(!environment.contains("FOL_LAUNCHER_PRESENCE_PATH"),
                "Launcher presence path must come from the checked launch option");
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
        if (launcherPresenceProbe) {
            QString windowsLauncher = "Z:" + presenceLauncher;
            windowsLauncher.replace('/', '\\');
            overrides["FOL_LAUNCHER_PRESENCE_PATH"] = windowsLauncher;
        }
        QByteArray localSecret;
        if (developmentLogin) {
            const auto loginFile = paths.config + "/backend/local-login.json";
            const QFileInfo info(loginFile);
            require(info.isFile() && !info.isSymLink() &&
                        !(info.permissions() & (QFile::ReadGroup | QFile::WriteGroup |
                                                QFile::ReadOther | QFile::WriteOther)),
                    "Generated development login must be an owner-only regular file");
            const auto login = readJson(loginFile);
            static const QRegularExpression user("\\A[a-z0-9_-]{1,32}@localhost\\z"),
                password("\\A[0-9a-f]{32}\\z");
            require(login.size() == 2 && user.match(login["username"].toString()).hasMatch() &&
                        password.match(login["password"].toString()).hasMatch(),
                    "Only generated local development credentials are accepted");
            localSecret = login["password"].toString().toLatin1();
            QString windowsLogin = "Z:" + loginFile;
            windowsLogin.replace('/', '\\');
            overrides["FOL_DEV_LOGIN_FILE"] = windowsLogin;
            result["development_login"] = QJsonObject{
                {"username", login["username"]},
                {"scope",
                 "ordinary local password authentication; success requires backend/game evidence"}};
        }
        if (moduleBootstrap) {
            QString modulePath = "Z:" + checkedModule;
            QString moduleReportPath = "Z:" + directory + "/http-module.json";
            modulePath.replace('/', '\\');
            moduleReportPath.replace('/', '\\');
            overrides["FOL_HTTP_MODULE"] = modulePath;
            overrides["FOL_HTTP_MODULE_REPORT"] = moduleReportPath;
        }
        if (requestObserver || moduleObserver)
            overrides["FOL_HTTP_OBSERVER_LAYOUT"] = profile.httpObserverLayout;
        if (moduleRouter) {
            require(!profile.httpRouterLayout.isEmpty(), "No local router layout for this build");
            overrides["FOL_HTTP_ROUTER_LAYOUT"] = profile.httpRouterLayout;
            overrides["FOL_HTTP_MODULE_MS"] =
                QString::number(qBound(20000, (timeoutSeconds - 35) * 1000, 300000));
        }
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
        QStringList args{"--user", "--map-root-user", "--net", "--"};
        if (privateBackend) {
            const auto backend = QCoreApplication::applicationDirPath() + "/fortnite-backend";
            const auto identity = paths.config + "/backend/identity.json";
            require(QFileInfo(backend).isExecutable() && QFileInfo(identity).isFile(),
                    "Native backend or private identity missing; run fortnite-backend init first");
            args.append({backend, "--config", identity, "--log", directory + "/backend.log",
                         "--isolated-session", "--outer-netns", networkNamespaceIdentity()});
            if (!lawinConfig.isEmpty()) {
                const QFileInfo config(lawinConfig);
                require(config.isFile() && !config.isSymLink(), "Lawin deployment config missing");
                args.append({"--lawin-config", config.absoluteFilePath()});
            }
            args.append({"run", "--"});
            overrides["NO_PROXY"] = "127.0.0.1,localhost";
            overrides["no_proxy"] = "127.0.0.1,localhost";
            result["private_backend"] =
                QJsonObject{{"bind", "127.0.0.1:3551"},
                            {"provider", lawinConfig.isEmpty() ? "native" : "lawin-v2"},
                            {"scope", "same isolated namespace; request/login outcome must be "
                                      "verified from backend log"}};
        }
        args.append(runtime);
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
        // Unreal's first positional game argument is a map URL. Keep this after
        // the owned child's executable, not before the Proton/helper program.
        if (!worldUrl.isEmpty())
            args.append(worldUrl);
        args.append(profile.arguments);
        if (!checkedSdk.isEmpty()) {
            auto windowsPath = [](QString path) {
                path.replace('/', '\\');
                return "Z:" + path;
            };
            overrides["FOL_SOLO_SDK_MODULE"] = windowsPath(checkedSdk);
            overrides["FOL_SOLO_SDK_REPORT"] = windowsPath(directory + "/solo-sdk-probe.jsonl");
        }
        if (launcherContextProbe)
            args.append(profile.launcherContextArguments);
        if (launcherIdentityProbe) {
            args.append(profile.launcherIdentityArguments);
            result["launcher_identity_arguments"] =
                QJsonArray::fromStringList(profile.launcherIdentityArguments);
        }
        if (context.rebootBootstrap) {
            args.append(profile.rebootBootstrapArguments);
            result["reboot_bootstrap_context"] = QJsonObject{
                {"arguments", QJsonArray::fromStringList(profile.rebootBootstrapArguments)},
                {"scope", "public upstream command-line context; EAC is not started; "
                          "not genuine Epic authorization"}};
        }
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
                if (!checkedSdk.isEmpty()) {
                    QFile sdkReceipt(directory + "/solo-sdk-probe.jsonl");
                    const auto sdk = analyzeSdkProbe(
                        sdkReceipt.open(QIODevice::ReadOnly) ? sdkReceipt.read(16385)
                                                             : QByteArray{},
                        session, evidence["child_windows_pid"].toInteger());
                    result["sdk_probe_observation"] = sdk;
                    if (!sdk["complete"].toBool() && !result.contains("error"))
                        result["error"] = "SDK compatibility probe did not complete successfully; "
                                          "inspect its receipt";
                }
                if (developmentLogin && !evidence["development_login_requested"].toBool() &&
                    !result.contains("error"))
                    result["error"] = "Helper did not confirm local development login arguments";
                if (moduleBootstrap) {
                    QFile moduleReceipt(directory + "/http-module.json");
                    const auto readiness = analyzeModuleReceipt(
                        moduleReceipt.open(QIODevice::ReadOnly) ? moduleReceipt.read(4097)
                                                                : QByteArray{},
                        session, evidence["child_windows_pid"].toInteger());
                    result["http_module_receipt"] = readiness;
                    if (moduleObserver) {
                        QFile observations(directory + "/http-module.json.requests");
                        const auto observed = analyzeModuleObservation(
                            observations.open(QIODevice::ReadOnly) ? observations.read(4097)
                                                                   : QByteArray{},
                            session, evidence["child_windows_pid"].toInteger());
                        result["http_module_observation"] = observed;
                        if ((!observed["observed"].toBool() ||
                             (moduleRouter && observed["routed"].toInt() == 0)) &&
                            !result.contains("error"))
                            result["error"] =
                                "No completed in-process request observation; inspect receipts";
                    }
                    if ((!readiness["ready"].toBool() || !evidence["module_loaded"].toBool() ||
                         !evidence["module_control_child_live"].toBool()) &&
                        !result.contains("error"))
                        result["error"] = "Module lifecycle validation failed; inspect receipts";
                }
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
                result["game_logs"] =
                    captureGameLogs(prefixPath, directory, launchStarted, localSecret);
                const auto files = result["game_logs"].toObject()["files"].toArray();
                QJsonArray worlds;
                for (const auto &file : files) {
                    const auto observations = file.toObject()["observations"].toObject();
                    for (const auto &world : observations["worlds_brought_up"].toArray())
                        if (!worlds.contains(world))
                            worlds.append(world);
                    if (observations["backend_rejected_logged"].toBool())
                        result["game_reported_blocker"] =
                            "Backend configuration rejected; see game_logs observations and "
                            "source line numbers. This is separate from runtime exit status.";
                }
                if (worldProbe != WorldProbe::None) {
                    const auto requested = worldUrl.section('?', 0, 0).section('/', -1);
                    bool seen = false;
                    for (const auto &world : worlds)
                        seen |= world.toString().section('/', -1) == requested;
                    result["world_probe_observation"] = QJsonObject{
                        {"requested_world", requested},
                        {"observed", seen},
                        {"worlds_brought_up", worlds},
                        {"scope",
                         "recognized world log markers; not server readiness or gameplay"}};
                    if (!seen && !result.contains("error"))
                        result["error"] =
                            "Requested world was not observed; inspect world_probe_observation";
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
