#include "fol/core.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <sys/syscall.h>
#include <unistd.h>

namespace fol {
bool isEacName(const QString &name) {
    QString base = name;
    base.replace('\\', '/');
    base = base.section('/', -1).toLower();
    return base.startsWith("easyanticheat") || base == "start_protected_game.exe" ||
           base.contains("shipping_eac") || base == "eac_launcher.exe";
}
static std::optional<Process> inspectProcess(qint64 pid) {
    QFile stat(QString("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly))
        return {};
    QByteArray bytes = stat.readAll();
    auto end = bytes.lastIndexOf(')');
    auto begin = bytes.indexOf('(');
    if (end < 0 || begin < 0)
        return {};
    auto fields = bytes.mid(end + 2).split(' ');
    if (fields.size() < 20)
        return {};
    Process process{pid, fields[1].toLongLong(), fields[19].toULongLong(),
                    QString::fromLocal8Bit(bytes.mid(begin + 1, end - begin - 1)), false};
    process.eac = isEacName(process.name);
    QFile cmd(QString("/proc/%1/cmdline").arg(pid));
    if (cmd.open(QIODevice::ReadOnly)) {
        // Inspect executable names; do not persist other applications' arguments or credentials.
        auto args = cmd.read(65536).split('\0');
        for (const auto &arg : args) {
            if (isEacName(QString::fromLocal8Bit(arg)))
                process.eac = true;
        }
    }
    return process;
}
QList<Process> processes() {
    QList<Process> result;
    QDir proc("/proc");
    for (const auto &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok;
        auto pid = entry.toLongLong(&ok);
        if (!ok)
            continue;
        if (auto process = inspectProcess(pid))
            result.append(*process);
    }
    return result;
}
QList<Process> ownedProcesses(const QString &prefix, const QString &session) {
    QList<Process> result;
    auto wantedPrefix = "WINEPREFIX=" + prefix.toLocal8Bit();
    auto wantedSession = "FOL_SESSION_ID=" + session.toUtf8();
    for (const auto &process : processes()) {
        QFile env(QString("/proc/%1/environ").arg(process.pid));
        if (!env.open(QIODevice::ReadOnly))
            continue;
        auto entries = env.read(1024 * 1024).split('\0');
        if (entries.contains(wantedPrefix) &&
            (session.isEmpty() || entries.contains(wantedSession)))
            result.append(process);
    }
    return result;
}
bool signalProcess(const Process &process, int signal) {
    int fd = int(syscall(SYS_pidfd_open, process.pid, 0));
    if (fd < 0)
        return false;
    auto current = inspectProcess(process.pid);
    bool ok = current && current->started == process.started &&
              syscall(SYS_pidfd_send_signal, fd, signal, nullptr, 0) == 0;
    close(fd);
    return ok;
}
QJsonObject Process::json() const {
    return {{"pid", pid},
            {"parent_pid", parent},
            {"start_ticks", QString::number(started)},
            {"name", name},
            {"eac_name_detected", eac}};
}
} // namespace fol
