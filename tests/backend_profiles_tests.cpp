#include "fol/backend.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <stdexcept>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    unsigned checks = 0;
    const auto check = [&](bool condition, const char *message) {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    };
    try {
        const QByteArray password = "synthetic-profile-test-password";
        const auto identity = fol::backend::Identity::create("player@localhost", "Local", password);
        fol::backend::Service service(identity);
        const auto token = service.handle(
            "POST", "/account/api/oauth/token",
            {{"content-type", "application/x-www-form-urlencoded"}},
            "grant_type=password&username=player%40localhost&password=" + password, 1000);
        check(token.status == 200, "test player authentication");
        QMap<QByteArray, QByteArray> auth{
            {"authorization", "Bearer " + token.json["access_token"].toString().toLatin1()},
            {"content-type", "application/json"}};
        const auto path =
            "/fortnite/api/game/v2/profile/" + identity.accountId + "/client/QueryProfile";
        const auto query = [&](const QString &params, const QByteArray &body = "{}",
                               qint64 now = 1001) {
            return service.handle("POST", path, auth, body, now, QUrlQuery(params));
        };
        const auto initial = query("profileId=common_core&rvn=-1");
        check(initial.status == 200 && initial.json["profileRevision"] == 1 &&
                  initial.json["profileCommandRevision"] == 0,
              "initial read-only profile revision");
        const auto changes = initial.json["profileChanges"].toArray();
        check(changes.size() == 1 && changes[0].toObject()["changeType"] == "fullProfileUpdate",
              "initial query sends full snapshot");
        const auto profile = changes[0].toObject()["profile"].toObject();
        check(profile["accountId"] == identity.accountId && profile["profileId"] == "common_core",
              "snapshot belongs to the local account and requested profile");
        check(profile["items"].toObject().isEmpty() && !profile.contains("entitlements"),
              "no invented inventory or entitlements");
        const auto repeated = query("profileId=common_core&rvn=-1", "{}", 1010);
        check(repeated.json["profileChanges"].toArray()[0].toObject()["profile"] == profile &&
                  repeated.json["serverTime"] != initial.json["serverTime"],
              "read-only snapshot stable within backend lifetime; server time advances");
        check(query("profileId=common_core&rvn=1").json["profileChanges"].toArray().isEmpty(),
              "current revision yields no changes");
        check(query("profileId=common_core&rvn=200").json["profileChanges"].toArray().size() == 1,
              "unexpected client revision resynchronizes with full local snapshot");
        const auto publicProfile = query("profileId=common_public&rvn=-1")
                                       .json["profileChanges"]
                                       .toArray()[0]
                                       .toObject()["profile"]
                                       .toObject();
        check(publicProfile["profileId"] == "common_public" &&
                  publicProfile["stats"].toObject()["attributes"].toObject()["homebase_name"] ==
                      "Local",
              "separate public profile contains local display metadata");
        check(query("profileId=athena&rvn=-1").status == 404, "unimplemented profile rejected");
        check(query("profileId=common_core&profileId=common_public&rvn=-1").status == 400,
              "duplicate profile ID rejected");
        check(query("profileId=common_core&rvn=-1&rvn=1").status == 400,
              "duplicate revision rejected");
        check(query("profileId=common_core").status == 400, "missing revision rejected");
        for (const auto &revision : {"-2", "1.0", "2147483648", "garbage", "01"})
            check(query("profileId=common_core&rvn=" + QString(revision)).status == 400,
                  "malformed or overflowing revision rejected");
        check(query("profileId=common_core&rvn=-1", "[]").status == 400 &&
                  query("profileId=common_core&rvn=-1", "{bad").status == 400 &&
                  query("profileId=common_core&rvn=-1", "{\"grant\":true}").status == 400,
              "read query rejects malformed, array and mutation bodies");
        check(
            service.handle("POST", path, {}, "{}", 1001, QUrlQuery("profileId=common_core&rvn=-1"))
                    .status == 401,
            "profile query requires authenticated player");
        const auto application =
            service.handle("POST", "/account/api/oauth/token",
                           {{"content-type", "application/x-www-form-urlencoded"}},
                           "grant_type=client_credentials", 1001);
        const QMap<QByteArray, QByteArray> appHeaders{
            {"authorization", "Bearer " + application.json["access_token"].toString().toLatin1()},
            {"content-type", "application/json"}};
        check(application.status == 200 && service.handle("POST", path, appHeaders, "{}", 1001,
                                                          QUrlQuery("profileId=common_core&rvn=-1"))
                                                   .status == 401,
              "live public application token cannot read player profiles");
        check(service.handle("POST", path + "Other", auth, "{}", 1001,
                             QUrlQuery("profileId=common_core&rvn=-1"))
                      .status == 404,
              "unknown command is not silently successful");
        auto otherPath = path;
        otherPath.replace(identity.accountId, "other-account");
        check(service.handle("POST", otherPath, auth, "{}", 1001,
                             QUrlQuery("profileId=common_core&rvn=-1"))
                      .status == 404,
              "cannot read another account's profile");
        auth["content-type"] = "text/plain";
        check(query("profileId=common_core&rvn=-1").status == 415, "JSON content type required");
        auth["content-type"] = "application/json";
        check(query("profileId=common_core&rvn=-1", "{}", 4600).status == 401,
              "expired player token rejected");
        QTextStream(stdout) << checks << " backend profile checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << "Profile check " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
