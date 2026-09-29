#include <QCoreApplication>
#include <QProcess>
#include <QTextStream>
#include <QThread>

// Native synthetic runtime for supervisor tests. No Wine, Fortnite or EAC code.
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--version")) {
        QTextStream(stdout) << "synthetic-runtime-fixture\n";
        return 0;
    }
    if (qEnvironmentVariable("FOL_FIXTURE_MODE") == "eac-name") {
        QProcess child;
        child.start("/bin/bash", {"-c", "exec -a EasyAntiCheat_synthetic.exe /bin/sleep 30"});
        if (!child.waitForStarted())
            return 1;
        child.waitForFinished(30000);
        return 0;
    }
    QTextStream(stderr) << "Synthetic failure: err:module:import_dll c0000135\n";
    QThread::msleep(150);
    return 53;
}
