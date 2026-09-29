#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStringList>
#include <optional>
#include <stdexcept>

namespace fol {
class Error : public std::runtime_error {
  public:
    explicit Error(const QString &message) : std::runtime_error(message.toStdString()) {}
};
struct Paths {
    QString config, data, cache, state;
    static Paths system();
    QString prefix(const QString &id) const;
};
void require(bool condition, const QString &message);
void validateId(const QString &id);
QByteArray readFile(const QString &path, qint64 limit = 1024 * 1024);
void writeFile(const QString &path, const QByteArray &data);
QJsonObject readJson(const QString &path);
QString quote(const QString &value);
struct CommandResult {
    int exitCode;
    bool timedOut;
    QString output;
};
CommandResult run(const QString &program, const QStringList &args, int timeout = 10000,
                  const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment());
struct Profile {
    QString version, executable, workingDirectory, rebootStatus;
    QStringList arguments;
    bool privatePathVerified;
    static Profile load();
};
struct PeInfo {
    bool valid = false, x64 = false, dll = false;
    QString error;
};
PeInfo inspectPe(const QString &path);
struct Build {
    QString root, version, engine;
    QStringList evidence, errors, warnings;
    bool valid() const { return errors.isEmpty(); }
    QJsonObject json() const;
};
Build inspectBuild(const QString &path);
QJsonObject verifyIntegrity(const QString &root, const QString &manifest);
QStringList discoverBuilds(const QStringList &roots, int maxDepth = 7);
class Registry {
    Paths paths;

  public:
    explicit Registry(Paths p) : paths(std::move(p)) {}
    QMap<QString, QString> list() const;
    void add(const QString &id, const QString &path);
    void remove(const QString &id);
    QString resolve(const QString &id) const;
};
QJsonArray runtimes();
QString wineExecutable();
QJsonObject prefixStatus(const Paths &, const QString &id);
QJsonObject createPrefix(const Paths &, const QString &id);
QJsonObject stopPrefix(const Paths &, const QString &id);
QJsonObject backupPrefix(const Paths &, const QString &id, bool archive = false);
QJsonObject prefixBackups(const Paths &, const QString &id);
QJsonObject restorePrefix(const Paths &, const QString &id, const QString &backup);
QJsonObject resetPrefix(const Paths &, const QString &id);
QJsonObject testRuntime(const Paths &, const QString &id);
QJsonObject importProton(const Paths &, const QString &id, const QString &source);
QJsonArray importedProtonRuntimes(const Paths &);
QJsonObject testProton(const Paths &, const QString &id, const QString &runtimeId);
QJsonObject systemReport();
QJsonObject vulkanReport();
QJsonObject portReport(const QString &address, quint16 port);
QJsonObject moduleReport(const QString &path, const QString &manifest);
struct Process {
    qint64 pid, parent;
    quint64 started;
    QString name;
    bool eac;
    QJsonObject json() const;
};
bool isEacName(const QString &name);
QList<Process> processes();
QList<Process> ownedProcesses(const QString &prefix, const QString &session);
bool signalProcess(const Process &, int signal);
QJsonObject launch(const Paths &, const QString &id, bool printOnly, bool offlineProbe,
                   int timeoutSeconds);
QString classifyFailure(const QString &text);
} // namespace fol
