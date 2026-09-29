#include "fol/core.h"
#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QUdpSocket>
#include <iostream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QUdpSocket port;
        const bool bound = port.bind(QHostAddress::LocalHost, 0, QUdpSocket::DontShareAddress);
        if (!bound)
            throw fol::Error("UDP test fixture could not bind: " + port.errorString() + "; " +
                             QString::fromUtf8(QJsonDocument(fol::udpSocketCreationReport(false))
                                                   .toJson(QJsonDocument::Compact)));
        auto occupied = fol::portReport("127.0.0.1", port.localPort());
        fol::require(!occupied["bind_available"].toBool(), "Occupied UDP port not detected");
        fol::require(occupied["upnp"] == "off" && occupied["firewall"] == "not tested",
                     "Bind probe must not imply router/firewall configuration");
        auto available = fol::portReport("127.0.0.1", 0);
        fol::require(available["bind_available"].toBool(), "Ephemeral UDP bind failed");
        std::cout << "4 network checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
