#include "fol/backend_http.h"
#include "fol/core.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTextStream>
#include <QUuid>

static void createPrivateFile(const QString &path, const QJsonObject &object) {
    QFile file(path);
    fol::require(file.open(QIODevice::WriteOnly | QIODevice::NewOnly) &&
                     file.setPermissions(QFile::ReadOwner | QFile::WriteOwner),
                 "Cannot exclusively create private backend file: " + path);
    const auto bytes = QJsonDocument(object).toJson();
    fol::require(file.write(bytes) == bytes.size() && file.flush(),
                 "Cannot write backend identity");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("fortnite-backend");
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Loopback-only local development backend; no Epic credentials");
    parser.addHelpOption();
    parser.addOptions(
        {{"config", "Private identity JSON", "file"},
         {"port", "Loopback HTTP port", "port", "3551"},
         {"isolated-session", "Enable loopback only in a separate network namespace"},
         {"outer-netns", "Trusted launcher namespace identity captured before unshare",
          "device:inode"},
         {"log", "New session JSONL log", "file"}});
    parser.addPositionalArgument("mode", "init, serve, or run -- COMMAND [ARGS]");
    parser.addPositionalArgument("command", "Owned child command for run", "[command...]");
    parser.process(app);
    try {
        const auto args = parser.positionalArguments();
        fol::require(!args.isEmpty(), "Expected init, serve or run");
        const auto mode = args[0];
        const auto directory = fol::Paths::system().config + "/backend";
        const auto config = parser.isSet("config")
                                ? QFileInfo(parser.value("config")).absoluteFilePath()
                                : directory + "/identity.json";
        if (mode == "init") {
            fol::require(args.size() == 1, "init accepts no child command");
            const auto parent = QFileInfo(config).absolutePath();
            fol::require(QDir().mkpath(parent), "Cannot create backend config directory");
            const auto credentials = parent + "/local-login.json";
            fol::require(!QFileInfo::exists(config) && !QFileInfo(config).isSymLink() &&
                             !QFileInfo::exists(credentials) && !QFileInfo(credentials).isSymLink(),
                         "Backend identity already exists; refusing to overwrite");
            const auto password = QUuid::createUuid().toString(QUuid::Id128).toLatin1();
            const auto identity =
                fol::backend::Identity::create("player@localhost", "Local Player", password);
            createPrivateFile(
                config, {{"username", identity.username},
                         {"display_name", identity.displayName},
                         {"account_id", identity.accountId},
                         {"salt", QString::fromLatin1(identity.salt.toHex())},
                         {"password_hash", QString::fromLatin1(identity.passwordHash.toHex())}});
            createPrivateFile(credentials, {{"username", identity.username},
                                            {"password", QString::fromLatin1(password)}});
            QTextStream(stdout) << "Created local-only identity. Private login file: "
                                << credentials << '\n';
            return 0;
        }
        fol::require(mode == "serve" || mode == "run", "Expected init, serve or run");
        fol::require(mode == "run" ? args.size() >= 2 : args.size() == 1, "Invalid child command");
        const auto info = QFileInfo(config);
        fol::require(info.isFile() && !info.isSymLink() &&
                         !(info.permissions() & (QFile::ReadGroup | QFile::WriteGroup |
                                                 QFile::ReadOther | QFile::WriteOther)),
                     "Backend identity must be a private regular file");
        const auto data = fol::readJson(config);
        fol::backend::Identity identity{
            data["username"].toString(), data["display_name"].toString(),
            data["account_id"].toString(), QByteArray::fromHex(data["salt"].toString().toLatin1()),
            QByteArray::fromHex(data["password_hash"].toString().toLatin1())};
        if (parser.isSet("isolated-session"))
            fol::preparePrivateLoopback(parser.value("outer-netns"));
        bool valid;
        const auto port = parser.value("port").toUInt(&valid);
        fol::require(valid && port > 0 && port <= 65535, "Port must be 1-65535");
        QFile logFile;
        if (parser.isSet("log")) {
            logFile.setFileName(parser.value("log"));
            fol::require(logFile.open(QIODevice::WriteOnly | QIODevice::NewOnly) &&
                             logFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner),
                         "Cannot create private backend session log");
        }
        const auto logger = [&](QJsonObject entry) {
            entry["session_id"] = qEnvironmentVariable("FOL_SESSION_ID");
            entry["process_id"] = QCoreApplication::applicationPid();
            const auto bytes = QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n';
            if (logFile.isOpen() && (logFile.write(bytes) != bytes.size() || !logFile.flush()))
                app.exit(74);
            QTextStream(stdout) << bytes;
        };
        fol::backend::HttpServer server(std::move(identity), logger);
        fol::require(server.start(quint16(port)), "Backend bind failed: " + server.errorString());
        logger({{"component", "backend"}, {"event", "listening-loopback"}, {"port", int(port)}});
        QProcess child;
        if (mode == "run") {
            fol::require(parser.isSet("isolated-session"),
                         "Owned game run requires isolated-session");
            child.setProcessChannelMode(QProcess::ForwardedChannels);
            QObject::connect(&child, &QProcess::finished, &app,
                             [&](int code, QProcess::ExitStatus status) {
                                 logger({{"component", "backend"},
                                         {"event", "owned-child-exited"},
                                         {"exit_code", code}});
                                 app.exit(status == QProcess::NormalExit ? code : 70);
                             });
            QObject::connect(&child, &QProcess::errorOccurred, &app,
                             [&](QProcess::ProcessError error) {
                                 if (error == QProcess::FailedToStart)
                                     app.exit(71);
                             });
            child.start(args[1], args.mid(2));
        }
        return app.exec();
    } catch (const std::exception &error) {
        QTextStream(stderr) << "Backend: " << error.what() << '\n';
        return 2;
    }
}
