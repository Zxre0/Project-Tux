#include "fol/core.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <iostream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto report = fol::localIpcReport();
    std::cout << QJsonDocument(report).toJson(QJsonDocument::Compact).constData() << '\n';
    return report["functional"].toBool() && !report.contains("cleanup_error") &&
                   !report.contains("close_error")
               ? 0
               : 1;
}
