#include "fol/backend.h"
#include "fol/core.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>

namespace fol::backend {
static QByteArray randomBytes(unsigned size) {
    QByteArray result;
    result.reserve(size);
    for (unsigned i = 0; i < size; ++i)
        result.append(char(QRandomGenerator::system()->generate() & 255));
    return result;
}
static QByteArray digest(const QByteArray &password, const QByteArray &salt) {
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, password, salt, 100000,
                                              32);
}
static bool equalDigest(const QByteArray &a, const QByteArray &b) {
    if (a.size() != 32 || b.size() != 32)
        return false;
    unsigned difference = 0;
    for (int i = 0; i < 32; ++i)
        difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    return difference == 0;
}
static QString expiry(qint64 now, int seconds) {
    return QDateTime::fromSecsSinceEpoch(now + seconds).toUTC().toString(Qt::ISODateWithMs);
}
static Response failure(int status, const QString &code) {
    return {status,
            {{"errorCode", "errors.fortniteonlinux." + code},
             {"errorMessage", "Local development service: " + code},
             {"numericErrorCode", status},
             {"originatingService", "fortnite-on-linux"},
             {"intent", "local-development"}},
            code};
}
Identity Identity::create(const QString &username, const QString &displayName,
                          const QByteArray &password) {
    static const QRegularExpression localName("\\A[a-z0-9_-]{1,32}@localhost\\z");
    require(localName.match(username).hasMatch(), "Development username must end in @localhost");
    require(!displayName.isEmpty() && displayName.size() <= 32, "Invalid development display name");
    require(password.size() >= 16 && password.size() <= 256, "Local password must be 16-256 bytes");
    Identity result{
        username, displayName, QString::fromLatin1(randomBytes(16).toHex()), randomBytes(32), {}};
    result.passwordHash = digest(password, result.salt);
    return result;
}
Service::Service(Identity value) : identity(std::move(value)) {
    require(identity.username.endsWith("@localhost") && identity.salt.size() == 32 &&
                identity.passwordHash.size() == 32 && identity.accountId.size() == 32,
            "Invalid private backend identity");
}
Response Service::issue(bool player, qint64 now) {
    if (sessions.size() >= 1024)
        return failure(429, "session_limit");
    const auto token = QString::fromLatin1(randomBytes(32).toHex());
    const auto refresh = player ? QString::fromLatin1(randomBytes(32).toHex()) : QString{};
    sessions.insert(token, {player, now + 3600, refresh});
    QJsonObject response{{"access_token", token},
                         {"expires_in", 3600},
                         {"expires_at", expiry(now, 3600)},
                         {"token_type", "bearer"},
                         {"client_id", "fortnite-on-linux-local"},
                         {"client_service", "fortnite"},
                         {"internal_client", true},
                         {"app", "fortnite"}};
    if (player) {
        refreshTokens.insert(refresh, {token, now + 86400});
        response["account_id"] = identity.accountId;
        response["in_app_id"] = identity.accountId;
        response["displayName"] = identity.displayName;
        response["refresh_token"] = refresh;
        response["refresh_expires"] = 86400;
        response["refresh_expires_at"] = expiry(now, 86400);
    }
    return {200, response,
            player ? "local-player-token-issued" : "local-public-client-token-issued"};
}
void Service::revoke(const QString &token) {
    const auto found = sessions.find(token);
    if (found != sessions.end()) {
        refreshTokens.remove(found->refresh);
        sessions.erase(found);
    }
}
Response Service::handle(const QByteArray &method, const QString &path,
                         const QMap<QByteArray, QByteArray> &headers, const QByteArray &body,
                         qint64 now, const QUrlQuery &query) {
    if (body.size() > 16384 || path.size() > 2048)
        return failure(413, "request_too_large");
    for (auto it = sessions.begin(); it != sessions.end();) {
        // Retain the access entry while a refresh token is still valid, so
        // revocation and rotation can invalidate both members of the pair.
        if (it->expires <= now &&
            (it->refresh.isEmpty() || refreshTokens.value(it->refresh).second <= now)) {
            refreshTokens.remove(it->refresh);
            it = sessions.erase(it);
        } else
            ++it;
    }
    if (method == "GET" && path == "/health")
        return {200, {{"service", "fortnite-on-linux"}, {"mode", "private-development"}}, "health"};
    if (method == "GET" &&
        (path == "/fortnite/api/v2/versioncheck" ||
         path.startsWith("/fortnite/api/v2/versioncheck/") ||
         path == "/fortnite/api/versioncheck" || path.startsWith("/fortnite/api/versioncheck/")))
        return {200, {{"type", "NO_UPDATE"}}, "local-versioncheck-no-update"};
    if (method == "POST" && path == "/account/api/oauth/token") {
        if (!headers.value("content-type").startsWith("application/x-www-form-urlencoded"))
            return failure(415, "form_required");
        QMap<QByteArray, QByteArray> form;
        for (auto entry : body.split('&')) {
            const auto split = entry.indexOf('=');
            if (split < 1)
                return failure(400, "invalid_form");
            auto key = entry.left(split).replace('+', ' ');
            auto value = entry.mid(split + 1).replace('+', ' ');
            key = QByteArray::fromPercentEncoding(key);
            value = QByteArray::fromPercentEncoding(value);
            if (form.contains(key))
                return failure(400, "duplicate_form_field");
            form.insert(key, value);
        }
        const auto grant = form.value("grant_type");
        if (grant == "client_credentials") {
            // Deliberately public, loopback-only application bootstrap. This
            // grants no player identity and does not validate Epic credentials.
            return issue(false, now);
        }
        if (grant == "password") {
            const auto password = form.value("password");
            if (password.size() > 256)
                return failure(400, "invalid_local_credentials");
            const bool valid = equalDigest(digest(password, identity.salt), identity.passwordHash);
            if (!valid || QString::fromUtf8(form.value("username")) != identity.username)
                return failure(400, "invalid_local_credentials");
            return issue(true, now);
        }
        if (grant == "refresh_token") {
            const auto refresh = QString::fromUtf8(form.value("refresh_token"));
            const auto found = refreshTokens.constFind(refresh);
            if (found == refreshTokens.cend() || found->second <= now)
                return failure(400, "invalid_refresh_token");
            const auto old = found->first;
            revoke(old);
            return issue(true, now);
        }
        return failure(400, "unsupported_grant_type");
    }
    const auto authorization = headers.value("authorization");
    QString token;
    if (authorization.left(7).toLower() == "bearer " && authorization.size() == 71)
        token = QString::fromLatin1(authorization.mid(7));
    const auto found = sessions.constFind(token);
    const bool authorized = found != sessions.cend() && found->expires > now;
    if (method == "GET" && path == "/fortnite/api/cloudstorage/system") {
        if (!authorized)
            return failure(401, "invalid_access_token");
        // This private service has no configured hotfix files. Do not invent
        // asset hashes or copy upstream security-affecting INI files.
        return {200, {}, "local-cloud-system-index", QJsonArray{}};
    }
    if (method == "GET" && path == "/account/api/oauth/verify") {
        if (!authorized)
            return failure(401, "invalid_access_token");
        QJsonObject response{{"token", token},
                             {"token_type", "bearer"},
                             {"client_id", "fortnite-on-linux-local"},
                             {"client_service", "fortnite"},
                             {"internal_client", true},
                             {"app", "fortnite"},
                             {"expires_in", found->expires - now},
                             {"expires_at", expiry(now, int(found->expires - now))}};
        if (found->player) {
            response["account_id"] = identity.accountId;
            response["display_name"] = identity.displayName;
            response["in_app_id"] = identity.accountId;
            response["auth_method"] = "password";
        }
        return {200, response, "local-token-verified"};
    }
    if (method == "DELETE" && (path == "/account/api/oauth/sessions/kill" ||
                               path == "/account/api/oauth/sessions/kill/" + token)) {
        if (!authorized)
            return failure(401, "invalid_access_token");
        if (query.allQueryItemValues("killType").size() > 1)
            return failure(400, "duplicate_kill_type");
        const auto killType = query.queryItemValue("killType");
        if (killType == "OTHERS_ACCOUNT_CLIENT_SERVICE" &&
            path == "/account/api/oauth/sessions/kill") {
            if (!found->player)
                return failure(401, "player_authentication_required");
            QStringList others;
            for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
                if (it.key() != token && it->player)
                    others.append(it.key());
            for (const auto &other : others)
                revoke(other);
            return {204, {}, "other-local-player-sessions-revoked"};
        }
        if (!killType.isEmpty())
            return failure(400, "unsupported_kill_type");
        revoke(token);
        return {204, {}, "local-session-revoked"};
    }
    if (method == "POST" &&
        path == "/fortnite/api/game/v2/tryPlayOnPlatform/account/" + identity.accountId) {
        if (!authorized || !found->player)
            return failure(401, "player_authentication_required");
        const auto platforms = query.allQueryItemValues("platform");
        if (platforms.size() != 1)
            return failure(400, "one_platform_required");
        // This is this private service's policy for its own local account, not
        // an Epic entitlement or an assertion about production platform access.
        // Observed from the actual 12.50 request, not inferred from profile metadata.
        const bool allowed = platforms[0] == "PC";
        static const QRegularExpression platformTag("\\A[A-Za-z][A-Za-z0-9_-]{0,15}\\z");
        return {200,
                {},
                allowed ? "local-platform-allowed" : "local-platform-denied",
                std::nullopt,
                allowed,
                platformTag.match(platforms[0]).hasMatch() ? platforms[0] : QString{}};
    }
    if (method == "GET" && path == "/account/api/public/account") {
        if (!authorized || !found->player)
            return failure(401, "player_authentication_required");
        const auto requested = query.allQueryItemValues("accountId");
        if (requested.isEmpty() || requested.size() > 100)
            return failure(400, "invalid_account_query");
        QJsonArray accounts;
        if (requested.contains(identity.accountId))
            accounts.append(QJsonObject{{"id", identity.accountId},
                                        {"displayName", identity.displayName},
                                        {"externalAuths", QJsonObject{}}});
        return {200, {}, "local-account-list", accounts};
    }
    if (method == "GET" &&
        path == "/account/api/public/account/" + identity.accountId + "/externalAuths") {
        if (!authorized || !found->player)
            return failure(401, "player_authentication_required");
        return {200, {}, "local-external-auth-list", QJsonArray{}};
    }
    if (method == "GET" && path == "/account/api/public/account/" + identity.accountId) {
        if (!authorized || !found->player)
            return failure(401, "player_authentication_required");
        return {200,
                {{"id", identity.accountId},
                 {"displayName", identity.displayName},
                 {"email", identity.username},
                 {"country", "US"},
                 {"preferredLanguage", "en"},
                 {"canUpdateDisplayName", false}},
                "local-account-read"};
    }
    auto missing = failure(404, "route_not_implemented");
    const QMap<QString, QString> categories{{"/account/api/", "account"},
                                            {"/fortnite/api/", "fortnite"},
                                            {"/lightswitch/api/", "lightswitch"},
                                            {"/content/api/", "content"},
                                            {"/launcher/api/", "launcher"}};
    for (auto it = categories.cbegin(); it != categories.cend(); ++it)
        if (path.startsWith(it.key())) {
            missing.event = "unimplemented-" + it.value();
            break;
        }
    return missing;
}
} // namespace fol::backend
