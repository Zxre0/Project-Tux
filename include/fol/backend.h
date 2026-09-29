#pragma once
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QString>

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
};
class Service {
    struct Session {
        bool player;
        qint64 expires;
        QString refresh;
    };
    Identity identity;
    QHash<QString, Session> sessions;
    QHash<QString, QPair<QString, qint64>> refreshTokens;
    Response issue(bool player, qint64 now);
    void revoke(const QString &token);

  public:
    explicit Service(Identity identity);
    Response handle(const QByteArray &method, const QString &path,
                    const QMap<QByteArray, QByteArray> &headers, const QByteArray &body,
                    qint64 now);
};
} // namespace fol::backend
