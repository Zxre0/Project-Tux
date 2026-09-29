#include "fol/core.h"
#include <QCryptographicHash>
#include <QFile>
#include <toml++/toml.hpp>

namespace fol {
QJsonObject moduleReport(const QString &path, const QString &manifest) {
    auto pe = inspectPe(path);
    QJsonObject result{{"module", path},
                       {"expected_version", Profile::load().version},
                       {"architecture", pe.x64 ? "x64" : "unknown/unsupported"},
                       {"status", "invalid"},
                       {"error", pe.error},
                       {"load_supported", false}};
    if (!pe.valid || !pe.x64 || !pe.dll)
        return result;
    result["status"] = "unverified";
    if (manifest.isEmpty()) {
        result["error"] = "Compatibility manifest and SHA-256 required";
        return result;
    }
    auto table = toml::parse(readFile(manifest).toStdString());
    auto version = QString::fromStdString(table["fortnite_version"].value_or(""));
    auto expected = QByteArray::fromStdString(table["sha256"].value_or(""));
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read module");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&file), "Module hash read failed");
    auto actual = hash.result().toHex();
    result["sha256"] = QString::fromLatin1(actual);
    bool matches =
        version == Profile::load().version && expected.size() == 64 && expected.toLower() == actual;
    result["status"] = matches ? "manifest-matches; runtime-compatibility-unverified" : "invalid";
    result["error"] = matches ? "" : "Manifest version or SHA-256 does not match";
    return result;
}
} // namespace fol
