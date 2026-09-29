#include "fol/backend_http.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <memory>

namespace fol::backend {
HttpServer::HttpServer(Identity identity, std::function<void(const QJsonObject &)> logger,
                       QObject *parent)
    : QTcpServer(parent), service(std::move(identity)), log(std::move(logger)) {
    setMaxPendingConnections(32);
    connect(this, &QTcpServer::newConnection, this, [this] { acceptConnections(); });
}
bool HttpServer::start(quint16 port) { return listen(QHostAddress::LocalHost, port); }
void HttpServer::acceptConnections() {
    while (auto *socket = nextPendingConnection()) {
        if (active >= 32 || !socket->peerAddress().isLoopback()) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        ++active;
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            --active;
            socket->deleteLater();
        });
        struct State {
            QByteArray buffer;
            bool done = false;
        };
        const auto state = std::make_shared<State>();
        const auto send = [this, socket, state](const Response &response) {
            if (state->done)
                return;
            state->done = true;
            const auto body = response.status == 204 ? QByteArray{}
                              : response.boolean ? QByteArray(*response.boolean ? "true" : "false")
                                                 : (response.array ? QJsonDocument(*response.array)
                                                                   : QJsonDocument(response.json))
                                                       .toJson(QJsonDocument::Compact);
            const auto wire =
                "HTTP/1.1 " + QByteArray::number(response.status) + " Response\r\nContent-Type: " +
                (response.boolean ? QByteArray("text/plain") : QByteArray("application/json")) +
                "\r\nConnection: close\r\n"
                "Cache-Control: no-store\r\nDate: " +
                QLocale::c()
                    .toString(QDateTime::currentDateTimeUtc(), "ddd, dd MMM yyyy HH:mm:ss 'GMT'")
                    .toLatin1() +
                "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
            // Never log raw URLs, headers, bodies or tokens. The one optional
            // protocol field is a validated short platform tag from the service.
            if (log) {
                QJsonObject entry{
                    {"component", "backend"},
                    {"event", response.event},
                    {"status", response.status},
                    {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
                if (!response.platformDiagnostic.isEmpty())
                    entry["platform"] = response.platformDiagnostic;
                log(entry);
            }
            if (socket->write(wire) != wire.size())
                socket->abort();
            else
                socket->disconnectFromHost();
            state->buffer.fill(0);
            state->buffer.clear();
        };
        const auto reject = [send](int status, const QString &reason) {
            send({status, {{"errorCode", "errors.fortniteonlinux." + reason}}, reason});
        };
        QTimer::singleShot(10000, socket, [state, reject] {
            if (!state->done)
                reject(408, "request_timeout");
        });
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket, state, send, reject] {
            if (state->done) {
                socket->readAll();
                return;
            }
            state->buffer += socket->read(32769 - state->buffer.size());
            if (state->buffer.size() > 32768) {
                reject(413, "request_too_large");
                return;
            }
            const auto end = state->buffer.indexOf("\r\n\r\n");
            if (end < 0) {
                if (state->buffer.size() > 16384)
                    reject(431, "headers_too_large");
                return;
            }
            if (end > 16384) {
                reject(431, "headers_too_large");
                return;
            }
            const auto lines = state->buffer.left(end).split('\n');
            const auto start = lines.first().trimmed().split(' ');
            if (start.size() != 3 || (start[2] != "HTTP/1.1" && start[2] != "HTTP/1.0") ||
                (start[0] != "GET" && start[0] != "POST" && start[0] != "DELETE") ||
                !start[1].startsWith('/') || start[1].startsWith("//") || start[1].contains('#')) {
                reject(400, "invalid_request_line");
                return;
            }
            QMap<QByteArray, QByteArray> headers;
            for (qsizetype i = 1; i < lines.size(); ++i) {
                const auto line = lines[i].trimmed();
                const auto colon = line.indexOf(':');
                if (colon < 1) {
                    reject(400, "invalid_header");
                    return;
                }
                const auto key = line.left(colon).toLower();
                for (char c : key)
                    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
                        reject(400, "invalid_header_name");
                        return;
                    }
                if (headers.contains(key)) {
                    reject(400, "duplicate_header");
                    return;
                }
                headers.insert(key, line.mid(colon + 1).trimmed());
            }
            const auto host = headers.value("host");
            const auto port = ':' + QByteArray::number(serverPort());
            if (host != "127.0.0.1" + port && host != "localhost" + port) {
                reject(403, "loopback_host_required");
                return;
            }
            if (headers.contains("transfer-encoding")) {
                reject(501, "chunked_not_supported");
                return;
            }
            qsizetype size = 0;
            if (headers.contains("content-length")) {
                const auto value = headers.value("content-length");
                if (value.isEmpty() || value.size() > 5) {
                    reject(400, "invalid_content_length");
                    return;
                }
                for (char c : value) {
                    if (c < '0' || c > '9') {
                        reject(400, "invalid_content_length");
                        return;
                    }
                    size = size * 10 + c - '0';
                }
            }
            if (size > 16384) {
                reject(413, "body_too_large");
                return;
            }
            if (state->buffer.size() < end + 4 + size)
                return;
            if (state->buffer.size() != end + 4 + size) {
                reject(400, "pipelining_not_supported");
                return;
            }
            const auto url = QUrl::fromEncoded(start[1], QUrl::StrictMode);
            if (!url.isValid()) {
                reject(400, "invalid_target");
                return;
            }
            send(service.handle(start[0], url.path(), headers, state->buffer.mid(end + 4, size),
                                QDateTime::currentSecsSinceEpoch(), QUrlQuery(url)));
        });
    }
}
} // namespace fol::backend
