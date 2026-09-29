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
        auto root = temp.path() + "/game with spaces";
        auto profile = fol::Profile::load();
        check(profile.version == "12.61", "12.61 remains default profile");
        auto diagnosticProfile = fol::Profile::forVersion("12.50");
        check(diagnosticProfile && diagnosticProfile->version == "12.50" &&
                  !diagnosticProfile->privatePathVerified,
              "12.50 has separate diagnostic-only profile");
        check(!fol::Profile::forVersion("12.60") && !fol::Profile::forVersion("../../12.61"),
              "unknown and traversal profiles rejected");
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
        check(fol::classifyFailure("err:module:import_dll missing") == "missing-library",
              "DLL failure classification");
        check(fol::classifyFailure("nothing informative") == "unknown; inspect logs",
              "no invented crash cause");
        auto launch = fol::launch(paths, "missing", false, false, 1);
        check(launch["status"] == "blocked" &&
                  QFile::exists(launch["logs"].toString() + "/result.json"),
              "failed launch persists session evidence");
        auto blocked = fol::launch(paths, "12.61", false, false, 1);
        check(blocked["status"] == "blocked", "unverified private path blocked");
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
            auto traced = fol::launch(paths, "12.61", true, true, 5, true);
            check(traced["startup_trace"].toBool() &&
                      traced["command"].toString().contains("+msgbox,+debugstr") &&
                      !planned["command"].toString().contains("+msgbox"),
                  "startup trace is explicit and preserves baseline command");
            auto failed = fol::launch(paths, "12.61", false, true, 5);
            check(failed["exit_code"] == 53 && failed["likely_subsystem"] == "missing-library",
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
