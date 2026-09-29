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
