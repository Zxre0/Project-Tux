#pragma once
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLockFile>
#include <QProcessEnvironment>
#include <QStringList>
#include <memory>
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
QString networkNamespaceIdentity();
void preparePrivateLoopback(const QString &outerNamespace); // Trusted parent's pre-unshare receipt.
struct CommandResult {
    int exitCode;
    bool timedOut;
    QString output;
};
CommandResult run(const QString &program, const QStringList &args, int timeout = 10000,
                  const QProcessEnvironment &env = QProcessEnvironment::systemEnvironment());
struct Profile {
    QString version, executable, workingDirectory, rebootStatus;
    QStringList arguments, launcherContextArguments, localBackendArguments, serviceIniArguments;
    bool privatePathVerified, httpInterfaceProbeAllowed;
    QString httpObserverHash, httpObserverLayout, httpRouterLayout;
    static Profile load();
    static std::optional<Profile> forVersion(const QString &version);
};
struct PeInfo {
    bool valid = false, x64 = false, dll = false;
    QString error;
};
PeInfo inspectPe(const QString &path);
QJsonObject peImportReport(const QString &path, const QStringList &directories = {});
struct Build {
    QString root, version, engine;
    QStringList evidence, errors, warnings;
    bool valid() const { return errors.isEmpty(); }
    QJsonObject json() const;
};
Build inspectBuild(const QString &path);
QJsonObject verifyIntegrity(const QString &root, const QString &manifest);
QJsonObject inspectEpicManifest(const QString &manifest, const QString &installation = {},
                                bool hashFiles = false);
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
struct ProtonSession {
    QString runtime, compat, prefix, version;
    QProcessEnvironment environment;
    QMap<QString, QString> managed;
    QStringList unsetKeys;
    std::unique_ptr<QLockFile> prefixLock, runtimeLock;
};
// Dry planning validates ownership without creating storage or acquiring locks.
// Execution callers must retain the returned locks throughout supervision.
ProtonSession prepareProtonSession(const Paths &, const QString &id, const QString &runtimeId,
                                   const QString &session, const QString &logs, bool prepare);
QJsonObject systemReport();
QJsonObject vulkanReport();
QJsonObject portReport(const QString &address, quint16 port);
QJsonObject udpSocketCreationReport(bool ipv6);
QJsonObject localIpcReport();
QJsonObject moduleReport(const QString &path, const QString &manifest,
                         const QString &expectedVersion = {});
struct Process {
    qint64 pid, parent;
    quint64 started;
    QString name;
    bool eac;
    bool battleye = false;
    QJsonObject json() const;
};
bool isEacName(const QString &name);
bool isBattlEyeName(const QString &name);
QString antiCheatModuleEvidence(const QByteArray &text);
QList<Process> processes();
QList<Process> selectProcessTree(const QList<Process> &snapshot, const QList<Process> &roots);
QList<Process> ownedProcesses(const QString &prefix, const QString &session);
bool signalProcess(const Process &, int signal);
struct SupervisionResult {
    bool started = false, crashed = false, timedOut = false, interrupted = false;
    bool eac = false, battleye = false, remaining = false;
    int exitCode = -1;
    QString error, moduleEvidence;
    bool traceReadable = true;
    bool stopRequested = false;
};
struct SupervisionOptions {
    QString workingDirectory;
    bool inspectAntiCheatModules = false;
};
SupervisionResult superviseRuntime(const QString &program, const QStringList &args,
                                   const QProcessEnvironment &env, const QString &logs,
                                   const QString &outputName, int timeoutMs,
                                   const SupervisionOptions &options = {});
enum class BackendProbe { None, Selector, ServiceIni };
QJsonObject launch(const Paths &, const QString &id, bool printOnly, bool offlineProbe,
                   int timeoutSeconds, bool traceStartup = false, bool engineStdout = false,
                   const QString &protonId = {}, bool launcherContextProbe = false,
                   BackendProbe backendProbe = BackendProbe::None,
                   const QString &httpProbeHelper = {}, const QString &httpModule = {},
                   bool privateBackend = false, bool developmentLogin = false,
                   const QString &lawinConfig = {});
QString classifyFailure(const QString &text);
QJsonObject analyzeHttpProbe(const QByteArray &report, const QString &session);
QJsonObject analyzeModuleReceipt(const QByteArray &report, const QString &session, qint64 childPid);
QJsonObject analyzeModuleObservation(const QByteArray &report, const QString &session,
                                     qint64 childPid);
QJsonObject analyzeGameLog(const QByteArray &data);
QJsonObject captureGameLogs(const QString &prefix, const QString &directory,
                            const QDateTime &notBefore, const QByteArray &localSecret = {});
} // namespace fol
