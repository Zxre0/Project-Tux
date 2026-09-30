#include "fol/core.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>

namespace {
int checks = 0;
void check(bool value, const QString &message) {
    fol::require(value, message);
    ++checks;
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        const QString sdkSession = "sdk-test-session";
        const QStringList sdkStages{"started",
                                    "sdk-init-started",
                                    "sdk-init-completed",
                                    "fortnite-version-times100",
                                    "engine-version-times100",
                                    "object-count",
                                    "game-mode-class-found",
                                    "net-driver-class-found",
                                    "ready-to-start-function-found",
                                    "process-event-vtable-slot",
                                    "world-found",
                                    "completed"};
        const QList<int> sdkValues{1, 1, 1, 1250, 425, 50000, 1, 1, 1, 64, 1, 1};
        QByteArray sdkReceipt;
        for (int i = 0; i < sdkStages.size(); ++i)
            sdkReceipt += QJsonDocument(QJsonObject{{"event", sdkStages[i]},
                                                    {"session_id", sdkSession},
                                                    {"process_id", 336},
                                                    {"value", sdkValues[i]}})
                              .toJson(QJsonDocument::Compact) +
                          '\n';
        check(fol::analyzeSdkProbe(sdkReceipt, sdkSession, 336)["complete"].toBool(),
              "ordered SDK receipt establishes only reflection prerequisites");
        check(!fol::analyzeSdkProbe(sdkReceipt, "other-session", 336)["complete"].toBool() &&
                  !fol::analyzeSdkProbe(sdkReceipt, sdkSession, 337)["complete"].toBool(),
              "SDK evidence must belong to the same owned child and session");
        auto wrongVersion = sdkReceipt;
        wrongVersion.replace("1250", "1261");
        check(!fol::analyzeSdkProbe(wrongVersion, sdkSession, 336)["complete"].toBool(),
              "SDK version mismatch never reports compatibility");
        auto wrongOrder = sdkReceipt;
        wrongOrder.replace("sdk-init-completed", "sdk-init-started");
        check(!fol::analyzeSdkProbe(wrongOrder, sdkSession, 336)["complete"].toBool() &&
                  !fol::analyzeSdkProbe(sdkReceipt + sdkReceipt, sdkSession, 336)["complete"]
                       .toBool(),
              "SDK duplicated stages and post-completion records rejected");
        const auto partial = sdkReceipt.left(sdkReceipt.indexOf("{\"event\":\"sdk-init-completed"));
        const auto partialResult = fol::analyzeSdkProbe(partial, sdkSession, 336);
        check(!partialResult["complete"].toBool() &&
                  partialResult["last_completed_stage"] == "sdk-init-started",
              "interrupted SDK probe retains bounded last-stage evidence");
        const auto failure = partial +
                             QJsonDocument(QJsonObject{{"event", "structured-exception"},
                                                       {"session_id", sdkSession},
                                                       {"process_id", 336},
                                                       {"value", 3221225477.0}})
                                 .toJson(QJsonDocument::Compact) +
                             '\n' +
                             QJsonDocument(QJsonObject{{"event", "completed"},
                                                       {"session_id", sdkSession},
                                                       {"process_id", 336},
                                                       {"value", 0}})
                                 .toJson(QJsonDocument::Compact) +
                             '\n';
        check(fol::analyzeSdkProbe(failure, sdkSession, 336)["failure_code"] == 3221225477.0 &&
                  !fol::analyzeSdkProbe(failure, sdkSession, 336)["complete"].toBool(),
              "SDK access violation is evidence of failure, not compatibility");
        auto untrusted = sdkReceipt;
        untrusted.replace("\"value\":1}", "\"value\":1,\"secret\":\"private-test-payload\"}");
        const auto untrustedResult = fol::analyzeSdkProbe(untrusted, sdkSession, 336);
        check(
            !untrustedResult["complete"].toBool() &&
                !QJsonDocument(untrustedResult).toJson().contains("private-test-payload") &&
                !fol::analyzeSdkProbe(sdkReceipt.chopped(1), sdkSession, 336)["complete"]
                     .toBool() &&
                !fol::analyzeSdkProbe(QByteArray(16385, 'x'), sdkSession, 336)["complete"].toBool(),
            "SDK schema payloads, truncation and oversized input fail closed without echo");
        auto empty = fol::analyzeGameLog({});
        check(empty["engine_version"] == "unknown" && empty["evidence"].toArray().isEmpty() &&
                  !empty["manual_sign_in_logged"].toBool(),
              "empty logs cannot establish startup");
        const QByteArray fixture =
            "\xef\xbb\xbfLogInit: Engine Version: 4.25.0-123+++Fortnite+Release-12.50\r\n"
            "LogInit: Compatible Engine Version: 4.25.0-122+++Fortnite+Release-12.50\n"
            "[2026.09.29-18.53.50:100][  0]LogOnlineGame: Error: Did not find Fortnite "
            "MCPConfig Localhost for service fortnite\n"
            "[2026.09.29-18.53.50:100][  0]LogOnlineGame: Using EpicApp backend 'Localhost' "
            "with host '<unconfigured>' based on environment 'GameDev' and override 'Localhost'\n"
            "[2026.09.29-18.53.50:100][  0]LogOnlineGame: Error: Invalid backend requested! "
            "Exiting.";
        auto blocked = fol::analyzeGameLog(fixture);
        check(blocked["engine_version"] == "4.25.0-123+++Fortnite+Release-12.50" &&
                  blocked["compatible_engine_version"] == "4.25.0-122+++Fortnite+Release-12.50",
              "engine and compatibility metadata remain separate");
        check(blocked["backend_configuration_missing_logged"].toBool() &&
                  blocked["backend_rejected_logged"].toBool() &&
                  !blocked["manual_sign_in_logged"].toBool(),
              "missing private configuration is not sign-in success");
        check(blocked["last_backend_selection"].toObject()["host"] == "<unconfigured>" &&
                  blocked["evidence"].toArray().last().toObject()["line"] == 5,
              "backend evidence includes source line with unterminated final line");
        const QByteArray signIn = "LogFortLogin: Focusing widget SignIn_Credentials\n";
        check(fol::analyzeGameLog(signIn)["manual_sign_in_logged"].toBool(),
              "recognized focus diagnostic reports logged sign-in only");
        check(
            fol::analyzeGameLog("LogUIActionRouter: Display: Applying input config for "
                                "leaf-most node [SignIn_Credentials]\r\n")["manual_sign_in_logged"]
                .toBool(),
            "input configuration diagnostic recognized");
        auto mixed = fol::analyzeGameLog(signIn + fixture);
        check(mixed["manual_sign_in_logged"].toBool() &&
                  mixed["interpretation"] == "backend rejection logged; inspect configuration",
              "earlier sign-in marker never suppresses backend rejection");
        const QByteArray loginAbort =
            "[2026.09.29-21.37.06:230][ 98]LogOnlineAccount: Warning: "
            "[UOnlineAccountCommon::AbortLoggingIn] Aborting OnlineAccountCommon login due to "
            "error [CurrentLoginStep=LoggingIn] [LoginResult=13] "
            "[DisplayError=synthetic-private-error] [DetailedError=synthetic-secret]\r\n";
        const auto failedLogin = fol::analyzeGameLog(loginAbort + signIn);
        check(failedLogin["login_aborted_logged"].toBool() &&
                  failedLogin["last_logged_login_failure"].toObject()["stage"] == "LoggingIn" &&
                  failedLogin["last_logged_login_failure"].toObject()["result"] == 13 &&
                  failedLogin["evidence"].toArray()[0].toObject()["line"] == 1,
              "actual login abort shape retains stage, result and source line");
        check(failedLogin["manual_sign_in_logged"].toBool() &&
                  failedLogin["interpretation"] ==
                      "login abort logged; successful login unverified",
              "manual sign-in after failure does not imply authenticated login");
        check(!QJsonDocument(failedLogin).toJson().contains("synthetic-private-error") &&
                  !QJsonDocument(failedLogin).toJson().contains("synthetic-secret"),
              "login evidence excludes arbitrary error payloads");
        check(!fol::analyzeGameLog("chat: " + loginAbort)["login_aborted_logged"].toBool() &&
                  !empty["login_aborted_logged"].toBool(),
              "embedded abort strings and missing evidence are not login failures");
        check(fol::analyzeGameLog(loginAbort + fixture)["interpretation"] ==
                  "backend rejection logged; inspect configuration",
              "backend configuration rejection retains precedence");
        const QByteArray keychainFailure =
            "[2026.09.29-22.25.21:678][132]LogKeychainHelper: [FMcpKeychainHelper] Unable to "
            "parse json, failed download\r\n"
            "LogKeychainHelper: Warning: Unable to refresh keychain: Failure "
            "ErrorCode=UnableToParseJson, Message=, Raw=UnableToParseJson (HTTP 200)\n";
        auto keychainAbort = loginAbort;
        keychainAbort.replace("CurrentLoginStep=LoggingIn", "CurrentLoginStep=QueryKeychain");
        keychainAbort.replace("LoginResult=13", "LoginResult=37");
        const auto keychain = fol::analyzeGameLog(keychainFailure + keychainAbort);
        check(keychain["asset_keychain_parse_failure_logged"].toBool() &&
                  keychain["asset_keychain_http_200_parse_failure_logged"].toBool() &&
                  keychain["last_logged_login_failure"].toObject()["result"] == 37 &&
                  keychain["evidence"].toArray()[1].toObject()["line"] == 2,
              "actual keychain diagnostics preserve bounded evidence and source lines");
        check(keychain["interpretation"] ==
                  "asset-keychain login abort logged; parser requirement unresolved",
              "keychain failure is not attributed to a bad password or a proven key requirement");
        check(!fol::analyzeGameLog(keychainFailure)["login_aborted_logged"].toBool() &&
                  !empty["asset_keychain_parse_failure_logged"].toBool() &&
                  !empty["asset_keychain_http_200_parse_failure_logged"].toBool(),
              "parse observation alone does not establish login abort or HTTP status");
        check(fol::analyzeGameLog(keychainFailure + keychainAbort + loginAbort)["interpretation"] ==
                  "login abort logged; successful login unverified",
              "historical keychain failure does not override the most recent abort stage");
        check(fol::analyzeGameLog(keychainFailure + keychainAbort + fixture)["interpretation"] ==
                  "backend rejection logged; inspect configuration",
              "backend configuration rejection still takes precedence over keychain abort");
        auto keychainSpoof = keychainFailure;
        keychainSpoof.replace("LogKeychainHelper:", "chat: LogKeychainHelper:");
        check(fol::analyzeGameLog(keychainSpoof)["evidence"].toArray().isEmpty(),
              "embedded keychain diagnostic strings are not recognized");
        auto privateKeychainError = keychainFailure;
        privateKeychainError.replace("Message=,", "Message=synthetic-private-payload,");
        const auto privateKeychain = fol::analyzeGameLog(privateKeychainError);
        check(!privateKeychain["asset_keychain_http_200_parse_failure_logged"].toBool() &&
                  !QJsonDocument(privateKeychain).toJson().contains("synthetic-private-payload"),
              "unrecognized keychain error payloads are not echoed or given an HTTP status");
        auto spoof = fol::analyzeGameLog(
            "LogInit: Command Line: LogFortLogin: Focusing widget SignIn_Credentials\n"
            "chat: LogOnlineGame: Error: Invalid backend requested! Exiting.\n"
            "LogFortLogin: Focusing widget SignIn_CredentialsExtra\n"
            "LogOnlineGame: Using EpicApp backend 'Localhost' with host 'user:secret@localhost' "
            "based on environment 'GameDev' and override ''\n"
            "LogHttp: Authorization: synthetic-secret\n");
        check(spoof["evidence"].toArray().isEmpty() &&
                  !QJsonDocument(spoof).toJson().contains("synthetic-secret"),
              "unrelated substrings, credential URLs and payloads excluded");
        const auto maps = fol::analyzeGameLog(
            "[2026.09.30-02.44.30:000][  0]LogLoad: LoadMap: "
            "/Game/Maps/Frontend?Name=Player&token=private-map-option\n"
            "[2026.09.30-02.44.30:001][  0]LogWorld: Bringing World /Game/Maps/Frontend.Frontend "
            "up for play (max tick rate 0) at 2026.09.30-02.44.30\n"
            "LogLoad: LoadMap: /Game/Athena/Apollo/Maps/Apollo_Terrain\n"
            "LogInit: Command Line: LogWorld: Bringing World /Game/Maps/Fake up for play (max tick "
            "rate 0) at 2026.09.30-02.44.30\n");
        check(maps["worlds_brought_up"].toArray() == QJsonArray{"/Game/Maps/Frontend"},
              "map requests and spoofed command lines do not establish a brought-up world");
        check(maps["matched_lines"] == 3 &&
                  !QJsonDocument(maps).toJson().contains("private-map-option"),
              "world diagnostics discard all map URL options");
        auto repeated = fol::analyzeGameLog(signIn.repeated(150));
        check(repeated["evidence"].toArray().size() == 128 &&
                  repeated["evidence_truncated"].toBool() && repeated["matched_lines"] == 150,
              "repeated evidence is capped and truncation reported");
        auto oversized = fol::analyzeGameLog(QByteArray(4097, 'x') + '\n' + signIn);
        check(oversized["oversized_lines_skipped"] == 1 &&
                  oversized["evidence"].toArray()[0].toObject()["line"] == 2,
              "long lines skipped without losing line numbers");
        bool rejected = false;
        try {
            fol::analyzeGameLog(QByteArray(64 * 1024 * 1024 + 1, 'x'));
        } catch (const fol::Error &) {
            rejected = true;
        }
        check(rejected, "analysis rejects oversized input");
        QTemporaryDir temporary;
        check(temporary.isValid(), "temporary fixture");
        const auto path = temporary.path() + "/game.log";
        fol::writeFile(path, fixture);
        const auto cli = QCoreApplication::applicationDirPath() + "/fortnite-linux";
        auto command = fol::run(cli, {"logs", "analyze", path});
        check(command.exitCode == 0 && QJsonDocument::fromJson(command.output.toUtf8())
                                           .object()["backend_rejected_logged"]
                                           .toBool(),
              "CLI analyzes historical logs without running the game");
        check(fol::run(cli, {"logs", "analyze", temporary.path()}).exitCode != 0 &&
                  fol::run(cli, {"logs", "analyze", path + ".missing"}).exitCode != 0,
              "CLI rejects directory and missing file");
        const auto prefix = temporary.path() + "/prefix";
        fol::writeFile(prefix + "/drive_c/users/test/AppData/Local/FortniteGame/Saved/Logs/"
                                "FortniteGame.log",
                       fixture);
        auto captured = fol::captureGameLogs(prefix, temporary.path() + "/session",
                                             QDateTime::currentDateTimeUtc().addSecs(-1));
        check(captured["files"]
                  .toArray()[0]
                  .toObject()["observations"]
                  .toObject()["backend_rejected_logged"]
                  .toBool(),
              "automatic capture includes structured game evidence");
        std::cout << checks << " game evidence checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
