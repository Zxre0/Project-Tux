#include "fol/core.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QTextStream>

static void output(const QJsonObject &value) {
    QTextStream(stdout) << QJsonDocument(value).toJson();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("fortnite-linux");
    app.setApplicationVersion("0.1.0-dev");
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Native Linux Fortnite development diagnostics. Gameplay and private-server integration "
        "remain unverified.\nCommands: builds, add PATH, remove ID, verify ID|PATH, discover "
        "[ROOT...], diagnose, runtime list|test ID, prefix "
        "create|status|stop|backup|backups|archive|reset ID, "
        "prefix restore ID BACKUP_TOKEN, launch ID, "
        "runtime import-proton PATH --id NAME, runtime test-proton ID --proton NAME, "
        "manifest PATH [--installation ROOT] [--hash-files], "
        "imports PE_PATH [--dll-dir DIRECTORY...], "
        "module PATH, "
        "network ADDRESS PORT, logs [analyze GAME_LOG], processes, version");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "Command and arguments", "command [arguments...]");
    parser.addOptions(
        {{"id", "Registration ID (defaults to detected version)", "id"},
         {"output", "Also save JSON to this file", "path"},
         {"manifest", "Module compatibility or build-integrity TOML manifest", "path"},
         {"installation", "Installation root for local Epic manifest verification", "path"},
         {"hash-files", "Hash all files listed in the supplied legacy Epic manifest (read-only)"},
         {"dll-dir",
          "Additional DLL inventory directory; repeatable; does not change Wine search paths",
          "path"},
         {"proton", "Project-imported Proton runtime ID (runtime test-proton or launch)", "id"},
         {"print-command", "Plan only; never execute"},
         {"offline-probe", "Network-isolated direct startup diagnostic"},
         {"trace-startup",
          "Capture Wine message-box, debug-string, process, shell and registry traces"},
         {"engine-stdout",
          "Request engine stdout logging and flushing; support is build-dependent"},
         {"launcher-context-probe",
          "Opt-in 12.50 launcher-context experiment; requires --offline-probe; no authentication"},
         {"local-backend-probe", "Probe built-in localhost backend selection; requires offline and "
                                 "launcher-context probes"},
         {"backend-probe-method", "Local backend experiment: selector or service-ini", "method",
          "selector"},
         {"http-probe-helper",
          "Trusted locally built Windows diagnostic helper; "
          "requires isolated context probe",
          "exe"},
         {"http-module",
          "Trusted lifecycle DLL with adjacent .dll.toml manifest; requires "
          "fol-module-bootstrap.exe and network isolation",
          "dll"},
         {"timeout", "Startup probe timeout in seconds (1-3600)", "seconds", "60"}});
    parser.process(app);
    auto args = parser.positionalArguments();
    if (args.isEmpty())
        parser.showHelp();
    try {
        auto paths = fol::Paths::system();
        fol::Registry registry(paths);
        QJsonObject result;
        int exitCode = 0;
        auto arity = [&](int count) {
            fol::require(args.size() == count, "Wrong argument count; see --help");
        };
        auto command = args[0];
        if (command == "version") {
            arity(1);
            result = {{"version", app.applicationVersion()}};
        } else if (command == "builds") {
            arity(1);
            QJsonArray entries;
            auto builds = registry.list();
            for (auto it = builds.begin(); it != builds.end(); ++it) {
                auto build = fol::inspectBuild(it.value()).json();
                build["id"] = it.key();
                entries.append(build);
            }
            result["builds"] = entries;
        } else if (command == "add") {
            arity(2);
            auto build = fol::inspectBuild(args[1]);
            auto id = parser.isSet("id") ? parser.value("id") : build.version;
            fol::require(build.valid(), "Invalid installation: " + build.errors.join("; "));
            registry.add(id, args[1]);
            result = build.json();
            result["id"] = id;
        } else if (command == "remove") {
            arity(2);
            registry.remove(args[1]);
            result = {{"removed_registration", args[1]}, {"game_files_deleted", false}};
        } else if (command == "verify") {
            arity(2);
            result =
                fol::inspectBuild(QFileInfo(args[1]).isDir() ? args[1] : registry.resolve(args[1]))
                    .json();
            exitCode = result["valid"].toBool() ? 0 : 2;
            if (parser.isSet("manifest")) {
                auto integrity =
                    fol::verifyIntegrity(result["path"].toString(), parser.value("manifest"));
                result["integrity"] = integrity;
                if (!integrity["valid"].toBool())
                    exitCode = 2;
            }
        } else if (command == "discover") {
            auto roots = args.mid(1);
            if (roots.isEmpty())
                roots = {QDir::homePath() + "/Games", "/mnt",
                         "/run/media/" + qEnvironmentVariable("USER"),
                         QDir::homePath() + "/.local/share/Steam/steamapps/common"};
            result["search_roots"] = QJsonArray::fromStringList(roots);
            result["candidates"] = QJsonArray::fromStringList(fol::discoverBuilds(roots));
            result["scope"] =
                "bounded directory scan; no symlink traversal, depth 7, at most 100000 directories";
        } else if (command == "diagnose") {
            arity(1);
            result = fol::systemReport();
            result["config_path"] = paths.config;
            result["data_path"] = paths.data;
            result["log_path"] = paths.state + "/logs";
        } else if (command == "runtime") {
            fol::require(args.size() >= 2, "Supported runtime actions: list, test PREFIX_ID");
            if (args[1] == "list") {
                arity(2);
                result["runtimes"] = fol::runtimes();
                result["imported_proton"] = fol::importedProtonRuntimes(paths);
            } else if (args[1] == "import-proton") {
                arity(3);
                fol::require(parser.isSet("id"), "Runtime import requires --id NAME");
                result = fol::importProton(paths, parser.value("id"), args[2]);
            } else if (args[1] == "test-proton") {
                arity(3);
                fol::require(parser.isSet("proton"), "Proton test requires --proton RUNTIME_ID");
                result = fol::testProton(paths, args[2], parser.value("proton"));
                exitCode = result["passed"].toBool() ? 0 : 2;
            } else {
                arity(3);
                fol::require(args[1] == "test", "Supported runtime actions: list, test PREFIX_ID");
                result = fol::testRuntime(paths, args[2]);
                exitCode = result["passed"].toBool() ? 0 : 2;
            }
        } else if (command == "prefix") {
            fol::require(args.size() >= 2, "Missing prefix action; see --help");
            arity(args[1] == "restore" ? 4 : 3);
            if (args[1] == "create")
                result = fol::createPrefix(paths, args[2]);
            else if (args[1] == "status")
                result = fol::prefixStatus(paths, args[2]);
            else if (args[1] == "stop")
                result = fol::stopPrefix(paths, args[2]);
            else if (args[1] == "backup" || args[1] == "archive")
                result = fol::backupPrefix(paths, args[2], args[1] == "archive");
            else if (args[1] == "backups")
                result = fol::prefixBackups(paths, args[2]);
            else if (args[1] == "restore")
                result = fol::restorePrefix(paths, args[2], args[3]);
            else if (args[1] == "reset")
                result = fol::resetPrefix(paths, args[2]);
            else
                throw fol::Error("Unknown prefix action; see --help");
        } else if (command == "launch") {
            arity(2);
            bool ok;
            int timeout = parser.value("timeout").toInt(&ok);
            fol::require(ok, "Invalid timeout");
            const auto method = parser.value("backend-probe-method");
            fol::require(method == "selector" || method == "service-ini",
                         "Unknown backend probe method");
            fol::require(!parser.isSet("backend-probe-method") ||
                             parser.isSet("local-backend-probe"),
                         "--backend-probe-method requires --local-backend-probe");
            const auto backendProbe = !parser.isSet("local-backend-probe") ? fol::BackendProbe::None
                                      : method == "service-ini" ? fol::BackendProbe::ServiceIni
                                                                : fol::BackendProbe::Selector;
            result = fol::launch(
                paths, args[1], parser.isSet("print-command"), parser.isSet("offline-probe"),
                timeout, parser.isSet("trace-startup"), parser.isSet("engine-stdout"),
                parser.value("proton"), parser.isSet("launcher-context-probe"), backendProbe,
                parser.value("http-probe-helper"), parser.value("http-module"));
            exitCode = result.contains("error") || result["status"] == "stopped-eac-detected" ||
                               result["status"] == "stopped-anticheat-detected" ||
                               result["status"] == "probe-timeout" ||
                               result["exit_code"].toInt() != 0
                           ? 2
                           : 0;
        } else if (command == "manifest") {
            arity(2);
            result = fol::inspectEpicManifest(args[1], parser.value("installation"),
                                              parser.isSet("hash-files"));
            if (result.contains("all_listed_files_match") &&
                !result["all_listed_files_match"].toBool())
                exitCode = 2;
        } else if (command == "imports") {
            arity(2);
            result = fol::peImportReport(args[1], parser.values("dll-dir"));
        } else if (command == "module") {
            arity(2);
            result = fol::moduleReport(args[1], parser.value("manifest"));
            exitCode = result["status"].toString().startsWith("manifest-matches") ? 0 : 2;
        } else if (command == "network") {
            arity(3);
            bool ok;
            auto port = args[2].toUInt(&ok);
            fol::require(ok && port > 0 && port <= 65535, "Port must be 1-65535");
            result = fol::portReport(args[1], quint16(port));
            exitCode = result["bind_available"].toBool() ? 0 : 2;
        } else if (command == "logs") {
            if (args.size() == 3 && args[1] == "analyze") {
                fol::require(QFileInfo(args[2]).isFile(), "Game log must be a regular file");
                result = fol::analyzeGameLog(fol::readFile(args[2], 64 * 1024 * 1024));
                result["source"] = QFileInfo(args[2]).absoluteFilePath();
            } else {
                arity(1);
                result["path"] = paths.state + "/logs";
                result["sessions"] = QJsonArray::fromStringList(
                    QDir(paths.state + "/logs/sessions")
                        .entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name));
            }
        } else if (command == "processes") {
            arity(1);
            QJsonArray entries;
            for (const auto &process : fol::processes())
                entries.append(process.json());
            result["processes"] = entries;
            result["scope"] = "readable /proc snapshot; names and ancestry only";
        } else if (command == "host") {
            throw fol::Error("Hosting blocked: no validated non-bypass Project Reboot integration "
                             "or 12.61 runtime evidence yet");
        } else
            throw fol::Error("Unknown command; see --help");
        if (parser.isSet("output"))
            fol::writeFile(parser.value("output"), QJsonDocument(result).toJson());
        output(result);
        return exitCode;
    } catch (const std::exception &error) {
        output({{"component", "launcher"},
                {"level", "ERROR"},
                {"error", QString::fromUtf8(error.what())}});
        return 2;
    }
}
