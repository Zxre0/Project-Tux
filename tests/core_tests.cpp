#include "fol/core.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>
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
        QTextStream(stdout) << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << '\n';
        return 1;
    }
}
