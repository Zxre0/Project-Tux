#pragma once
#include <QHash>
#include <QJsonObject>
#include <QString>

namespace fol::backend {
// Read-only, in-memory local profiles. No purchases, items or external identity.
class Profiles {
    QString accountId, displayName;
    QHash<QString, QJsonObject> snapshots;

  public:
    Profiles(QString accountId, QString displayName);
    static bool supported(const QString &id);
    QJsonObject query(const QString &id, int revision, qint64 now);
};
} // namespace fol::backend
