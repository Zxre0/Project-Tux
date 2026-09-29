#pragma once
#include "fol/backend_profiles.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QUrlQuery>
#include <optional>

namespace fol::backend {
struct Identity {
    QString username, displayName, accountId;
    QByteArray salt, passwordHash;
    static Identity create(const QString &username, const QString &displayName,
                           const QByteArray &password);
};
struct Response {
    int status;
    QJsonObject json;
    QString event; // Fixed diagnostic category; never a request body or token.
    std::optional<QJsonArray> array = std::nullopt;
    std::optional<bool> boolean = std::nullopt;
    QString platformDiagnostic = {}; // Only a validated short tag; never a raw query.
    QString routeDiagnostic = {};    // Fixed segments and redacted account placeholders only.
};
class Service {
    struct Session {
        bool player;
        qint64 expires;
        QString refresh;
    };
    Identity identity;
    Profiles profiles;
    QHash<QString, Session> sessions;
    QHash<QString, QPair<QString, qint64>> refreshTokens;
    Response issue(bool player, qint64 now);
    void revoke(const QString &token);

  public:
    explicit Service(Identity identity);
    Response handle(const QByteArray &method, const QString &path,
                    const QMap<QByteArray, QByteArray> &headers, const QByteArray &body, qint64 now,
                    const QUrlQuery &query = {});
};
} // namespace fol::backend
