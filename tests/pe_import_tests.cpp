#include "fol/core.h"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QtEndian>
#include <functional>
#include <iostream>

namespace {
int checks = 0;
void check(bool condition, const QString &message) {
    fol::require(condition, message);
    ++checks;
}
void rejects(const std::function<void()> &operation, const QString &message) {
    try {
        operation();
    } catch (const fol::Error &error) {
        check(QString::fromUtf8(error.what()).contains(message),
              "Unexpected error: " + QString::fromUtf8(error.what()));
        return;
    }
    throw fol::Error("Expected rejection: " + message);
}
void u32(QByteArray &data, int offset, quint32 value) {
    qToLittleEndian(value, data.data() + offset);
}
QByteArray fixture() {
    QByteArray data(1536, '\0');
    data.replace(0, 2, "MZ");
    u32(data, 60, 64);
    data.replace(64, 4, QByteArray("PE\0\0", 4));
    qToLittleEndian<quint16>(0x8664, data.data() + 68);
    qToLittleEndian<quint16>(1, data.data() + 70);
    qToLittleEndian<quint16>(240, data.data() + 84);
    qToLittleEndian<quint16>(2, data.data() + 86);
    qToLittleEndian<quint16>(0x20b, data.data() + 88);
    u32(data, 88 + 60, 512);
    u32(data, 88 + 108, 16);
    u32(data, 88 + 112 + 8, 0x1000);
    u32(data, 88 + 116 + 8, 40);
    u32(data, 88 + 112 + 13 * 8, 0x1080);
    u32(data, 88 + 116 + 13 * 8, 64);
    u32(data, 328 + 8, 1024);
    u32(data, 328 + 12, 0x1000);
    u32(data, 328 + 16, 1024);
    u32(data, 328 + 20, 512);
    u32(data, 512, 0x1280);
    u32(data, 512 + 12, 0x1200);
    u32(data, 512 + 16, 0x1280);
    u32(data, 640, 1);
    u32(data, 644, 0x1240);
    data.replace(1024, 12, "KERNEL32.dll");
    data.replace(1088, 12, "optional.dll");
    return data;
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir temporary;
        check(temporary.isValid(), "fixture directory");
        auto path = temporary.path() + "/image.exe";
        const auto base = fixture();
        auto inspect = [&](const QByteArray &bytes) {
            fol::writeFile(path, bytes);
            return fol::peImportReport(path);
        };
        auto report = inspect(base);
        check(report["immediate_count"] == 1 && report["delayed_count"] == 1,
              "both import kinds parsed");
        check(report["imports_without_candidate"] == 2, "missing candidates reported");
        check(report["imports"].toArray()[0].toObject()["name"] == "KERNEL32.dll",
              "RVA translated into raw file data");
        auto library = base;
        qToLittleEndian<quint16>(0x2002, library.data() + 86);
        fol::writeFile(temporary.path() + "/kernel32.DLL", library);
        report = inspect(base);
        auto candidate =
            report["imports"].toArray()[0].toObject()["candidates"].toArray()[0].toObject();
        check(candidate["valid_x64_dll"].toBool(), "case-insensitive DLL inventory");
        fol::writeFile(temporary.path() + "/optional.dll", "corrupted");
        report = inspect(base);
        check(!report["imports"]
                   .toArray()[1]
                   .toObject()["candidates"]
                   .toArray()[0]
                   .toObject()["valid_x64_dll"]
                   .toBool(),
              "file presence does not imply valid DLL");
        check(report["imports_without_candidate"] == 0 &&
                  report["imports_without_valid_x64_candidate"] == 1,
              "invalid candidates distinguished from absent files");
        auto wrongMachine = library;
        qToLittleEndian<quint16>(0x14c, wrongMachine.data() + 68);
        fol::writeFile(temporary.path() + "/optional.dll", wrongMachine);
        report = inspect(base);
        check(report["imports"]
                      .toArray()[1]
                      .toObject()["candidates"]
                      .toArray()[0]
                      .toObject()["error"] == "PE architecture is not x64",
              "architecture mismatch has actionable reason");
        auto data = base;
        data.replace(1024, 32, QByteArray(32, '\0'));
        data.replace(1024, 15, "api-ms-test.dll");
        report = inspect(data);
        check(report["imports"].toArray()[0].toObject()["api_set_contract"].toBool() &&
                  report["imports_without_candidate"] == 0,
              "absent API-set file not labelled missing concrete DLL");
        data = base;
        u32(data, 88 + 108, 1);
        check(inspect(data)["imports"].toArray().isEmpty(), "undeclared directories not read");
        data = base;
        u32(data, 88 + 108, 17);
        rejects([&] { inspect(data); }, "directory count");
        data = base;
        u32(data, 88 + 60, 100);
        rejects([&] { inspect(data); }, "header extent");
        data = base;
        u32(data, 512 + 12, 0);
        rejects([&] { inspect(data); }, "null RVA");
        data = base;
        u32(data, 512 + 12, 0x1400);
        rejects([&] { inspect(data); }, "not backed");
        data = base;
        u32(data, 88 + 116 + 8, 20);
        rejects([&] { inspect(data); }, "null terminator");
        data = base;
        u32(data, 88 + 116 + 8, 0xffffffff);
        rejects([&] { inspect(data); }, "directory size");
        data = base;
        u32(data, 640, 0);
        rejects([&] { inspect(data); }, "delayed-import flags");
        data = base;
        data.replace(1024, 13, "../unsafe.dll");
        rejects([&] { inspect(data); }, "basename");
        data = base;
        data.replace(1024, 256, QByteArray(256, 'a'));
        rejects([&] { inspect(data); }, "255 bytes");
        data = base;
        u32(data, 88 + 116 + 8, 60);
        data.replace(532, 20, data.mid(512, 20));
        rejects([&] { inspect(data); }, "Duplicate");
        data = base;
        qToLittleEndian<quint16>(2, data.data() + 70);
        data.replace(368, 40, data.mid(328, 40));
        rejects([&] { inspect(data); }, "Overlapping");
        rejects([&] { inspect(base.left(1000)); }, "valid x64 PE");
        data = base;
        qToLittleEndian<quint16>(0x14c, data.data() + 68);
        rejects([&] { inspect(data); }, "valid x64 PE");
        fol::writeFile(path, base);
        rejects([&] { fol::peImportReport(path, {temporary.path() + "/missing"}); }, "Invalid DLL");
        std::cout << checks << " PE import checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
