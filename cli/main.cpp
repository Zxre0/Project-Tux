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
        "[ROOT...], diagnose, runtime list, prefix create|status ID, launch ID, module PATH, "
        "network ADDRESS PORT, logs, processes, version");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "Command and arguments", "command [arguments...]");
    parser.addOptions({{"id", "Registration ID (defaults to detected version)", "id"},
                       {"output", "Also save JSON to this file", "path"},
                       {"manifest", "Module compatibility TOML manifest", "path"},
                       {"print-command", "Plan only; never execute"},
                       {"offline-probe", "Network-isolated, unmodified direct startup diagnostic"},
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
            arity(2);
            fol::require(args[1] == "list", "Only runtime list is currently implemented");
            result["runtimes"] = fol::runtimes();
        } else if (command == "prefix") {
            arity(3);
            fol::require(args[1] == "create" || args[1] == "status",
                         "Supported prefix actions: create, status");
            result = args[1] == "create" ? fol::createPrefix(paths, args[2])
                                         : fol::prefixStatus(paths, args[2]);
        } else if (command == "launch") {
            arity(2);
            bool ok;
            int timeout = parser.value("timeout").toInt(&ok);
            fol::require(ok, "Invalid timeout");
            result = fol::launch(paths, args[1], parser.isSet("print-command"),
                                 parser.isSet("offline-probe"), timeout);
            exitCode = result.contains("error") || result["status"] == "stopped-eac-detected" ||
                               result["status"] == "probe-timeout" ||
                               result["exit_code"].toInt() != 0
                           ? 2
                           : 0;
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
            arity(1);
            result["path"] = paths.state + "/logs";
            result["sessions"] = QJsonArray::fromStringList(
                QDir(paths.state + "/logs/sessions")
                    .entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name));
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
