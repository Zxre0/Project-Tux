#include "fol/core.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>

// Explicit manual Wine test, not an automatic CI test. It uses only the existing
// launcher-owned runtime-smoke prefix and never executes a Fortnite installation.
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        fol::require(argc == 3 || argc == 4,
                     "Usage: windows-probe-runner HELPER_EXE FIXTURE_EXE [LIFECYCLE_DLL]");
        const auto paths = fol::Paths::system();
        const auto status = fol::prefixStatus(paths, "runtime-smoke");
        fol::require(status["owned"].toBool() && status["initialized"].toBool() &&
                         status["active_processes"].toInt() == 0,
                     "Owned runtime-smoke prefix must be initialized and idle");
        QLockFile lock(paths.prefix("runtime-smoke") + ".lock");
        fol::require(lock.tryLock(), "Runtime smoke prefix is busy");
        fol::require(QFileInfo(paths.prefix("runtime-smoke") + "/dosdevices/z:").symLinkTarget() ==
                         "/",
                     "Smoke prefix requires its standard Z drive mapping");
        const auto helper = QFileInfo(app.arguments()[1]).canonicalFilePath();
        const bool observer = QFileInfo(helper).fileName() == "fol-http-observer.exe";
        const bool moduleBootstrap = QFileInfo(helper).fileName() == "fol-module-bootstrap.exe";
        fol::require(moduleBootstrap == (argc == 4),
                     "Module bootstrap requires one diagnostic DLL");
        QString module;
        if (moduleBootstrap) {
            module = QFileInfo(app.arguments()[3]).canonicalFilePath();
            const auto validation = fol::moduleReport(module, module + ".toml", "12.50");
            fol::require(validation["status"] ==
                                 "manifest-matches; runtime-compatibility-unverified" &&
                             validation["role"] == "http-lifecycle-probe",
                         "Diagnostic module failed manifest checks");
        }
        const auto fixture = QFileInfo(app.arguments()[2]).canonicalFilePath();
        const auto helperPe = fol::inspectPe(helper), fixturePe = fol::inspectPe(fixture);
        fol::require(helperPe.valid && helperPe.x64 && !helperPe.dll && fixturePe.valid &&
                         fixturePe.x64 && !fixturePe.dll,
                     "Both fixtures must be x64 PE images");
        auto windowsFixture = "Z:" + fixture;
        windowsFixture.replace('/', '\\');
        const auto session = "http-fixture-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto directory = paths.state + "/logs/sessions/" + session;
        fol::require(QDir().mkpath(directory) &&
                         QFile::setPermissions(directory, QFileDevice::ReadOwner |
                                                              QFileDevice::WriteOwner |
                                                              QFileDevice::ExeOwner),
                     "Cannot create private session directory");
        const auto unshare = QStandardPaths::findExecutable("unshare");
        fol::require(!unshare.isEmpty(), "Network isolation tool missing");
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", paths.prefix("runtime-smoke"));
        env.insert("FOL_SESSION_ID", session);
        QString reportPath = "Z:" + directory + "/http-probe.jsonl";
        reportPath.replace('/', '\\');
        env.insert("FOL_HTTP_PROBE_LOG", reportPath);
        QString moduleAssignments;
        if (moduleBootstrap) {
            QString modulePath = "Z:" + module;
            QString moduleReportPath = "Z:" + directory + "/http-module.json";
            modulePath.replace('/', '\\');
            moduleReportPath.replace('/', '\\');
            env.insert("FOL_HTTP_MODULE", modulePath);
            env.insert("FOL_HTTP_MODULE_REPORT", moduleReportPath);
            moduleAssignments = " FOL_HTTP_MODULE=" + fol::quote(modulePath) +
                                " FOL_HTTP_MODULE_REPORT=" + fol::quote(moduleReportPath);
        }
        env.insert("WINEDEBUG", "+timestamp,+pid,+seh,+loaddll");
        env.insert("WINEDLLOVERRIDES", "winemenubuilder.exe=d");
        const bool codeRequested = env.contains("FOL_HTTP_PROBE_CODE");
        fol::require(!codeRequested || env.value("FOL_HTTP_PROBE_CODE") == "1",
                     "Only FOL_HTTP_PROBE_CODE=1 is supported");
        const QStringList args{"--user", "--map-root-user", "--net", "--", fol::wineExecutable(),
                               helper,   windowsFixture};
        QStringList quoted;
        for (const auto &argument : args)
            quoted.append(fol::quote(argument));
        fol::writeFile(directory + "/launch-command.txt",
                       ("env WINEPREFIX=" + fol::quote(env.value("WINEPREFIX")) +
                        " FOL_SESSION_ID=" + fol::quote(session) + " FOL_HTTP_PROBE_LOG=" +
                        fol::quote(reportPath) + (codeRequested ? " FOL_HTTP_PROBE_CODE=1" : "") +
                        moduleAssignments + " WINEDEBUG=" + fol::quote(env.value("WINEDEBUG")) +
                        " WINEDLLOVERRIDES=" + fol::quote(env.value("WINEDLLOVERRIDES")) + " " +
                        fol::quote(unshare) + " " + quoted.join(' ') + "\n")
                           .toUtf8());
        auto outcome =
            fol::superviseRuntime(unshare, args, env, directory, "wine.log", 45000, {{}, true});
        QFile receipt(directory + "/http-probe.jsonl");
        const auto receiptData =
            receipt.open(QIODevice::ReadOnly) ? receipt.read(65537) : QByteArray{};
        const auto evidence = fol::analyzeHttpProbe(receiptData, session);
        QJsonObject moduleReceipt;
        if (moduleBootstrap) {
            QFile file(directory + "/http-module.json");
            moduleReceipt = fol::analyzeModuleReceipt(
                file.open(QIODevice::ReadOnly) ? file.read(4097) : QByteArray{}, session,
                evidence["child_windows_pid"].toInteger());
        }
        const auto requests = evidence["requests"].toArray();
        bool observedRequests = evidence["request_observation_completed"].toBool() &&
                                requests.size() == 8 && !receiptData.contains("fixture-secret") &&
                                !receiptData.contains("never-log-this");
        for (const auto &request : requests)
            observedRequests &= request.toObject()["origin"] == "loopback" &&
                                request.toObject()["route"] == "oauth-token";
        const auto tables = evidence["pointer_tables"].toArray();
        const bool fixtureTable = tables.size() == 1 &&
                                  tables[0].toObject()["candidate_slot"].toInt(-1) == 0 &&
                                  tables[0].toObject()["function_rvas"].toArray().size() == 2 &&
                                  !tables[0].toObject()["scan_limit_reached"].toBool(true);
        const bool passed = outcome.started && outcome.exitCode == 0 && !outcome.remaining &&
                            !outcome.eac && !outcome.battleye && outcome.moduleEvidence.isEmpty() &&
                            outcome.error.isEmpty() && !outcome.timedOut &&
                            evidence["complete"].toBool() &&
                            evidence["candidate_observed"].toBool() && fixtureTable &&
                            (!observer || observedRequests) &&
                            (!moduleBootstrap || (moduleReceipt["ready"].toBool() &&
                                                  evidence["module_loaded"].toBool() &&
                                                  evidence["module_control_child_live"].toBool()));
        QJsonObject result{{"session_id", session},
                           {"logs", directory},
                           {"passed", passed},
                           {"exit_code", outcome.exitCode},
                           {"owned_processes_remaining", outcome.remaining},
                           {"error", outcome.error},
                           {"http_probe_report", evidence},
                           {"module_receipt", moduleReceipt},
                           {"fortnite_executed", false}};
        fol::writeFile(directory + "/result.json", QJsonDocument(result).toJson());
        QTextStream(stdout) << QJsonDocument(result).toJson();
        return passed ? 0 : 1;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << '\n';
        return 1;
    }
}
