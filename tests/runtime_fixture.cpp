#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <QThread>
#include <unistd.h>

// Native synthetic runtime for supervisor tests. No Wine, Fortnite or EAC code.
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto executable = QFileInfo(app.arguments()[0]).fileName();
    if (executable == "wineserver")
        return 0;
    const auto mode = qEnvironmentVariable("FOL_FIXTURE_MODE");
    if (mode == "proton-game") {
        if (executable != "proton" || app.arguments().value(1) != "run" ||
            !app.arguments().value(2).endsWith("FortniteClient-Win64-Shipping.exe") ||
            qEnvironmentVariable("PROTON_LOG") != "0" ||
            !qEnvironmentVariable("WINEDEBUG").contains("+loaddll") ||
            !qEnvironmentVariable("STEAM_COMPAT_MEDIA_PATH").isEmpty() ||
            !qEnvironmentVariable("PROTON_REMOTE_DEBUG_CMD").isEmpty())
            return 88;
        QTextStream(stderr) << "Synthetic Proton import failure: err:module:import_dll c0000135\n";
        QThread::msleep(150);
        return 53;
    }
    if (mode == "orphan-stripped") {
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove("WINEPREFIX");
        environment.remove("FOL_SESSION_ID");
        child.setProcessEnvironment(environment);
        child.setProgram("/bin/sleep");
        child.setArguments({"30"});
        child.start();
        if (!child.waitForStarted())
            return 1;
        // Allow at least one supervisor sample before this parent exits. The
        // child's ownership must then be retained by PID/start-time identity.
        QThread::msleep(200);
        // Exit without QProcess's destructor killing the synthetic child.
        ::_exit(0);
    }
    if (mode.startsWith("trace-")) {
        QTextStream output(stdout);
        if (mode == "trace-boundary")
            output << QString(1024 * 1024 - 16, 'x') << '\n';
        if (mode == "trace-eac" || mode == "trace-boundary")
            output << "trace:loaddll:build_module Loaded L\"C:\\test\\EasyAntiCheat_x64.dll\" at "
                      "0x10\n";
        else if (mode == "trace-be")
            output << "trace:loaddll:build_module Loaded L\"C:\\test\\BEClient_x64.dll\" at 0x10\n";
        else
            output << "Missing EasyAntiCheat_x64.dll; this is not a loaded-module trace\n";
        output << "working-directory=" << QDir::currentPath() << Qt::endl;
        QThread::msleep(mode == "trace-mention" ? 100 : 30000);
        return 0;
    }
    if (qEnvironmentVariable("FOL_FIXTURE_MODE") == "root-exits-first") {
        QProcess child;
        child.setProgram("/bin/sleep");
        child.setArguments({"0.5"});
        child.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        return child.startDetached() ? 0 : 1;
    }
    if (app.arguments().contains("--version")) {
        QTextStream(stdout) << "synthetic-runtime-fixture\n";
        return 0;
    }
    if (qEnvironmentVariable("FOL_FIXTURE_MODE") == "hang") {
        QFile ready(qEnvironmentVariable("FOL_FIXTURE_READY"));
        if (!ready.open(QIODevice::WriteOnly))
            return 1;
        ready.write("ready\n");
        ready.close();
        QThread::msleep(30000);
        return 0;
    }
    if (qEnvironmentVariable("FOL_FIXTURE_MODE").startsWith("eac-")) {
        QProcess child;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("WINEPREFIX", env.value("WINEPREFIX") + "/");
        if (qEnvironmentVariable("FOL_FIXTURE_MODE") == "eac-stripped") {
            env.remove("WINEPREFIX");
            env.remove("FOL_SESSION_ID");
        }
        child.setProcessEnvironment(env);
        child.start("/bin/bash", {"-c", "exec -a EasyAntiCheat_synthetic.exe /bin/sleep 30"});
        if (!child.waitForStarted())
            return 1;
        child.waitForFinished(30000);
        return 0;
    }
    if (executable == "proton" ||
        (executable == "wine" && qEnvironmentVariable("FOL_FIXTURE_MODE") == "no-receipt")) {
        if (!qEnvironmentVariable("STEAM_COMPAT_MEDIA_PATH").isEmpty() ||
            !qEnvironmentVariable("PROTON_REMOTE_DEBUG_CMD").isEmpty())
            return 88;
        if (qEnvironmentVariable("FOL_FIXTURE_MODE") != "no-receipt") {
            auto root = qEnvironmentVariable("WINEPREFIX") + "/drive_c";
            if (!QDir().mkpath(root))
                return 1;
            auto session = qEnvironmentVariable("FOL_SESSION_ID");
            QFile receipt(root + "/" + session + ".txt");
            if (!receipt.open(QIODevice::WriteOnly) || receipt.write(session.toUtf8() + "\r\n") < 0)
                return 1;
        }
        QThread::msleep(100);
        return 0;
    }
    QTextStream(stderr) << "Synthetic failure: err:module:import_dll c0000135\n";
    QThread::msleep(150);
    return 53;
}
