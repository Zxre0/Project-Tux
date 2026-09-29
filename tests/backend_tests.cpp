#include "fol/backend.h"
#include <QCoreApplication>
#include <QTextStream>
#include <stdexcept>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    unsigned checks = 0;
    const auto check = [&](bool value, const char *description) {
        ++checks;
        if (!value)
            throw std::runtime_error(description);
    };
    try {
        const QByteArray password = "synthetic-password-for-tests-only";
        const auto identity =
            fol::backend::Identity::create("player@localhost", "Local Player", password);
        fol::backend::Service service(identity);
        const QMap<QByteArray, QByteArray> form{
            {"content-type", "application/x-www-form-urlencoded"}};
        const auto post = [&](const QByteArray &body, qint64 now = 1000) {
            return service.handle("POST", "/account/api/oauth/token", form, body, now);
        };
        check(service.handle("GET", "/health", {}, {}, 1000).status == 200, "health");
        check(service.handle("GET", "/fortnite/api/v2/versioncheck/Windows", {}, {}, 1000)
                      .json["type"] == "NO_UPDATE",
              "local service does not distribute game updates");
        const auto application = post("grant_type=client_credentials");
        check(application.status == 200 && !application.json.contains("account_id"),
              "public client has no player identity");
        QMap<QByteArray, QByteArray> auth{
            {"authorization", "Bearer " + application.json["access_token"].toString().toLatin1()}};
        auth["authorization"].replace("Bearer ", "bearer ");
        const auto cloud =
            service.handle("GET", "/fortnite/api/cloudstorage/system", auth, {}, 1000);
        check(cloud.status == 200 && cloud.array && cloud.array->isEmpty(),
              "authenticated empty private hotfix index");
        check(service.handle("GET", "/fortnite/api/cloudstorage/system", {}, {}, 1000).status ==
                  401,
              "cloud index requires issued application token");
        check(service.handle("DELETE", "/account/api/oauth/sessions/kill", auth, {}, 1000,
                             QUrlQuery("killType=OTHERS_ACCOUNT_CLIENT_SERVICE"))
                      .status == 401,
              "application token cannot revoke player sessions");
        check(service.handle("GET", "/account/api/public/account/" + identity.accountId, auth, {},
                             1000)
                      .status == 401,
              "application token cannot read player account");
        check(post("grant_type=password&username=player%40localhost&password=wrong").status == 400,
              "wrong password");
        check(post("grant_type=password&username=real%40example.com&password=" + password).status ==
                  400,
              "nonlocal account rejected");
        check(post("grant_type=password&grant_type=client_credentials").status == 400,
              "duplicate grant rejected");
        check(post("grant_type=exchange_code&exchange_code=not-issued").status == 400,
              "unissued exchange rejected");
        check(service.handle("POST", "/account/api/oauth/token", {}, "grant_type=password", 1000)
                      .status == 415,
              "form type required");
        const auto player =
            post("grant_type=password&username=player%40localhost&password=" + password);
        check(player.status == 200 && player.json["account_id"] == identity.accountId,
              "local password authentication");
        check(player.json["access_token"] != application.json["access_token"], "tokens unique");
        auth["authorization"] = "Bearer " + player.json["access_token"].toString().toLatin1();
        check(
            service.handle("GET", "/account/api/oauth/verify", auth, {}, 1000).json["account_id"] ==
                identity.accountId,
            "verify authenticated player");
        check(service.handle("GET", "/account/api/public/account/" + identity.accountId, auth, {},
                             1000)
                      .status == 200,
              "authorized account read");
        const auto other =
            post("grant_type=password&username=player%40localhost&password=" + password);
        const QMap<QByteArray, QByteArray> otherAuth{
            {"authorization", "Bearer " + other.json["access_token"].toString().toLatin1()}};
        check(service.handle("DELETE", "/account/api/oauth/sessions/kill", auth, {}, 1001,
                             QUrlQuery("killType=OTHERS_ACCOUNT_CLIENT_SERVICE"))
                      .status == 204,
              "kill other sessions request accepted");
        check(service.handle("GET", "/account/api/oauth/verify", auth, {}, 1002).status == 200,
              "kill others preserves current player session");
        check(service.handle("GET", "/account/api/oauth/verify", otherAuth, {}, 1002).status == 401,
              "kill others revokes the other player session");
        check(post("grant_type=refresh_token&refresh_token=" +
                       other.json["refresh_token"].toString().toLatin1(),
                   1002)
                      .status == 400,
              "kill others also revokes other refresh token");
        check(service.handle("DELETE", "/account/api/oauth/sessions/kill", auth, {}, 1002,
                             QUrlQuery("killType=unknown"))
                      .status == 400,
              "unknown session kill mode rejected");
        check(service.handle("DELETE", "/account/api/oauth/sessions/kill", auth, {}, 1002,
                             QUrlQuery("killType=OTHERS_ACCOUNT_CLIENT_SERVICE&killType=unknown"))
                      .status == 400,
              "ambiguous session kill mode rejected");
        check(service.handle("GET", "/account/api/oauth/verify", auth, {}, 1002).status == 200,
              "rejected kill modes preserve caller token");
        const auto external = service.handle(
            "GET", "/account/api/public/account/" + identity.accountId + "/externalAuths", auth, {},
            1002);
        check(external.status == 200 && external.array && external.array->isEmpty(),
              "local account has no external auth bindings");
        const auto platformPath =
            "/fortnite/api/game/v2/tryPlayOnPlatform/account/" + identity.accountId;
        check(
            service.handle("POST", platformPath, {}, {}, 1002, QUrlQuery("platform=WIN")).status ==
                401,
            "local platform policy requires authentication");
        const auto platform =
            service.handle("POST", platformPath, auth, {}, 1002, QUrlQuery("platform=PC"));
        check(platform.status == 200 && platform.boolean && *platform.boolean,
              "local player may use Windows components on the private service");
        const auto lowerPlatform =
            service.handle("POST", platformPath, auth, {}, 1002, QUrlQuery("platform=win"));
        check(lowerPlatform.status == 200 && lowerPlatform.boolean && !*lowerPlatform.boolean,
              "unobserved Windows label does not silently grant platform access");
        const auto epicPcPlatform =
            service.handle("POST", platformPath, auth, {}, 1002, QUrlQuery("platform=EpicPC"));
        check(epicPcPlatform.status == 200 && epicPcPlatform.boolean && !*epicPcPlatform.boolean &&
                  epicPcPlatform.event == "local-platform-denied" &&
                  epicPcPlatform.platformDiagnostic == "EpicPC",
              "profile metadata label is not confused with the observed platform label");
        const auto longTag = service.handle("POST", platformPath, auth, {}, 1002,
                                            QUrlQuery("platform=0123456789abcdef0123456789abcdef"));
        const auto invalidTag = service.handle("POST", platformPath, auth, {}, 1002,
                                               QUrlQuery("platform=user%40localhost"));
        check(longTag.platformDiagnostic.isEmpty() && invalidTag.platformDiagnostic.isEmpty(),
              "platform diagnostics exclude credential-shaped and arbitrary query values");
        const auto unsupported =
            service.handle("POST", platformPath, auth, {}, 1002, QUrlQuery("platform=unknown"));
        check(unsupported.status == 200 && unsupported.boolean && !*unsupported.boolean,
              "unknown platforms are not granted private access");
        check(service.handle("POST", platformPath, auth, {}, 1002,
                             QUrlQuery("platform=WIN&platform=unknown"))
                      .status == 400,
              "ambiguous platform rejected");
        check(service.handle("POST", platformPath + "-other", auth, {}, 1002,
                             QUrlQuery("platform=WIN"))
                      .status == 404,
              "local policy does not grant access to another account");
        QUrlQuery accountQuery;
        accountQuery.addQueryItem("accountId", identity.accountId);
        accountQuery.addQueryItem("accountId", "unknown-local-account");
        const auto accounts =
            service.handle("GET", "/account/api/public/account", auth, {}, 1002, accountQuery);
        check(accounts.status == 200 && accounts.array && accounts.array->size() == 1 &&
                  accounts.array->at(0).toObject()["id"] == identity.accountId,
              "account list returns only an existing local identity");
        check(service.handle("GET", "/account/api/public/account", {}, {}, 1002, accountQuery)
                      .status == 401,
              "account list requires player authentication");
        check(service.handle("GET", "/account/api/oauth/verify", auth, {}, 4600).status == 401,
              "expired access rejected");
        const auto refresh = player.json["refresh_token"].toString().toLatin1();
        const auto rotated = post("grant_type=refresh_token&refresh_token=" + refresh, 4600);
        check(rotated.status == 200 && rotated.json["access_token"] != player.json["access_token"],
              "refresh rotates tokens");
        check(post("grant_type=refresh_token&refresh_token=" + refresh, 4601).status == 400,
              "refresh cannot be replayed");
        auth["authorization"] = "Bearer " + rotated.json["access_token"].toString().toLatin1();
        check(service.handle("DELETE", "/account/api/oauth/sessions/kill", auth, {}, 4602).status ==
                  204,
              "logout");
        check(service.handle("GET", "/account/api/oauth/verify", auth, {}, 4603).status == 401,
              "revoked access rejected");
        check(post("grant_type=refresh_token&refresh_token=" +
                       rotated.json["refresh_token"].toString().toLatin1(),
                   4603)
                      .status == 400,
              "logout revokes refresh");
        check(service.handle("GET", "/account/api/oauth/verify", {}, {}, 1000).status == 401,
              "missing bearer rejected");
        check(service.handle("POST", "/account/api/oauth/token", form, QByteArray(16385, 'x'), 1000)
                      .status == 413,
              "oversized input rejected");
        QTextStream(stdout) << checks << " backend authentication checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << "Backend check " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
