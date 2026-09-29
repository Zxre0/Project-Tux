#include "fol/backend_http.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <stdexcept>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    unsigned checks = 0;
    const auto check = [&](bool condition, const char *description) {
        ++checks;
        if (!condition)
            throw std::runtime_error(description);
    };
    try {
        const QByteArray password = "synthetic-http-password-not-a-real-secret";
        const auto identity = fol::backend::Identity::create("player@localhost", "Test", password);
        QJsonArray logs;
        fol::backend::HttpServer server(identity,
                                        [&](const QJsonObject &entry) { logs.append(entry); });
        if (!server.start(0))
            throw std::runtime_error(("loopback bind: " + server.errorString()).toStdString());
        check(server.serverAddress() == QHostAddress::LocalHost, "loopback bind");
        QNetworkAccessManager network;
        network.setProxy(QNetworkProxy::NoProxy);
        const auto request = [&](const QString &path, const QByteArray &body,
                                 const QByteArray &token = QByteArray{},
                                 const QByteArray &method = QByteArray{}) {
            QNetworkRequest input(
                QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) + path));
            if (!token.isEmpty())
                input.setRawHeader("Authorization", "Bearer " + token);
            input.setHeader(QNetworkRequest::ContentTypeHeader,
                            "application/x-www-form-urlencoded");
            auto *reply = !method.isEmpty() ? network.sendCustomRequest(input, method, body)
                          : body.isEmpty()  ? network.get(input)
                                            : network.post(input, body);
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            timeout.start(5000);
            loop.exec();
            check(reply->isFinished(), "HTTP response deadline");
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto result = QJsonDocument::fromJson(reply->readAll()).object();
            reply->deleteLater();
            return qMakePair(status, result);
        };
        check(request("/health", {}).first == 200, "HTTP health");
        check(request("/account/api/oauth/verify", {}).first == 401,
              "HTTP missing bearer rejected");
        const auto login =
            request("/account/api/oauth/token",
                    "grant_type=password&username=player%40localhost&password=" + password);
        check(login.first == 200 && login.second["account_id"] == identity.accountId,
              "HTTP local login");
        const auto token = login.second["access_token"].toString().toLatin1();
        check(request("/account/api/oauth/verify", {}, token).first == 200,
              "HTTP verify issued token");
        const auto otherLogin =
            request("/account/api/oauth/token",
                    "grant_type=password&username=player%40localhost&password=" + password);
        const auto otherToken = otherLogin.second["access_token"].toString().toLatin1();
        check(otherLogin.first == 200 && !otherToken.isEmpty() && otherToken != token,
              "HTTP second player session");
        check(request("/account/api/oauth/sessions/kill?killType=OTHERS_ACCOUNT_CLIENT_SERVICE", {},
                      token, "DELETE")
                      .first == 204,
              "HTTP kill other sessions query");
        check(request("/account/api/oauth/verify", {}, token).first == 200,
              "HTTP kill others preserves the caller token");
        check(request("/account/api/oauth/verify", {}, otherToken).first == 401,
              "HTTP kill others revokes the other token");
        check(request("/account/api/oauth/sessions/kill?killType=unknown", {}, token, "DELETE")
                      .first == 400,
              "HTTP unsupported kill mode does not become a logout");
        check(request("/account/api/oauth/verify", {}, token).first == 200,
              "HTTP invalid kill mode preserves caller token");
        QTcpSocket malformed;
        QEventLoop loop;
        QObject::connect(&malformed, &QTcpSocket::connected, &loop, [&] {
            malformed.write("POST /account/api/oauth/token HTTP/1.1\r\nHost: 127.0.0.1:" +
                            QByteArray::number(server.serverPort()) +
                            "\r\nContent-Length: 0\r\nContent-Length: 1\r\n\r\n");
        });
        QObject::connect(&malformed, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        malformed.connectToHost(QHostAddress::LocalHost, server.serverPort());
        loop.exec();
        check(malformed.readAll().startsWith("HTTP/1.1 400"), "duplicate framing rejected");
        const auto diagnostics = QJsonDocument(logs).toJson();
        check(!diagnostics.contains(password) && !diagnostics.contains(token) &&
                  !diagnostics.contains(otherToken) && !diagnostics.contains("player@localhost"),
              "logs exclude credentials and token");
        QTextStream(stdout) << checks << " backend HTTP checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << "HTTP check " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
