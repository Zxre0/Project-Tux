#include "fol/core.h"
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QSet>
#include <QThread>
#include <csignal>

namespace fol {
static volatile std::sig_atomic_t runtimeInterrupted = 0;
static void onInterrupt(int) { runtimeInterrupted = 1; }
// CLI supervision is serialized. A future GUI must run this in a worker process,
// not install competing process-wide signal handlers from multiple threads.
class SignalScope {
    using Handler = void (*)(int);
    Handler oldInt, oldTerm;

  public:
    SignalScope() {
        runtimeInterrupted = 0;
        oldInt = std::signal(SIGINT, onInterrupt);
        oldTerm = std::signal(SIGTERM, onInterrupt);
    }
    ~SignalScope() {
        std::signal(SIGINT, oldInt);
        std::signal(SIGTERM, oldTerm);
    }
};
SupervisionResult superviseRuntime(const QString &program, const QStringList &args,
                                   const QProcessEnvironment &env, const QString &logs,
                                   const QString &outputName, int timeoutMs) {
    auto prefix = env.value("WINEPREFIX"), session = env.value("FOL_SESSION_ID");
    require(QDir::isAbsolutePath(prefix) && !session.isEmpty() && timeoutMs > 0,
            "Runtime supervision requires absolute prefix, session and positive timeout");
    require(outputName == "wine.log" || outputName == "runtime.log", "Invalid runtime log name");
    require(QDir().mkpath(logs), "Cannot create runtime log directory");
    QFile snapshots(logs + "/processes.log");
    require(snapshots.open(QIODevice::WriteOnly), "Cannot open runtime process log");
    SignalScope signalScope;
    QProcess process;
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setStandardOutputFile(logs + "/" + outputName);
    SupervisionResult result;
    process.start(program, args);
    result.started = process.waitForStarted(5000);
    if (!result.started) {
        result.error = process.errorString();
        return result;
    }
    QMap<qint64, Process> observed;
    auto members = [&] {
        auto all = processes();
        QSet<qint64> ids;
        if (process.state() != QProcess::NotRunning)
            ids.insert(process.processId());
        for (const auto &item : ownedProcesses(prefix, session))
            ids.insert(item.pid);
        // Retain ancestry ownership after reparenting only while PID/start ticks
        // still match. Never treat a recycled PID as the old session's child.
        for (const auto &item : all)
            if (observed.contains(item.pid) && observed[item.pid].started == item.started)
                ids.insert(item.pid);
        for (qsizetype pass = 0; pass < all.size(); ++pass) {
            bool changed = false;
            for (const auto &item : all)
                if (ids.contains(item.parent) && !ids.contains(item.pid)) {
                    ids.insert(item.pid);
                    changed = true;
                }
            if (!changed)
                break;
        }
        QList<Process> selected;
        for (const auto &item : all)
            if (ids.contains(item.pid)) {
                selected.append(item);
                observed.insert(item.pid, item);
            }
        return selected;
    };
    QElapsedTimer elapsed;
    elapsed.start();
    while (true) {
        auto current = members();
        QJsonArray snapshot;
        for (const auto &item : current) {
            snapshot.append(item.json());
            result.eac |= item.eac;
        }
        auto bytes =
            QJsonDocument(QJsonObject{{"timestamp",
                                       QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                                      {"elapsed_ms", elapsed.elapsed()},
                                      {"processes", snapshot}})
                .toJson(QJsonDocument::Compact) +
            '\n';
        if (snapshots.write(bytes) != bytes.size() || !snapshots.flush()) {
            result.error = "Process log write failed";
            break;
        }
        result.interrupted = bool(runtimeInterrupted);
        result.timedOut = elapsed.elapsed() >= timeoutMs;
        if (result.eac || result.timedOut || result.interrupted ||
            (process.state() == QProcess::NotRunning && current.isEmpty()))
            break;
        if (process.state() == QProcess::NotRunning)
            QThread::msleep(50);
        else
            process.waitForFinished(50);
    }
    for (const auto &item : members())
        signalProcess(item, SIGTERM);
    if (process.state() != QProcess::NotRunning) {
        process.terminate();
        process.waitForFinished(1000);
    }
    for (const auto &item : members())
        signalProcess(item, SIGKILL);
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(1000);
    }
    QElapsedTimer cleanup;
    cleanup.start();
    while (!members().isEmpty() && cleanup.elapsed() < 3000)
        QThread::msleep(50);
    result.remaining = !members().isEmpty();
    result.exitCode = process.exitCode();
    result.crashed = process.exitStatus() == QProcess::CrashExit;
    result.interrupted |= bool(runtimeInterrupted);
    return result;
}
} // namespace fol
