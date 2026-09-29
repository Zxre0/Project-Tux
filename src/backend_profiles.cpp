#include "fol/backend_profiles.h"
#include "fol/core.h"
#include <QDateTime>
#include <QJsonArray>

namespace fol::backend {
Profiles::Profiles(QString account, QString name)
    : accountId(std::move(account)), displayName(std::move(name)) {}
bool Profiles::supported(const QString &id) { return id == "common_public" || id == "common_core"; }
QJsonObject Profiles::query(const QString &id, int revision, qint64 now) {
    require(supported(id) && revision >= -1, "Unsupported local profile query");
    const auto time = QDateTime::fromSecsSinceEpoch(now).toUTC().toString(Qt::ISODateWithMs);
    if (!snapshots.contains(id)) {
        QJsonObject attributes;
        if (id == "common_public")
            attributes["homebase_name"] = displayName;
        else {
            attributes["current_mtx_platform"] = "EpicPC";
            attributes["allowed_to_receive_gifts"] = false;
            attributes["allowed_to_send_gifts"] = false;
        }
        snapshots.insert(id, {{"_id", accountId + ":" + id},
                              {"accountId", accountId},
                              {"profileId", id},
                              {"created", time},
                              {"updated", time},
                              {"rvn", 1},
                              {"commandRevision", 0},
                              {"wipeNumber", 1},
                              {"version", "no_version"},
                              {"items", QJsonObject{}},
                              {"stats", QJsonObject{{"attributes", attributes}}}});
    }
    QJsonArray changes;
    if (revision != 1)
        changes.append(
            QJsonObject{{"changeType", "fullProfileUpdate"}, {"profile", snapshots.value(id)}});
    return {{"profileRevision", 1},
            {"profileId", id},
            {"profileChangesBaseRevision", 1},
            {"profileChanges", changes},
            {"profileCommandRevision", 0},
            {"serverTime", time},
            {"responseVersion", 1}};
}
} // namespace fol::backend
