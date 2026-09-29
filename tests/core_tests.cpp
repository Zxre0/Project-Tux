#include "fol/core.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QUdpSocket>
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
        auto exe = root + "/" + profile.executable;
        fol::writeFile(exe, syntheticPe());
        QDir().mkpath(root + "/Engine");
        fol::writeFile(root + "/FortniteGame/Content/Paks/synthetic.pak", QByteArray(128, 'x'));
        auto build = fol::inspectBuild(root);
        check(build.valid() && build.version == "12.61" && build.engine == "4.25.0",
              "ASCII version metadata");
        check(fol::discoverBuilds({temp.path()}).contains(root),
              "discovery finds structural build");
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

        QUdpSocket port;
        check(port.bind(QHostAddress::LocalHost, 0, QUdpSocket::DontShareAddress), "test UDP bind");
        check(!fol::portReport("127.0.0.1", port.localPort())["bind_available"].toBool(),
              "occupied UDP port detected");

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
        auto malformed = paths.config + "/builds.toml";
        fol::writeFile(malformed, "builds = 12\n");
        rejects([&] { registry.list(); }, "malformed registry schema rejected");
        rejects([&] { registry.add("12.61", root); }, "mutation refuses malformed registry");
        check(fol::readFile(malformed) == "builds = 12\n", "malformed registry preserved");

        QProcess owned, unrelated;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", temp.path() + "/test-prefix");
        env.insert("FOL_SESSION_ID", "test-session");
        owned.setProcessEnvironment(env);
        owned.start("/bin/bash", {"-c", "exec -a EasyAntiCheat_test.exe /bin/sleep 30"});
        unrelated.start("/bin/sleep", {"30"});
        check(owned.waitForStarted() && unrelated.waitForStarted(), "test child processes start");
        QThread::msleep(50);
        auto members = fol::ownedProcesses(temp.path() + "/test-prefix", "test-session");
        check(members.size() == 1 && members[0].pid == owned.processId() && members[0].eac,
              "session ownership and synthetic EAC executable name detection");
        auto stale = members[0];
        ++stale.started;
        check(!fol::signalProcess(stale, 15), "stale process identity must not be signalled");
        check(fol::signalProcess(members[0], 15) && owned.waitForFinished(1000),
              "owned process stops");
        check(unrelated.state() == QProcess::Running, "unrelated process survives");
        unrelated.terminate();
        check(unrelated.waitForFinished(1000), "test cleanup");
        QTextStream(stdout) << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << '\n';
        return 1;
    }
}
