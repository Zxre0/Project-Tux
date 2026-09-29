#include "fol/core.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
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
bool isBattlEyeName(const QString &name) {
    QString base = name;
    base.replace('\\', '/');
    base = base.section('/', -1).toLower();
    return base == "beservice.exe" || base == "beservice_x64.exe" || base == "belauncher.exe" ||
           base.contains("shipping_be.exe");
}
QString antiCheatModuleEvidence(const QByteArray &text) {
    for (const auto &line : text.split('\n')) {
        auto lower = line.toLower();
        if (lower.contains(":loaddll:") && lower.contains("loaded ") &&
            (lower.contains("easyanticheat") || lower.contains("beclient_x64.dll") ||
             lower.contains("beclient.dll")))
            return QString::fromUtf8(line.left(2048));
    }
    return {};
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
    // Zombies cannot execute or be signalled; counting them as live descendants
    // can turn a completed launch into a spurious supervisor timeout.
    if (fields[0] == "Z" || fields[0] == "X")
        return {};
    Process process{pid, fields[1].toLongLong(), fields[19].toULongLong(),
                    QString::fromLocal8Bit(bytes.mid(begin + 1, end - begin - 1)), false};
    process.eac = isEacName(process.name);
    process.battleye = isBattlEyeName(process.name);
    QFile cmd(QString("/proc/%1/cmdline").arg(pid));
    if (cmd.open(QIODevice::ReadOnly)) {
        // Inspect executable names; do not persist other applications' arguments or credentials.
        auto args = cmd.read(65536).split('\0');
        for (const auto &arg : args) {
            if (isEacName(QString::fromLocal8Bit(arg)))
                process.eac = true;
            if (isBattlEyeName(QString::fromLocal8Bit(arg)))
                process.battleye = true;
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
QList<Process> selectProcessTree(const QList<Process> &snapshot, const QList<Process> &roots) {
    QSet<qint64> selected;
    for (const auto &root : roots) {
        // QProcess::processId() becomes zero after exit. Expanding zero reaches
        // init/kernel threads and unrelated sessions, not a valid launch tree.
        if (root.pid <= 1)
            continue;
        for (const auto &item : snapshot)
            if (item.pid == root.pid && item.started == root.started)
                selected.insert(item.pid);
    }
    for (qsizetype pass = 0; pass < snapshot.size(); ++pass) {
        bool changed = false;
        for (const auto &item : snapshot)
            if (item.pid > 1 && selected.contains(item.parent) && !selected.contains(item.pid)) {
                selected.insert(item.pid);
                changed = true;
            }
        if (!changed)
            break;
    }
    QList<Process> result;
    for (const auto &item : snapshot)
        if (selected.contains(item.pid))
            result.append(item);
    return result;
}
QList<Process> ownedProcesses(const QString &prefix, const QString &session) {
    QList<Process> result;
    auto wantedPrefix = QDir::cleanPath(prefix);
    auto wantedSession = "FOL_SESSION_ID=" + session.toUtf8();
    for (const auto &process : processes()) {
        QFile env(QString("/proc/%1/environ").arg(process.pid));
        if (!env.open(QIODevice::ReadOnly))
            continue;
        auto entries = env.read(1024 * 1024).split('\0');
        bool prefixMatches = false;
        for (const auto &entry : entries) {
            if (entry.startsWith("WINEPREFIX=")) {
                auto value = QString::fromLocal8Bit(entry.mid(11));
                // Proton appends a trailing slash to compatdata/pfx. Compare
                // absolute lexical paths; never broaden to a prefix-string match.
                prefixMatches =
                    QDir::isAbsolutePath(value) && QDir::cleanPath(value) == wantedPrefix;
                break;
            }
        }
        if (prefixMatches && (session.isEmpty() || entries.contains(wantedSession)))
            result.append(process);
    }
    return result;
}
bool signalProcess(const Process &process, int signal) {
    if (process.pid <= 1)
        return false;
    // Never terminate this launcher or its host ancestry, even if ownership
    // selection regresses. pidfds alone only guard against recycled identities.
    auto ancestor = qint64(getpid());
    while (ancestor > 1) {
        if (process.pid == ancestor)
            return false;
        auto parent = inspectProcess(ancestor);
        if (!parent || parent->parent == ancestor)
            break;
        ancestor = parent->parent;
    }
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
    return {{"pid", pid},   {"parent_pid", parent},     {"start_ticks", QString::number(started)},
            {"name", name}, {"eac_name_detected", eac}, {"battleye_name_detected", battleye}};
}
} // namespace fol
