#include "fol/core.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <sstream>
#include <toml++/toml.hpp>

static void initProfiles() { Q_INIT_RESOURCE(profiles); }
namespace fol {
void require(bool condition, const QString &message) {
    if (!condition)
        throw Error(message);
}
void validateId(const QString &id) {
    require(QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$").match(id).hasMatch(),
            "Invalid identifier: use 1-64 letters, digits, dots, underscores or hyphens");
}
static QString xdg(const char *key, const QString &fallback) {
    QString value = qEnvironmentVariable(key);
    if (!QDir::isAbsolutePath(value))
        value = QDir::homePath() + fallback;
    return QDir::cleanPath(value) + "/fortnite-on-linux";
}
Paths Paths::system() {
    return {xdg("XDG_CONFIG_HOME", "/.config"), xdg("XDG_DATA_HOME", "/.local/share"),
            xdg("XDG_CACHE_HOME", "/.cache"), xdg("XDG_STATE_HOME", "/.local/state")};
}
QString Paths::prefix(const QString &id) const {
    validateId(id);
    return data + "/prefixes/" + id;
}
QByteArray readFile(const QString &path, qint64 limit) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read " + path + ": " + file.errorString());
    auto data = file.read(limit + 1);
    require(data.size() <= limit, "File exceeds read limit: " + path);
    require(file.error() == QFile::NoError, "Read failed: " + path);
    return data;
}
void writeFile(const QString &path, const QByteArray &data) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "Cannot create parent: " + path);
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot write " + path);
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    require(file.write(data) == data.size() && file.commit(), "Cannot commit file: " + path);
}
QJsonObject readJson(const QString &path) {
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(readFile(path), &error);
    require(error.error == QJsonParseError::NoError && doc.isObject(), "Invalid JSON: " + path);
    return doc.object();
}
QString quote(const QString &value) {
    auto result = value;
    result.replace("'", "'\\''");
    return "'" + result + "'";
}
CommandResult run(const QString &program, const QStringList &args, int timeout,
                  const QProcessEnvironment &env) {
    QProcess process;
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, args);
    if (!process.waitForStarted(3000))
        return {-1, false, process.errorString()};
    bool timedOut = !process.waitForFinished(timeout);
    if (timedOut) {
        process.kill();
        process.waitForFinished(3000);
    }
    return {process.exitStatus() == QProcess::CrashExit ? -1 : process.exitCode(), timedOut,
            QString::fromUtf8(process.readAll())};
}
Profile Profile::load() { return forVersion("12.61").value(); }
std::optional<Profile> Profile::forVersion(const QString &version) {
    static const bool initialized = (initProfiles(), true);
    Q_UNUSED(initialized);
    if (!QRegularExpression("^[0-9]{1,2}\\.[0-9]{1,2}$").match(version).hasMatch())
        return {};
    auto resource = ":/profiles/fortnite/" + version + ".toml";
    if (!QFile::exists(resource))
        return {};
    auto table = toml::parse(readFile(resource).toStdString());
    auto str = [&](const char *key) { return QString::fromStdString(table[key].value_or("")); };
    Profile p{str("version"),
              str("executable"),
              str("working_directory"),
              str("reboot_status"),
              {},
              {},
              {},
              {},
              table["private_path_verified"].value_or(false)};
    for (auto &arg : *table["arguments"].as_array())
        p.arguments.append(QString::fromStdString(arg.value_or("")));
    if (auto arguments = table["launcher_context_arguments"].as_array())
        for (auto &arg : *arguments)
            p.launcherContextArguments.append(QString::fromStdString(arg.value_or("")));
    if (auto arguments = table["local_backend_arguments"].as_array())
        for (auto &arg : *arguments)
            p.localBackendArguments.append(QString::fromStdString(arg.value_or("")));
    if (auto arguments = table["service_ini_arguments"].as_array())
        for (auto &arg : *arguments)
            p.serviceIniArguments.append(QString::fromStdString(arg.value_or("")));
    require(p.version == version, "Embedded compatibility profile version mismatch");
    return p;
}
QMap<QString, QString> Registry::list() const {
    QMap<QString, QString> result;
    QString file = paths.config + "/builds.toml";
    if (!QFile::exists(file))
        return result;
    auto table = toml::parse(readFile(file).toStdString());
    require(table.size() == 1 && table["builds"].is_table(),
            "Invalid registry schema: expected only a [builds] table; file preserved");
    if (auto builds = table["builds"].as_table()) {
        for (auto &&[key, val] : *builds) {
            auto path = val.value<std::string>();
            require(path.has_value(), "Invalid path in build registry");
            require(QDir::isAbsolutePath(QString::fromStdString(*path)),
                    "Registry paths must be absolute; file preserved");
            QString id = QString::fromStdString(std::string(key.str()));
            validateId(id);
            result.insert(id, QString::fromStdString(*path));
        }
    }
    return result;
}
static void saveRegistry(const Paths &paths, const QMap<QString, QString> &builds) {
    toml::table entries;
    for (auto it = builds.begin(); it != builds.end(); ++it)
        entries.insert(it.key().toStdString(), it.value().toStdString());
    toml::table doc{{"builds", entries}};
    std::ostringstream out;
    out << doc;
    writeFile(paths.config + "/builds.toml", QByteArray::fromStdString(out.str()));
}
void Registry::add(const QString &id, const QString &path) {
    validateId(id);
    auto build = inspectBuild(path);
    require(build.valid(), "Invalid installation: " + build.errors.join("; "));
    require(QDir().mkpath(paths.config), "Cannot create config directory");
    QLockFile lock(paths.config + "/registry.lock");
    require(lock.tryLock(), "Build registry is busy");
    auto builds = list();
    require(!builds.contains(id) || builds[id] == build.root,
            "ID already registered to another path");
    builds.insert(id, build.root);
    saveRegistry(paths, builds);
}
void Registry::remove(const QString &id) {
    validateId(id);
    require(QDir().mkpath(paths.config), "Cannot create config directory");
    QLockFile lock(paths.config + "/registry.lock");
    require(lock.tryLock(), "Build registry is busy");
    auto builds = list();
    require(builds.remove(id) != 0, "Build is not registered: " + id);
    saveRegistry(paths, builds);
}
QString Registry::resolve(const QString &id) const {
    auto builds = list();
    require(builds.contains(id), "Build is not registered: " + id + "; use add /path/to/Fortnite");
    return builds[id];
}
} // namespace fol
