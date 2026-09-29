#include "fol/core.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QtEndian>

static int checks = 0;
static void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw fol::Error(QString("Test failed: ") + message);
}
template <typename F> static void rejects(F fn, const char *message) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, message);
}
static QByteArray syntheticPe(bool dll = false) {
    QByteArray bytes(1024, '\0');
    bytes.replace(0, 2, "MZ");
    qToLittleEndian<quint32>(64, bytes.data() + 60);
    bytes.replace(64, 4, QByteArray("PE\0\0", 4));
    qToLittleEndian<quint16>(0x8664, bytes.data() + 68);
    qToLittleEndian<quint16>(1, bytes.data() + 70);
    qToLittleEndian<quint16>(240, bytes.data() + 84);
    qToLittleEndian<quint16>(dll ? 0x2002 : 2, bytes.data() + 86);
    qToLittleEndian<quint16>(0x20b, bytes.data() + 88);
    qToLittleEndian<quint32>(512, bytes.data() + 344);
    qToLittleEndian<quint32>(512, bytes.data() + 348);
    bytes.replace(512, 46, "4.25.0-13498980+++Fortnite+Release-12.61-CL-13498");
    return bytes;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir temp;
        check(temp.isValid(), "temporary directory");
        const QByteArray moduleReceipt =
            "{\"event\":\"module-ready\",\"session_id\":\"module-test\",\"process_id\":42}\n";
        check(fol::analyzeModuleReceipt(moduleReceipt, "module-test", 42)["ready"].toBool(),
              "module receipt proves matching session and owned child only");
        check(!fol::analyzeModuleReceipt(moduleReceipt, "stale", 42)["ready"].toBool(),
              "stale module session rejected");
        check(!fol::analyzeModuleReceipt(moduleReceipt, "module-test", 43)["ready"].toBool(),
              "different module process rejected");
        check(!fol::analyzeModuleReceipt({}, "module-test", 42)["ready"].toBool(),
              "missing module receipt cannot count as loaded");
        check(!fol::analyzeModuleReceipt(moduleReceipt.chopped(1), "module-test", 42)["ready"]
                   .toBool(),
              "truncated module receipt rejected");
        check(
            !fol::analyzeModuleReceipt(QByteArray(4097, 'x'), "module-test", 42)["ready"].toBool(),
            "oversized module receipt rejected");
        const QByteArray moduleObservation =
            "{\"event\":\"module-observation\",\"session_id\":\"module-test\",\"process_id\":42,"
            "\"installed\":true,\"restored\":true,\"rejected\":0,\"error\":\"none\","
            "\"requests\":[{\"origin\":\"loopback\",\"route\":\"oauth-token\"}]}\n";
        check(fol::analyzeModuleObservation(moduleObservation, "module-test", 42)["observed"]
                  .toBool(),
              "restored session-bound module observation accepted");
        check(!fol::analyzeModuleObservation(moduleObservation, "stale", 42)["observed"].toBool(),
              "stale observation rejected");
        check(!fol::analyzeModuleObservation(moduleObservation, "module-test", 43)["observed"]
                   .toBool(),
              "wrong observation child rejected");
        check(!fol::analyzeModuleObservation({}, "module-test", 42)["observed"].toBool(),
              "missing observation rejected");
        check(!fol::analyzeModuleObservation(moduleObservation.chopped(1), "module-test",
                                             42)["observed"]
                   .toBool(),
              "truncated observation rejected");
        for (const auto &pair : QList<QPair<QByteArray, QByteArray>>{
                 {"\"restored\":true", "\"restored\":false"},
                 {"\"rejected\":0", "\"rejected\":1"},
                 {"\"error\":\"none\"", "\"error\":\"no-requests\""},
                 {"\"route\":\"oauth-token\"", "\"route\":\"sensitive-text\""},
                 {"\"origin\":\"loopback\"", "\"origin\":\"loopback\",\"query\":\"secret\""}}) {
            auto invalid = moduleObservation;
            invalid.replace(pair.first, pair.second);
            const auto result = fol::analyzeModuleObservation(invalid, "module-test", 42);
            check(!result["observed"].toBool() &&
                      !QJsonDocument(result).toJson().contains("secret"),
                  "invalid or non-redacted module observation cannot report success");
        }
        const QByteArray receipt =
            "FOL_HTTP_PROBE {\"event\":\"started\",\"session_id\":\"test\"}\n"
            "FOL_HTTP_PROBE {\"event\":\"owned_child_started\",\"value\":42}\n"
            "FOL_HTTP_PROBE {\"event\":\"interface_candidate_observed\",\"value\":1}\n"
            "FOL_HTTP_PROBE {\"event\":\"completed\",\"value\":0}\n";
        const auto receiptOk = fol::analyzeHttpProbe(receipt, "test");
        check(receiptOk["complete"].toBool() && receiptOk["candidate_observed"].toBool(),
              "session-bound helper completion receipt");
        check(fol::analyzeHttpProbe({}, "test").contains("error"), "missing helper receipt");
        check(fol::analyzeHttpProbe(receipt, "other").contains("error"), "stale helper receipt");
        check(fol::analyzeHttpProbe(receipt.chopped(1), "test").contains("error"),
              "truncated helper receipt");
        check(fol::analyzeHttpProbe(receipt + receipt, "test").contains("error"),
              "duplicate helper completion");
        check(fol::analyzeHttpProbe(QByteArray(65537, 'x'), "test").contains("error"),
              "oversized helper receipt");
        auto absent = receipt;
        absent.replace("observed\",\"value\":1", "observed\",\"value\":0");
        absent.replace("completed\",\"value\":0", "completed\",\"value\":67");
        const auto noCandidate = fol::analyzeHttpProbe(absent, "test");
        check(noCandidate["complete"].toBool() && !noCandidate["candidate_observed"].toBool(),
              "completed discovery can find no candidate");
        auto tableReceipt = receipt;
        const QByteArray table =
            "FOL_HTTP_PROBE {\"event\":\"pointer_table\",\"reference_rva\":800,"
            "\"start_rva\":800,\"candidate_slot\":0,\"scan_limit_reached\":false,"
            "\"function_rvas\":[4096,4144],\"table_address_xrefs\":[]}\n";
        tableReceipt.insert(tableReceipt.indexOf("FOL_HTTP_PROBE {\"event\":\"interface"), table);
        check(fol::analyzeHttpProbe(tableReceipt, "test")["pointer_tables"].toArray().size() == 1,
              "bounded pointer table evidence");
        tableReceipt.replace("\"candidate_slot\":0", "\"candidate_slot\":2");
        check(fol::analyzeHttpProbe(tableReceipt, "test").contains("error"),
              "pointer table rejects out of bounds candidate slot");
        auto codeReceipt = receipt;
        QByteArray codeRecord =
            "FOL_HTTP_PROBE {\"event\":\"code_observation\",\"function_rva\":4096,"
            "\"table_slot\":0,\"bytes\":\"" +
            QByteArray(256, 'a') + "\"}\n";
        codeReceipt.insert(codeReceipt.indexOf("FOL_HTTP_PROBE {\"event\":\"interface"),
                           codeRecord);
        const auto codeReport = fol::analyzeHttpProbe(codeReceipt, "test");
        check(codeReport["complete"].toBool() && codeReport["code_observations"].toInt() == 1 &&
                  !QJsonDocument(codeReport).toJson().contains(QByteArray(256, 'a')),
              "code observations counted without propagating code into launch summary");
        codeReceipt.replace(QByteArray(256, 'a'), "invalid");
        check(fol::analyzeHttpProbe(codeReceipt, "test").contains("error"),
              "malformed code sample rejected");
        auto observedReceipt = receipt;
        const QByteArray observed =
            "FOL_HTTP_PROBE {\"event\":\"debug_attached\",\"value\":42}\n"
            "FOL_HTTP_PROBE {\"event\":\"request_observed\",\"origin\":\"epic\","
            "\"route\":\"oauth-token\"}\n"
            "FOL_HTTP_PROBE {\"event\":\"request_observation_completed\",\"value\":1}\n";
        observedReceipt.insert(observedReceipt.indexOf("FOL_HTTP_PROBE {\"event\":\"completed"),
                               observed);
        const auto observedReport = fol::analyzeHttpProbe(observedReceipt, "test");
        check(observedReport["complete"].toBool() &&
                  observedReport["request_observation_completed"].toBool() &&
                  observedReport["requests"].toArray().size() == 1,
              "observer receipt reports fixed request categories");
        auto rawRequest = observedReceipt;
        rawRequest.replace("oauth-token", "arbitrary-secret-query");
        check(fol::analyzeHttpProbe(rawRequest, "test").contains("error"),
              "arbitrary request detail is not a valid category");
        auto noObservation = observedReceipt;
        noObservation.replace(
            "FOL_HTTP_PROBE {\"event\":\"request_observation_completed\",\"value\":1}\n", "");
        check(fol::analyzeHttpProbe(noObservation, "test").contains("error"),
              "observer requires completion before helper completion");
        auto failedObservation = observedReceipt;
        failedObservation.replace("observation_completed\",\"value\":1",
                                  "observation_completed\",\"value\":0");
        check(fol::analyzeHttpProbe(failedObservation, "test").contains("error"),
              "failed observation cannot be interpreted as success");
        auto holdReceipt = receipt;
        holdReceipt.insert(
            holdReceipt.indexOf("FOL_HTTP_PROBE {\"event\":\"completed"),
            "FOL_HTTP_PROBE {\"event\":\"observer_control\",\"value\":1}\n"
            "FOL_HTTP_PROBE {\"event\":\"observer_control_child_live\",\"value\":1}\n");
        holdReceipt.replace("completed\",\"value\":0", "completed\",\"value\":67");
        const auto holdReport = fol::analyzeHttpProbe(holdReceipt, "test");
        check(holdReport["complete"].toBool() && holdReport["candidate_observed"].toBool() &&
                  !holdReport["request_observation_completed"].toBool(),
              "control completion is not request observation success");
        auto attachReceipt = receipt;
        attachReceipt.insert(
            attachReceipt.indexOf("FOL_HTTP_PROBE {\"event\":\"completed"),
            "FOL_HTTP_PROBE {\"event\":\"observer_control\",\"value\":2}\n"
            "FOL_HTTP_PROBE {\"event\":\"debug_attached\",\"value\":42}\n"
            "FOL_HTTP_PROBE {\"event\":\"request_observation_completed\",\"value\":0}\n");
        attachReceipt.replace("completed\",\"value\":0", "completed\",\"value\":67");
        // The observation flag remains zero; only the helper completion is67.
        attachReceipt.replace("observation_completed\",\"value\":67",
                              "observation_completed\",\"value\":0");
        const auto attachReport = fol::analyzeHttpProbe(attachReceipt, "test");
        check(attachReport["complete"].toBool() &&
                  !attachReport["request_observation_completed"].toBool(),
              "attach-only control cannot claim a captured request");
        auto root = temp.path() + "/game with spaces";
        auto profile = fol::Profile::load();
        check(profile.version == "12.61", "12.61 remains default profile");
        auto diagnosticProfile = fol::Profile::forVersion("12.50");
        check(diagnosticProfile && diagnosticProfile->version == "12.50" &&
                  !diagnosticProfile->privatePathVerified,
              "12.50 has separate diagnostic-only profile");
        check(!fol::Profile::forVersion("12.60") && !fol::Profile::forVersion("../../12.61"),
              "unknown and traversal profiles rejected");
        const auto logPrefix = temp.path() + "/log-prefix";
        const auto gameLog =
            logPrefix +
            "/drive_c/users/test/AppData/Local/FortniteGame/Saved/Logs/FortniteGame.log";
        const auto captureStart = QDateTime::currentDateTimeUtc().addSecs(-1);
        fol::writeFile(gameLog, "synthetic game log\n");
        auto gameCapture = fol::captureGameLogs(logPrefix, temp.path() + "/captured", captureStart);
        check(gameCapture["status"] == "captured" &&
                  fol::readFile(temp.path() + "/captured/fortnite.log") == "synthetic game log\n",
              "current prefix-local game log is preserved in the session");
        rejects([&] { fol::captureGameLogs(logPrefix, temp.path() + "/captured", captureStart); },
                "game capture never overwrites an existing session log");
        QFile staleLog(gameLog);
        check(
            staleLog.open(QIODevice::ReadWrite) &&
                staleLog.setFileTime(captureStart.addSecs(-60), QFileDevice::FileModificationTime),
            "stale game log fixture timestamp");
        staleLog.close();
        auto staleCapture = fol::captureGameLogs(logPrefix, temp.path() + "/stale", captureStart);
        check(staleCapture["files"].toArray()[0].toObject()["status"] ==
                      "skipped: predates launch" &&
                  !QFile::exists(temp.path() + "/stale/fortnite.log"),
              "old game log is not attributed to a new launch");
        check(QFile::remove(gameLog), "remove owned stale fixture");
        fol::writeFile(temp.path() + "/outside-log", "must not capture\n");
        check(QFile::link(temp.path() + "/outside-log", gameLog), "symlink log fixture");
        auto unsafeCapture = fol::captureGameLogs(logPrefix, temp.path() + "/unsafe", captureStart);
        check(unsafeCapture["files"].toArray()[0].toObject()["status"] == "skipped: unsafe path",
              "game log symlink outside prefix is not read");
        check(QFile::remove(gameLog), "remove owned log symlink");
        QFile largeLog(gameLog);
        check(largeLog.open(QIODevice::WriteOnly) && largeLog.resize(64 * 1024 * 1024 + 1),
              "oversize sparse log fixture");
        largeLog.close();
        auto largeCapture = fol::captureGameLogs(logPrefix, temp.path() + "/large", captureStart);
        check(largeCapture["files"].toArray()[0].toObject()["status"] ==
                  "skipped: exceeds 64 MiB limit",
              "game log capture enforces bounded reads");
        auto exe = root + "/" + profile.executable;
        fol::writeFile(exe, syntheticPe());
        QDir().mkpath(root + "/Engine");
        fol::writeFile(root + "/FortniteGame/Content/Paks/synthetic.pak", QByteArray(128, 'x'));
        auto build = fol::inspectBuild(root);
        check(build.valid() && build.version == "12.61" && build.engine == "4.25.0",
              "ASCII version metadata");
        auto older = syntheticPe();
        older.replace("Release-12.61", "Release-12.50");
        fol::writeFile(exe, older);
        auto olderBuild = fol::inspectBuild(root);
        check(olderBuild.version == "12.50" &&
                  olderBuild.json()["compatibility_profile"] == "12.50",
              "12.50 is not aliased to 12.61");
        fol::writeFile(exe, syntheticPe());
        check(fol::discoverBuilds({temp.path()}).contains(root),
              "discovery finds structural build");
        auto integrityManifest = temp.path() + "/integrity.toml";
        auto pakHash =
            QCryptographicHash::hash(QByteArray(128, 'x'), QCryptographicHash::Sha256).toHex();
        fol::writeFile(integrityManifest,
                       "[files]\n\"FortniteGame/Content/Paks/synthetic.pak\" = \"" + pakHash +
                           "\"\n");
        check(fol::verifyIntegrity(root, integrityManifest)["valid"].toBool(),
              "content hash verification");
        fol::writeFile(root + "/FortniteGame/Content/Paks/synthetic.pak", QByteArray(128, 'y'));
        check(!fol::verifyIntegrity(root, integrityManifest)["valid"].toBool(),
              "content corruption detected");
        fol::writeFile(integrityManifest, "[files]\n\"../outside\" = \"" + pakHash + "\"\n");
        rejects([&] { fol::verifyIntegrity(root, integrityManifest); },
                "integrity traversal rejected");
        fol::writeFile(temp.path() + "/outside", "test");
        QFile::link(temp.path() + "/outside", root + "/linked-file");
        fol::writeFile(integrityManifest, "[files]\n\"linked-file\" = \"" + pakHash + "\"\n");
        rejects([&] { fol::verifyIntegrity(root, integrityManifest); },
                "integrity symlink escape rejected");
        auto pe = syntheticPe();
        pe.truncate(700);
        fol::writeFile(exe, pe);
        check(!fol::inspectPe(exe).valid, "truncated PE section rejected");
        pe = syntheticPe();
        qToLittleEndian<quint16>(0x14c, pe.data() + 68);
        fol::writeFile(exe, pe);
        check(!fol::inspectBuild(root).valid(), "wrong architecture rejected");
        pe = syntheticPe();
        pe.replace(512, 200, QByteArray(200, '\0'));
        QString metadata = "4.25.0-13498980+++Fortnite+Release-12.61";
        QByteArray utf16;
        for (auto c : metadata) {
            utf16.append(char(c.unicode()));
            utf16.append('\0');
        }
        pe.replace(512, utf16.size(), utf16);
        fol::writeFile(exe, pe);
        check(fol::inspectBuild(root).version == "12.61", "UTF-16LE metadata");
        auto boundary = syntheticPe();
        boundary.replace(512, 200, QByteArray(200, '\0'));
        boundary.append(QByteArray(1024 * 1024 + 128 - boundary.size(), '\0'));
        QByteArray release = "++Fortnite+Release-12.61-CL-13498980";
        auto split = release.indexOf("61") + 1;
        boundary.replace(1024 * 1024 - split, release.size(), release);
        fol::writeFile(exe, boundary);
        check(fol::inspectBuild(root).version == "12.61", "version split across read boundary");
        fol::writeFile(exe, pe);
        fol::writeFile(root + "/Engine/Build/Build.version",
                       "{\"BranchName\":\"++Fortnite+Release-14.60\"}");
        check(!fol::inspectBuild(root).valid(), "conflicting versions rejected");
        QFile::remove(root + "/Engine/Build/Build.version");
        fol::Paths paths{temp.path() + "/config", temp.path() + "/data", temp.path() + "/cache",
                         temp.path() + "/state"};
        fol::Registry registry(paths);
        registry.add("12.61", root);
        check(registry.resolve("12.61") == root, "registry roundtrip");
        registry.add("alternate", root);
        registry.remove("alternate");
        check(QFile::exists(exe), "unregister preserves files");
        rejects([&] { paths.prefix("../unrelated"); }, "prefix traversal rejected");
        QDir().mkpath(paths.data + "/prefixes");
        QFile::link(root, paths.prefix("linked"));
        rejects([&] { fol::prefixStatus(paths, "linked"); }, "prefix symlink rejected");
        check(fol::isEacName("C:\\Game\\EasyAntiCheat_EOS.exe"), "EAC Windows path");
        check(fol::isEacName("FortniteClient-Win64-Shipping_EAC.exe"), "EAC bootstrap");
        check(!fol::isEacName("FortniteClient-Win64-Shipping.exe"), "ordinary shipping is not EAC");
        check(fol::isBattlEyeName("C:\\Game\\BEService_x64.exe") &&
                  fol::isBattlEyeName("FortniteClient-Win64-Shipping_BE.exe"),
              "BattlEye bootstrap/service detection");
        check(!fol::isBattlEyeName("FortniteClient-Win64-Shipping.exe"),
              "ordinary shipping is not BattlEye");
        check(
            !fol::antiCheatModuleEvidence(
                 "trace:loaddll:build_module Loaded L\"C:\\Game\\EasyAntiCheat_x64.dll\" at 0x10\n")
                 .isEmpty(),
            "EAC module-load trace evidence detected");
        check(!fol::antiCheatModuleEvidence(
                   "trace:loaddll:build_module Loaded L\"C:\\Game\\BEClient_x64.dll\" at 0x10\n")
                   .isEmpty(),
              "BattlEye module-load trace evidence detected");
        check(
            fol::antiCheatModuleEvidence("Missing EasyAntiCheat_x64.dll; not a loaded-module trace")
                .isEmpty(),
            "mention of anti-cheat is not treated as module load");
        check(fol::quote("a'b") == "'a'\\''b'", "shell escaping");
        const auto ipc = fol::localIpcReport();
        check(ipc["family"] == "AF_UNIX" && !ipc["operation"].toString().isEmpty(),
              "IPC probe identifies family and exact operation");
        check(ipc["functional"].toBool() ? ipc["errno"] == 0 : !ipc["error"].toString().isEmpty(),
              "IPC probe reports real success or explicit failure");
        check(!ipc.contains("cleanup_error") && !ipc.contains("close_error"),
              "IPC probe cleans its own resources");
        for (bool ipv6 : {false, true}) {
            auto capability = fol::udpSocketCreationReport(ipv6);
            check(capability["created"].toBool() == (capability["errno"].toInt(-1) == 0),
                  "socket capability distinguishes actual failure from success");
            check(capability["family"] == (ipv6 ? "IPv6" : "IPv4") &&
                      capability["operation"] == "socket",
                  "socket capability reports exact operation and family");
        }
        check(fol::classifyFailure("err:module:import_dll missing") == "missing-library",
              "DLL failure classification");
        check(fol::classifyFailure("nothing informative") == "unknown; inspect logs",
              "no invented crash cause");
        check(fol::classifyFailure("info: DXVK version 2.0\nerr:wineboot: service failed") ==
                  "unknown; inspect logs",
              "unrelated Wine error cannot implicate DXVK");
        check(fol::classifyFailure("trace:loaddll: vkd3d.dll loaded\nerr:rpc: unavailable") ==
                  "unknown; inspect logs",
              "module presence cannot implicate VKD3D");
        check(fol::classifyFailure("info: DXVK override configured without errors") ==
                  "unknown; inspect logs",
              "error substring is not a severity marker");
        check(fol::classifyFailure("err: vkd3d: device initialization failed") == "vkd3d",
              "component-scoped VKD3D error evidence");
        check(fol::classifyFailure("error: DXVK device initialization failed") == "dxvk",
              "component-scoped DXVK error evidence");
        auto launch = fol::launch(paths, "missing", false, false, 1);
        auto noBootstrap = fol::launch(paths, "missing", true, true, 1, false, false, {}, true,
                                       fol::BackendProbe::None, {}, "module.dll");
        check(noBootstrap["error"].toString().contains("requires the owned-child"),
              "module cannot launch without explicit bootstrap");
        check(launch["status"] == "blocked" &&
                  QFile::exists(launch["logs"].toString() + "/result.json"),
              "failed launch persists session evidence");
        auto blocked = fol::launch(paths, "12.61", false, false, 1);
        check(blocked["status"] == "blocked", "unverified private path blocked");
        auto contextOnline = fol::launch(paths, "12.61", true, false, 1, false, false, {}, true);
        check(contextOnline["error"].toString().contains("requires --offline-probe"),
              "launcher-context experiment cannot run without explicit network isolation");
        auto contextUnknown = fol::launch(paths, "12.61", true, true, 1, false, false, {}, true);
        check(contextUnknown["error"].toString().contains("No launcher-context experiment"),
              "12.50 launcher-context experiment does not silently apply to 12.61");
        auto backendNoContext = fol::launch(paths, "12.61", true, true, 1, false, false, {}, false,
                                            fol::BackendProbe::Selector);
        check(backendNoContext["error"].toString().contains("requires --offline-probe and"),
              "local backend experiment requires explicit context and isolation");
        auto backendNoIsolation = fol::launch(paths, "12.61", true, false, 1, false, false, {},
                                              true, fol::BackendProbe::Selector);
        check(backendNoIsolation["error"].toString().contains("requires --offline-probe and"),
              "local backend experiment rejects non-isolated use");
        auto backendWrongBuild = fol::launch(paths, "12.61", true, true, 1, false, false, {}, true,
                                             fol::BackendProbe::Selector);
        check(backendWrongBuild["error"].toString().contains("No local-backend experiment"),
              "12.50 backend discovery does not silently apply to 12.61");
        auto iniWrongBuild = fol::launch(paths, "12.61", true, true, 1, false, false, {}, true,
                                         fol::BackendProbe::ServiceIni);
        check(iniWrongBuild["error"].toString().contains("No local-backend experiment"),
              "service INI experiment remains version scoped");
        auto iniNoIsolation = fol::launch(paths, "12.61", true, false, 1, false, false, {}, true,
                                          fol::BackendProbe::ServiceIni);
        check(iniNoIsolation["error"].toString().contains("requires --offline-probe and"),
              "service INI experiment requires network isolation");
        const auto helperPath = temp.path() + "/http helper.exe";
        fol::writeFile(helperPath, syntheticPe());
        auto helperOnline = fol::launch(paths, "12.61", true, false, 1, false, false, {}, true,
                                        fol::BackendProbe::None, helperPath);
        check(helperOnline["error"].toString().contains("requires isolated launcher context"),
              "HTTP helper requires network isolation");
        auto helperWrongBuild = fol::launch(paths, "12.61", true, true, 1, false, false, {}, true,
                                            fol::BackendProbe::None, helperPath);
        check(helperWrongBuild["error"].toString().contains("not enabled for this build"),
              "HTTP helper must be enabled by the detected build profile");
        auto helperMixed = fol::launch(paths, "12.61", true, true, 1, false, false, {}, true,
                                       fol::BackendProbe::ServiceIni, helperPath);
        check(helperMixed["error"].toString().contains("no backend experiment"),
              "read-only helper is separate from routing experiments");
        fol::writeFile(temp.path() + "/module.dll", syntheticPe(true));
        check(fol::moduleReport(temp.path() + "/module.dll", {})["status"] == "unverified",
              "architecture is not module compatibility");
        auto checksum =
            QCryptographicHash::hash(syntheticPe(true), QCryptographicHash::Sha256).toHex();
        auto manifest = temp.path() + "/module.toml";
        fol::writeFile(manifest, "fortnite_version = \"12.61\"\nsha256 = \"" + checksum + "\"\n");
        check(fol::moduleReport(temp.path() + "/module.dll", manifest)["status"]
                  .toString()
                  .startsWith("manifest-matches"),
              "matching module manifest");
        check(fol::moduleReport(temp.path() + "/module.dll", manifest, "12.50")["status"] ==
                  "invalid",
              "12.61 module manifest cannot authorize 12.50");
        fol::writeFile(manifest, "fortnite_version = \"12.50\"\nsha256 = \"" + checksum + "\"\n");
        check(fol::moduleReport(temp.path() + "/module.dll", manifest, "12.50")["status"]
                  .toString()
                  .startsWith("manifest-matches"),
              "module manager accepts explicit 12.50 target without defaulting to 12.61");
        check(fol::moduleReport(temp.path() + "/module.dll", manifest)["status"] == "invalid",
              "default module validation still targets 12.61");
        fol::writeFile(manifest, "fortnite_version = \"12.61\"\nsha256 = \"" + checksum + "\"\n");
        auto changed = syntheticPe(true);
        changed[800] = 'x';
        fol::writeFile(temp.path() + "/module.dll", changed);
        check(fol::moduleReport(temp.path() + "/module.dll", manifest)["status"] == "invalid",
              "corrupted module hash rejected");

        auto cliEnvironment = QProcessEnvironment::systemEnvironment();
        cliEnvironment.insert("XDG_CONFIG_HOME", temp.path() + "/cli/config");
        cliEnvironment.insert("XDG_DATA_HOME", temp.path() + "/cli/data");
        cliEnvironment.insert("XDG_STATE_HOME", temp.path() + "/cli/state");
        auto cli = QCoreApplication::applicationDirPath() + "/fortnite-linux";
        auto invoke = [&](const QStringList &args) {
            return fol::run(cli, args, 5000, cliEnvironment);
        };
        auto added = invoke({"add", root, "--id", "test-build"});
        check(added.exitCode == 0 && added.output.contains("12.61"), "CLI add with spaced path");
        check(invoke({"verify", "test-build"}).exitCode == 0, "CLI verify registered build");
        check(invoke({"builds"}).output.contains("test-build"), "CLI list persists registration");
        check(invoke({"launch", "test-build"}).exitCode == 2,
              "CLI fails closed for unverified private path");
        check(invoke({"launch", "test-build", "--backend-probe-method", "unknown"})
                  .output.contains("Unknown backend probe method"),
              "CLI rejects unknown backend probe methods");
        check(invoke({"launch", "test-build", "--backend-probe-method", "service-ini"})
                  .output.contains("requires --local-backend-probe"),
              "CLI requires explicit local backend probe opt-in");
        check(invoke({"host", "test-build"}).exitCode == 2, "CLI host reports integration blocker");
        check(invoke({"network", "127.0.0.1", "65536"}).exitCode == 2, "CLI invalid port rejected");
        check(invoke({"remove", "test-build"}).exitCode == 0 && QFile::exists(exe),
              "CLI unregister preserves content");
        check(invoke({"verify", "test-build"}).exitCode == 2, "CLI missing build fails");

        auto recoveryPrefix = paths.prefix("recovery");
        QDir().mkpath(recoveryPrefix);
        rejects([&] { fol::backupPrefix(paths, "recovery"); }, "unowned backup rejected");
        fol::writeFile(recoveryPrefix + "/.fol-prefix.json",
                       QJsonDocument(QJsonObject{{"owner", "fortnite-on-linux"},
                                                 {"id", "recovery"},
                                                 {"path", recoveryPrefix}})
                           .toJson());
        fol::writeFile(recoveryPrefix + "/system.reg", "preserved registry");
        QDir().mkpath(recoveryPrefix + "/drive_c/windows");
        QFile::link(temp.path() + "/outside", recoveryPrefix + "/host-link");
        {
            QLockFile heldLock(recoveryPrefix + ".lock");
            check(heldLock.tryLock(), "test prefix lock acquired");
            rejects([&] { fol::backupPrefix(paths, "recovery"); }, "locked backup rejected");
        }
        auto backup = fol::backupPrefix(paths, "recovery");
        auto token = backup["token"].toString();
        check(backup["complete"].toBool() && QFile::exists(recoveryPrefix + "/system.reg"),
              "copy backup preserves active prefix");
        check(QFileInfo(backup["path"].toString() + "/prefix/host-link").isSymLink(),
              "backup preserves rather than follows host symlink");
        rejects([&] { fol::restorePrefix(paths, "recovery", token); },
                "restore never overwrites existing prefix");
        rejects([&] { fol::restorePrefix(paths, "recovery", "../escape"); },
                "restore traversal rejected");
        auto archived = fol::backupPrefix(paths, "recovery", true);
        check(!QFileInfo::exists(recoveryPrefix) && archived["complete"].toBool(),
              "archive moves prefix without deleting content");
        check(fol::prefixBackups(paths, "recovery")["backups"].toArray().size() == 2,
              "backups enumerated");
        auto restored = fol::restorePrefix(paths, "recovery", token);
        check(restored["initialized"].toBool() &&
                  fol::readFile(recoveryPrefix + "/system.reg") == "preserved registry",
              "backup restore preserves registry and ownership");
        check(QFile::exists(archived["path"].toString() + "/prefix/system.reg"),
              "restore does not consume other backups");
        QFile::link(temp.path(), paths.data + "/prefix-backups/linked-storage");
        rejects([&] { fol::prefixBackups(paths, "linked-storage"); },
                "symlink backup storage rejected");
        QProcess recoveryBusy;
        auto recoveryEnv = QProcessEnvironment::systemEnvironment();
        recoveryEnv.insert("WINEPREFIX", recoveryPrefix);
        recoveryBusy.setProcessEnvironment(recoveryEnv);
        recoveryBusy.start("/bin/sleep", {"30"});
        check(recoveryBusy.waitForStarted(), "busy prefix fixture starts");
        rejects([&] { fol::backupPrefix(paths, "recovery"); }, "active prefix backup rejected");
        recoveryBusy.terminate();
        check(recoveryBusy.waitForFinished(1000), "busy prefix fixture stops");

        auto protonSource = temp.path() + "/synthetic proton";
        QDir().mkpath(protonSource + "/files/bin");
        QDir().mkpath(protonSource + "/files/share/default_pfx");
        for (const QString name : {"proton", "files/bin/wine", "files/bin/wineserver"}) {
            check(QFile::copy(QCoreApplication::applicationDirPath() + "/runtime-fixture",
                              protonSource + "/" + name),
                  "synthetic Proton executable copy");
        }
        fol::writeFile(protonSource + "/version", "synthetic-proton-not-real\n");
        auto imported = fol::importProton(paths, "synthetic", protonSource);
        check(fol::importedProtonRuntimes(paths).size() == 1, "imported Proton runtimes listed");
        check(imported["version"] == "synthetic-proton-not-real" &&
                  QFile::exists(protonSource + "/proton"),
              "runtime import preserves source");
        {
            auto dry = fol::prepareProtonSession(paths, "dry-probe", "synthetic", "plan-only",
                                                 paths.state + "/plan-logs", false);
            check(!QDir(dry.compat).exists() && !dry.prefixLock && !dry.runtimeLock,
                  "Proton plan does not create compatibility data or acquire execution locks");
            auto held = fol::prepareProtonSession(paths, "locked-probe", "synthetic", "locked",
                                                  paths.state + "/lock-logs", true);
            rejects(
                [&] {
                    fol::prepareProtonSession(paths, "locked-probe", "synthetic", "second",
                                              paths.state + "/lock-logs", true);
                },
                "Proton session retains prefix and runtime locks through execution scope");
            rejects(
                [&] {
                    fol::prepareProtonSession(paths, "another-probe", "synthetic", "second",
                                              paths.state + "/lock-logs", true);
                },
                "Proton runtime lock also excludes another prefix during setup and execution");
            check(!QDir(paths.data + "/proton-prefixes/another-probe").exists(),
                  "failed Proton runtime lock does not create a new prefix");
        }
        rejects([&] { fol::importProton(paths, "synthetic", protonSource); },
                "runtime import refuses overwrite");
        check(!fol::testProton(paths, "probe", "missing")["passed"].toBool(),
              "missing Proton runtime fails closed");
        auto protonNamespace = fol::run("unshare", {"--user", "--map-root-user", "--net", "true"});
        if (protonNamespace.exitCode == 0) {
            qputenv("STEAM_COMPAT_MEDIA_PATH", "/must-not-use-other-game-media");
            qputenv("PROTON_REMOTE_DEBUG_CMD", "must-not-execute");
            auto proton = fol::testProton(paths, "probe", "synthetic");
            qunsetenv("STEAM_COMPAT_MEDIA_PATH");
            qunsetenv("PROTON_REMOTE_DEBUG_CMD");
            check(proton["passed"].toBool() && proton["execution_receipt_verified"].toBool(),
                  "synthetic Proton verifies execution receipt");
            qputenv("FOL_FIXTURE_MODE", "no-receipt");
            auto empty = fol::testProton(paths, "probe", "synthetic");
            qunsetenv("FOL_FIXTURE_MODE");
            check(!empty["passed"].toBool() && empty["exit_code"].toInt() == 0,
                  "Proton zero exit without current-session receipt is not success");
            qputenv("FOL_FIXTURE_MODE", "eac-name");
            auto eac = fol::testProton(paths, "probe", "synthetic");
            qunsetenv("FOL_FIXTURE_MODE");
            check(!eac["passed"].toBool() && eac["eac_status"].toString().startsWith("detected"),
                  "Proton supervisor stops synthetic EAC-named child");
            check(fol::ownedProcesses(paths.data + "/proton-prefixes/probe/pfx", {}).isEmpty(),
                  "Proton supervisor drains owned processes");
            fol::importProton(paths, "synthetic-second", protonSource);
            check(fol::testProton(paths, "probe", "synthetic-second").contains("error"),
                  "Proton runtime changes require a separate test prefix");

            check(invoke({"runtime", "import-proton", protonSource, "--id", "synthetic-cli"})
                          .exitCode == 0,
                  "CLI Proton runtime import");
            auto cliData = temp.path() + "/cli/data/fortnite-on-linux";
            for (const QString runtimeKind : {"wine", "proton"}) {
                auto prefix = runtimeKind == "wine"
                                  ? cliData + "/prefixes/interrupt-wine"
                                  : cliData + "/proton-prefixes/interrupt-proton/pfx";
                if (runtimeKind == "wine") {
                    fol::writeFile(prefix + "/.fol-prefix.json",
                                   QJsonDocument(QJsonObject{{"owner", "fortnite-on-linux"},
                                                             {"id", "interrupt-wine"},
                                                             {"path", prefix}})
                                       .toJson());
                    fol::writeFile(prefix + "/system.reg", "synthetic registry\n");
                    QDir().mkpath(prefix + "/drive_c/windows");
                }
                auto ready = temp.path() + "/" + runtimeKind + "-interrupt-ready";
                auto interruptEnv = cliEnvironment;
                interruptEnv.insert("FOL_FIXTURE_MODE", "hang");
                interruptEnv.insert("FOL_FIXTURE_READY", ready);
                interruptEnv.insert("PATH",
                                    protonSource + "/files/bin:" + interruptEnv.value("PATH"));
                QProcess interruptedCli;
                interruptedCli.setProcessEnvironment(interruptEnv);
                interruptedCli.setProcessChannelMode(QProcess::MergedChannels);
                QStringList command =
                    runtimeKind == "wine"
                        ? QStringList{"runtime", "test", "interrupt-wine"}
                        : QStringList{"runtime", "test-proton", "interrupt-proton", "--proton",
                                      "synthetic-cli"};
                interruptedCli.start(cli, command);
                check(interruptedCli.waitForStarted(), "interrupt test CLI starts");
                QElapsedTimer readyTimer;
                readyTimer.start();
                while (!QFileInfo::exists(ready) &&
                       interruptedCli.state() != QProcess::NotRunning &&
                       readyTimer.elapsed() < 5000)
                    interruptedCli.waitForFinished(50);
                bool reached = QFileInfo::exists(ready);
                interruptedCli.terminate();
                bool finished = interruptedCli.waitForFinished(7000);
                if (!finished) {
                    interruptedCli.kill();
                    interruptedCli.waitForFinished(1000);
                }
                auto result = QJsonDocument::fromJson(interruptedCli.readAll()).object();
                auto leftovers = fol::ownedProcesses(prefix, {});
                for (const auto &item : leftovers)
                    fol::signalProcess(item, 9);
                check(reached && finished && interruptedCli.exitStatus() == QProcess::NormalExit &&
                          interruptedCli.exitCode() == 2 && result["interrupted"].toBool(),
                      "SIGTERM produces a structured failed runtime result");
                check(leftovers.isEmpty(), "interrupted runtime leaves no owned processes");
                if (runtimeKind == "wine") {
                    interruptEnv.insert("FOL_FIXTURE_MODE", "no-receipt");
                    auto missingReceipt = fol::run(cli, command, 5000, interruptEnv);
                    auto report = QJsonDocument::fromJson(missingReceipt.output.toUtf8()).object();
                    check(missingReceipt.exitCode == 2 && report["exit_code"].toInt() == 0 &&
                              !report["execution_receipt_verified"].toBool(),
                          "Wine zero exit without receipt is not success");
                }
            }
        } else
            QTextStream(stdout) << "SKIP Proton supervisor: namespaces unavailable\n";
        fol::writeFile(paths.data + "/runtimes/synthetic/version", "changed\n");
        check(fol::testProton(paths, "probe", "synthetic")["error"].toString().contains("digest"),
              "changed Proton script/version is rejected");
        const auto originalNamespacePath = qgetenv("PATH");
        auto deniedNamespaceBin = temp.path() + "/denied-namespace-bin";
        QDir().mkpath(deniedNamespaceBin);
        check(QFile::link(QCoreApplication::applicationDirPath() + "/runtime-fixture",
                          deniedNamespaceBin + "/unshare"),
              "namespace-denial fixture link");
        qputenv("PATH", deniedNamespaceBin.toLocal8Bit() + ":" + originalNamespacePath);
        check(fol::testProton(paths, "probe", "synthetic")["error"].toString().contains("digest"),
              "Proton runtime validation remains available when namespace execution is denied");
        qputenv("PATH", originalNamespacePath);
        fol::writeFile(protonSource + "/user_settings.py", "# custom settings, must not execute\n");
        rejects([&] { fol::importProton(paths, "custom-settings", protonSource); },
                "Proton custom executable settings rejected");
        auto malformed = paths.config + "/builds.toml";
        fol::writeFile(malformed, "builds = 12\n");
        rejects([&] { registry.list(); }, "malformed registry schema rejected");
        rejects([&] { registry.add("12.61", root); }, "mutation refuses malformed registry");
        check(fol::readFile(malformed) == "builds = 12\n", "malformed registry preserved");

        QList<fol::Process> graph{{1, 0, 1, "init", false},
                                  {2, 0, 2, "kernel", false},
                                  {10, 1, 10, "unrelated", false},
                                  {20, 1, 20, "game", false},
                                  {21, 20, 21, "game-child", false}};
        check(fol::selectProcessTree(graph, {{0, 0, 0, "exited", false}}).isEmpty(),
              "PID zero must never seed a process tree");
        check(fol::selectProcessTree(graph, {graph[0]}).isEmpty(),
              "init must never seed a process tree");
        check(fol::selectProcessTree(graph, {graph[3]}).size() == 2,
              "only live root descendants selected");
        auto staleRoot = graph[3];
        ++staleRoot.started;
        check(fol::selectProcessTree(graph, {staleRoot}).isEmpty(), "stale root identity ignored");
        for (const auto &item : fol::processes())
            if (item.pid == QCoreApplication::applicationPid())
                check(!fol::signalProcess(item, 0), "cleanup refuses its own launcher identity");

        QProcess owned, unrelated;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", temp.path() + "/test-prefix/");
        env.insert("FOL_SESSION_ID", "test-session");
        owned.setProcessEnvironment(env);
        owned.start("/bin/bash", {"-c", "exec -a EasyAntiCheat_test.exe /bin/sleep 30"});
        unrelated.start("/bin/sleep", {"30"});
        check(owned.waitForStarted() && unrelated.waitForStarted(), "test child processes start");
        QThread::msleep(50);
        auto members = fol::ownedProcesses(temp.path() + "/test-prefix", "test-session");
        check(members.size() == 1 && members[0].pid == owned.processId() && members[0].eac,
              "session ownership and synthetic EAC executable name detection");
        check(fol::ownedProcesses(temp.path() + "/test-pref", "test-session").isEmpty(),
              "trailing slash normalization does not match unrelated prefix names");
        auto stale = members[0];
        ++stale.started;
        check(!fol::signalProcess(stale, 15), "stale process identity must not be signalled");
        check(fol::signalProcess(members[0], 15) && owned.waitForFinished(1000),
              "owned process stops");
        check(unrelated.state() == QProcess::Running, "unrelated process survives");
        unrelated.terminate();
        check(unrelated.waitForFinished(1000), "test cleanup");

        auto supervisorEnv = QProcessEnvironment::systemEnvironment();
        supervisorEnv.insert("WINEPREFIX", temp.path() + "/timeout-prefix");
        supervisorEnv.insert("FOL_SESSION_ID", "timeout-test");
        auto timeout = fol::superviseRuntime("/bin/sh", {"-c", "sleep 30"}, supervisorEnv,
                                             temp.path() + "/timeout-logs", "runtime.log", 150);
        check(timeout.timedOut && !timeout.remaining, "runtime timeout cleans descendants");
        supervisorEnv.insert("FOL_FIXTURE_MODE", "eac-stripped");
        auto stripped = fol::superviseRuntime(
            QCoreApplication::applicationDirPath() + "/runtime-fixture", {}, supervisorEnv,
            temp.path() + "/stripped-logs", "runtime.log", 3000);
        check(stripped.eac && !stripped.remaining,
              "ancestry catches EAC-named child even without ownership environment");

        const auto fixture = QCoreApplication::applicationDirPath() + "/runtime-fixture";
        for (const QString &mode : {QString("trace-eac"), QString("trace-be"),
                                    QString("trace-boundary"), QString("trace-mention")}) {
            supervisorEnv.insert("FOL_FIXTURE_MODE", mode);
            auto traceLogs = temp.path() + "/" + mode;
            const auto trace = fol::superviseRuntime(fixture, {}, supervisorEnv, traceLogs,
                                                     "wine.log", 3000, {root, true});
            check(trace.started && trace.traceReadable && !trace.remaining && !trace.timedOut,
                  "trace-enabled shared supervisor cleans up synthetic launch");
            check(trace.moduleEvidence.isEmpty() == (mode == "trace-mention"),
                  "module trace detection handles boundaries and ignores mere mentions");
            check(fol::readFile(traceLogs + "/wine.log", 2 * 1024 * 1024)
                      .contains(("working-directory=" + root).toUtf8()),
                  "shared game supervisor preserves requested working directory");
        }
        supervisorEnv.insert("FOL_FIXTURE_MODE", "orphan-stripped");
        QProcess survivor;
        survivor.start("/bin/sleep", {"30"});
        check(survivor.waitForStarted(), "unrelated survivor starts");
        auto orphan =
            fol::superviseRuntime(fixture, {}, supervisorEnv, temp.path() + "/orphan-logs",
                                  "wine.log", 700, {root, true});
        check(orphan.timedOut && !orphan.remaining,
              "observed child retains scoped ownership after reparenting and environment removal");
        check(survivor.state() == QProcess::Running, "orphan cleanup preserves unrelated child");
        survivor.terminate();
        check(survivor.waitForFinished(1000), "survivor fixture cleanup");
        auto brokenLogs = temp.path() + "/write-failure";
        check(QDir().mkpath(brokenLogs) && QFile::link("/dev/full", brokenLogs + "/processes.log"),
              "process-log failure fixture");
        supervisorEnv.insert("FOL_FIXTURE_MODE", "trace-mention");
        auto writeFailure = fol::superviseRuntime(fixture, {}, supervisorEnv, brokenLogs,
                                                  "wine.log", 3000, {root, true});
        check(writeFailure.error == "Process log write failed" && !writeFailure.remaining,
              "failed process logging stops and cleans owned launch");

        // Exercise the full offline supervisor when this host allows namespaces
        // and has a Vulkan device. CI without those facilities still runs all
        // parser, registry, CLI, module and process-identity tests above.
        auto namespaceProbe = fol::run("unshare", {"--user", "--map-root-user", "--net", "true"});
        if (!app.arguments().contains("--skip-gpu") && namespaceProbe.exitCode == 0 &&
            fol::vulkanReport()["functional"].toBool()) {
            QFile::remove(malformed);
            registry.add("12.61", root);
            auto prefix = paths.prefix("12.61");
            fol::writeFile(
                prefix + "/.fol-prefix.json",
                QJsonDocument(
                    QJsonObject{{"owner", "fortnite-on-linux"}, {"id", "12.61"}, {"path", prefix}})
                    .toJson());
            fol::writeFile(prefix + "/system.reg", "synthetic test registry");
            QDir().mkpath(prefix + "/drive_c/windows");
            auto bin = temp.path() + "/fake-bin";
            QDir().mkpath(bin);
            check(QFile::link(QCoreApplication::applicationDirPath() + "/runtime-fixture",
                              bin + "/wine"),
                  "runtime fixture link");
            auto originalPath = qgetenv("PATH");
            qputenv("PATH", bin.toLocal8Bit() + ":" + originalPath);
            auto planned = fol::launch(paths, "12.61", true, true, 5);
            check(planned["status"] == "planned; not executed" &&
                      planned["command"].toString().contains("WINEDEBUG="),
                  "offline plan includes managed environment");
            fol::writeFile(exe, older);
            for (const QString runtimeId : {QString{}, QString{"synthetic-second"}}) {
                auto contextPlan =
                    fol::launch(paths, "12.61", true, true, 5, false, false, runtimeId, true);
                check(contextPlan["status"] == "planned; not executed" &&
                          contextPlan["launcher_context_probe"].toBool() &&
                          contextPlan["command"].toString().contains("'-d3d11' '-epicportal'") &&
                          contextPlan["command"].toString().contains("'--net'"),
                      "12.50 opt-in context experiment retains isolation in Wine and Proton");
                auto backendPlan = fol::launch(paths, "12.61", true, true, 5, false, false,
                                               runtimeId, true, fol::BackendProbe::Selector);
                check(backendPlan["status"] == "planned; not executed" &&
                          backendPlan["local_backend_probe"].toBool() &&
                          backendPlan["command"].toString().contains(
                              "'-epicportal' '-epicapp=Localhost'") &&
                          backendPlan["command"].toString().contains("'--net'"),
                      "local backend selection is explicit and retains network isolation");
                auto iniPlan = fol::launch(paths, "12.61", true, true, 5, false, false, runtimeId,
                                           true, fol::BackendProbe::ServiceIni);
                check(iniPlan["status"] == "planned; not executed" &&
                          iniPlan["backend_probe_method"] == "service-ini" &&
                          iniPlan["command"].toString().contains("Domain=127.0.0.1:3551") &&
                          iniPlan["command"].toString().contains("Protocol=http") &&
                          iniPlan["command"].toString().contains("'--net'") &&
                          !iniPlan["command"].toString().contains("epicapp=Localhost"),
                      "service INI routing is a separate isolated experiment on both runtimes");
                const QString helperId = runtimeId.isEmpty() ? "12.61" : "http-probe";
                QString helperPrefix = contextPlan["prefix"].toObject()["path"].toString();
                if (!runtimeId.isEmpty()) {
                    registry.add(helperId, root);
                    auto prepared =
                        fol::prepareProtonSession(paths, helperId, runtimeId, "fixture-http",
                                                  temp.path() + "/http-logs", true);
                    helperPrefix = prepared.prefix;
                }
                QDir().mkpath(helperPrefix + "/dosdevices");
                check(QFile::link("/", helperPrefix + "/dosdevices/z:"),
                      "fixture-only Z drive mapping");
                auto helperPlan = fol::launch(paths, helperId, true, true, 5, false, false,
                                              runtimeId, true, fol::BackendProbe::None, helperPath);
                const auto observerPath = temp.path() + "/fol-http-observer.exe";
                fol::writeFile(observerPath, syntheticPe());
                const auto observerPlan =
                    fol::launch(paths, helperId, true, true, 5, false, false, runtimeId, true,
                                fol::BackendProbe::None, observerPath);
                check(observerPlan["error"].toString().contains("executable hash mismatch"),
                      "request observer rejects a different binary before starting debugger");
                check(helperPlan["status"] == "planned; not executed" &&
                          helperPlan["http_interface_probe"].toObject()["helper"] == helperPath &&
                          helperPlan["command"].toString().contains(fol::quote(helperPath)) &&
                          helperPlan["command"].toString().contains("'Z:\\") &&
                          helperPlan["command"].toString().contains("'--net'") &&
                          helperPlan["exit_code_scope"].toString().contains("HTTP probe helper"),
                      (QByteArray(
                           "read-only helper plan retains isolation, target and exit scope: ") +
                       QJsonDocument(helperPlan).toJson())
                          .constData());
            }
            auto contextBaseline = fol::launch(paths, "12.61", true, true, 5);
            check(!contextBaseline["command"].toString().contains("epicportal") &&
                      !contextBaseline["launcher_context_probe"].toBool(),
                  "12.50 baseline does not acquire experimental launcher context");
            qputenv("FOL_FIXTURE_MODE", "trace-eac");
            auto contextEac = fol::launch(paths, "12.61", false, true, 5, true, false, {}, true);
            qunsetenv("FOL_FIXTURE_MODE");
            check(contextEac["status"] == "stopped-anticheat-detected" &&
                      !contextEac["owned_processes_remaining"].toBool() &&
                      contextEac["stop_requested"].toBool() &&
                      contextEac["crash_diagnosis"].toString().contains(
                          "launcher requested termination"),
                  "context experiment retains anti-cheat trace stop and scoped cleanup");
            fol::writeFile(exe, syntheticPe());
            check(fol::launch(paths, "12.61", true, true, 5, true, false, "synthetic")["error"]
                      .toString()
                      .contains("digest"),
                  "Proton game launch rejects the previously corrupted runtime fixture");
            auto protonPlan =
                fol::launch(paths, "12.61", true, true, 5, true, false, "synthetic-second");
            check(protonPlan["status"] == "planned; not executed" &&
                      protonPlan["command"].toString().contains("PROTON_LOG='0'") &&
                      protonPlan["command"].toString().contains("/proton' 'run'") &&
                      !QDir(paths.data + "/proton-prefixes/12.61").exists(),
                  "Proton game plan uses script and visible Wine trace without creating prefix");
            auto interface = bin + "/steam-runtime-launcher-interface-0";
            check(QFile::link(fixture, interface), "synthetic Steam handoff interface link");
            auto handoff =
                fol::launch(paths, "12.61", true, true, 5, true, false, "synthetic-second");
            check(handoff["error"].toString().contains("cannot hand off"),
                  "Proton probe rejects a service handoff outside its managed namespace");
            check(QFile::remove(interface), "remove owned synthetic handoff link");
            qputenv("STEAM_COMPAT_MEDIA_PATH", "/must-not-use-other-game-media");
            qputenv("PROTON_REMOTE_DEBUG_CMD", "must-not-execute");
            qputenv("FOL_FIXTURE_MODE", "proton-game");
            auto protonGame =
                fol::launch(paths, "12.61", false, true, 5, true, false, "synthetic-second");
            qunsetenv("STEAM_COMPAT_MEDIA_PATH");
            qunsetenv("PROTON_REMOTE_DEBUG_CMD");
            qunsetenv("FOL_FIXTURE_MODE");
            check(protonGame["exit_code"] == 53 &&
                      protonGame["likely_subsystem"] == "missing-library" &&
                      !protonGame["owned_processes_remaining"].toBool(),
                  "Proton game probe scrubs foreign settings and supervises captured output");
            qputenv("FOL_FIXTURE_MODE", "trace-eac");
            auto protonModule =
                fol::launch(paths, "12.61", false, true, 5, true, false, "synthetic-second");
            qunsetenv("FOL_FIXTURE_MODE");
            check(protonModule["status"] == "stopped-anticheat-detected" &&
                      !protonModule["owned_processes_remaining"].toBool(),
                  "Proton game probe retains anti-cheat module trace monitoring and cleanup");
            auto traced = fol::launch(paths, "12.61", true, true, 5, true);
            check(traced["startup_trace"].toBool() &&
                      traced["command"].toString().contains(
                          "+msgbox,+debugstr,+process,+exec,+reg") &&
                      !planned["command"].toString().contains("+msgbox"),
                  "startup trace is explicit and preserves baseline command");
            auto engineLogging = fol::launch(paths, "12.61", true, true, 5, true, true);
            check(engineLogging["engine_stdout_requested"].toBool() &&
                      engineLogging["command"].toString().contains(
                          "'-log' '-d3d11' '-stdout' '-FullStdOutLogOutput' '-forcelogflush'") &&
                      !traced["command"].toString().contains("'-stdout'") &&
                      !planned["engine_stdout_requested"].toBool(),
                  "engine stdout is a separate opt-in logging-only argument change");
            const auto originalDllPath = qgetenv("WINEDLLPATH");
            const bool hadDllPath = qEnvironmentVariableIsSet("WINEDLLPATH");
            qputenv("WINEDLLPATH", "/runtime with spaces/it's-vkd3d:/runtime/wine");
            auto dllPlan = fol::launch(paths, "12.61", true, true, 5);
            check(dllPlan["inherited_runtime_environment"].toObject()["WINEDLLPATH"] ==
                          QString::fromUtf8(qgetenv("WINEDLLPATH")) &&
                      dllPlan["command"].toString().contains(
                          "WINEDLLPATH=" + fol::quote(QString::fromUtf8(qgetenv("WINEDLLPATH")))),
                  "runtime DLL search path is explicitly recorded and shell quoted");
            if (hadDllPath)
                qputenv("WINEDLLPATH", originalDllPath);
            else
                qunsetenv("WINEDLLPATH");
            const auto originalCpuMask = qgetenv("OPENSSL_ia32cap");
            const bool hadCpuMask = qEnvironmentVariableIsSet("OPENSSL_ia32cap");
            qputenv("OPENSSL_ia32cap", "~0x20000000");
            for (const QString runtimeId : {QString{}, QString{"synthetic-second"}}) {
                auto maskPlan = fol::launch(paths, "12.61", true, true, 5, false, false, runtimeId);
                check(maskPlan["status"] == "planned; not executed" &&
                          maskPlan["inherited_runtime_environment"].toObject()["OPENSSL_ia32cap"] ==
                              "~0x20000000" &&
                          maskPlan["command"].toString().contains("OPENSSL_ia32cap='~0x20000000'"),
                      "explicit OpenSSL diagnostic environment is recorded for Wine and Proton");
            }
            qunsetenv("OPENSSL_ia32cap");
            auto noMaskPlan = fol::launch(paths, "12.61", true, true, 5);
            check(!noMaskPlan["command"].toString().contains("OPENSSL_ia32cap"),
                  "launcher does not enable an OpenSSL capability mask by default");
            if (hadCpuMask)
                qputenv("OPENSSL_ia32cap", originalCpuMask);
            auto failed = fol::launch(paths, "12.61", false, true, 5);
            check(failed["exit_code"] == 53 && failed["likely_subsystem"] == "missing-library" &&
                      !failed["stop_requested"].toBool(),
                  "supervisor captures synthetic crash evidence");
            QProcess surviving;
            surviving.start("/bin/sleep", {"30"});
            check(surviving.waitForStarted(),
                  "unrelated process starts before root-exit regression");
            qputenv("FOL_FIXTURE_MODE", "root-exits-first");
            auto earlyExit = fol::launch(paths, "12.61", false, true, 5);
            qunsetenv("FOL_FIXTURE_MODE");
            check(
                earlyExit["status"] == "exited; gameplay-unverified" &&
                    earlyExit["exit_code"].toInt() == 0,
                "root exit before surviving Wine-style services does not select global processes");
            check(surviving.state() == QProcess::Running,
                  "unrelated process survives root-exit cleanup");
            surviving.terminate();
            check(surviving.waitForFinished(1000), "root-exit unrelated fixture cleanup");
            qputenv("FOL_FIXTURE_MODE", "eac-name");
            auto eac = fol::launch(paths, "12.61", false, true, 5);
            qunsetenv("FOL_FIXTURE_MODE");
            qputenv("PATH", originalPath);
            check(eac["status"] == "stopped-eac-detected",
                  "supervisor stops synthetic EAC-named child");
            QThread::msleep(100);
            check(fol::ownedProcesses(prefix, {}).isEmpty(),
                  "supervisor leaves no owned processes");
        } else {
            QTextStream(stdout)
                << "SKIP full offline supervisor: --skip-gpu, namespaces or Vulkan unavailable\n";
        }
        QTextStream(stdout) << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << '\n';
        return 1;
    }
}
