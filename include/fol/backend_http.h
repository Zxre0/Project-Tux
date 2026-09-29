#pragma once
#include "fol/backend.h"
#include <QTcpServer>
#include <functional>

namespace fol::backend {
class HttpServer : public QTcpServer {
    Service service;
    std::function<void(const QJsonObject &)> log;
    unsigned active = 0;
    void acceptConnections();

  public:
    HttpServer(Identity identity, std::function<void(const QJsonObject &)> logger,
               QObject *parent = nullptr);
    bool start(quint16 port); // Always IPv4 loopback; zero selects an ephemeral test port.
};
} // namespace fol::backend
