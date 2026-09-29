#include "fol/core.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
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
void rejects(const std::function<void()> &operation, const QString &expected) {
    try {
        operation();
    } catch (const fol::Error &error) {
        check(QString::fromUtf8(error.what()).contains(expected),
              "Unexpected rejection: " + QString::fromUtf8(error.what()));
        return;
    }
    throw fol::Error("Expected rejection: " + expected);
}
QByteArray integer(quint32 value) {
    QByteArray result(4, '\0');
    qToLittleEndian(value, result.data());
    return result;
}
QByteArray string(const QString &value, bool wide = false) {
    if (wide) {
        QByteArray result = integer(quint32(-qint32(value.size() + 1)));
        for (auto unit : value) {
            char encoded[2];
            qToLittleEndian(unit.unicode(), encoded);
            result.append(encoded, 2);
        }
        return result + QByteArray(2, '\0');
    }
    auto encoded = value.toUtf8();
    return integer(encoded.size() + 1) + encoded + '\0';
}
QByteArray block(const QByteArray &bytes) { return integer(bytes.size() + 4) + bytes; }
struct Entry {
    QString path;
    QByteArray contents;
    QString symlink;
    QStringList tags;
};
QByteArray payload(const QList<Entry> &entries, quint32 feature = 13, bool wide = false) {
    QByteArray metadata(1, '\0');
    metadata += integer(feature) + QByteArray(1, '\0') + integer(0);
    metadata += string("Fortnite") + string("++Fortnite+Release-12.50-CL-13137020", wide) +
                string("FortniteGame/Binaries/Win64/FortniteLauncher.exe");
    metadata += string("") + integer(0) + string("") + string("") + string("");
    QByteArray files(1, '\0');
    files += integer(entries.size());
    for (const auto &entry : entries)
        files += string(entry.path, wide);
    for (const auto &entry : entries)
        files += string(entry.symlink);
    for (const auto &entry : entries)
        files += QCryptographicHash::hash(entry.contents, QCryptographicHash::Sha1);
    files += QByteArray(entries.size(), '\0');
    for (const auto &entry : entries) {
        files += integer(entry.tags.size());
        for (const auto &tag : entry.tags)
            files += string(tag);
    }
    for (const auto &entry : entries) {
        files += integer(1) + integer(28) + QByteArray(20, '\0') + integer(entry.contents.size());
    }
    return block(metadata) + block(QByteArray(1, '\0') + integer(0)) + block(files) +
           block(QByteArray(1, '\0') + integer(0));
}
QByteArray envelope(const QByteArray &body, bool compressed = false) {
    auto stored = compressed ? qCompress(body).mid(4) : body;
    return integer(0x44bec00c) + integer(41) + integer(body.size()) + integer(stored.size()) +
           QCryptographicHash::hash(body, QCryptographicHash::Sha1) +
           QByteArray(1, compressed ? '\1' : '\0') + integer(18) + stored;
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir temporary;
        check(temporary.isValid(), "temporary fixture directory");
        auto root = temporary.path() + "/game";
        check(QDir().mkpath(root), "fixture root");
        auto manifest = temporary.path() + "/fixture.manifest";
        QList<Entry> entries{{"data.pak", "synthetic-content", {}, {"base"}},
                             {"empty.dat", {}, {}, {"optional"}}};
        for (const auto &entry : entries)
            fol::writeFile(root + "/" + entry.path, entry.contents);
        auto inspect = [&](const QByteArray &data, bool hash = false) {
            fol::writeFile(manifest, data);
            return fol::inspectEpicManifest(manifest, root, hash);
        };
        auto body = payload(entries);
        auto plain = envelope(body);
        auto result = inspect(plain);
        check(result["all_listed_files_match"].toBool() && !result["file_hashes_checked"].toBool(),
              "size-only verification is distinct from hashes");
        check(result["format_version"] == 18 && result["metadata_feature_level"] == 13,
              "container and feature versions may differ");
        check(result["build_version"] == "++Fortnite+Release-12.50-CL-13137020",
              "metadata build detection");
        check(result["files"].toArray()[1].toObject()["install_tags"].toArray()[0] == "optional",
              "optional install tags preserved");
        result = inspect(envelope(body, true), true);
        check(result["matched"] == 2 && result["hashed_bytes"].toInteger() == 17,
              "compressed manifest with empty and nonempty file hashes");
        check(inspect(envelope(payload(entries, 18, true)), true)["matched"] == 2,
              "UTF-16 strings supported");
        auto absent = entries;
        absent.append({"optional-missing.pak", "x", {}, {"optional"}});
        result = inspect(envelope(payload(absent)), true);
        check(result["missing"] == 1 && !result["all_listed_files_match"].toBool(),
              "missing optional file reported, not silently ignored");
        fol::writeFile(root + "/data.pak", "different-content");
        check(inspect(plain, true)["mismatched"] == 1, "same-size corruption detected");
        fol::writeFile(root + "/data.pak", "short");
        check(inspect(plain)["mismatched"] == 1, "size mismatch detected");
        for (const auto &entry : entries)
            fol::writeFile(root + "/" + entry.path, entry.contents);
        rejects([&] { inspect(plain.left(40)); }, "Truncated");
        rejects([&] { inspect(plain.left(plain.size() - 1)); }, "sizes");
        auto modified = plain;
        modified[16] ^= 1;
        rejects([&] { inspect(modified); }, "SHA-1 mismatch");
        modified = plain;
        modified[36] = 2;
        rejects([&] { inspect(modified); }, "Encrypted");
        modified = plain;
        modified.replace(37, 4, integer(19));
        rejects([&] { inspect(modified); }, "Unsupported manifest version");
        modified = plain;
        modified.replace(8, 4, integer(65 * 1024 * 1024));
        rejects([&] { inspect(modified); }, "sizes");
        modified = envelope(body, true);
        modified.replace(8, 4, integer(body.size() - 1));
        rejects([&] { inspect(modified); }, "decompression");
        modified = envelope(body, true);
        modified.append('x');
        modified.replace(12, 4, integer(modified.size() - 41));
        rejects([&] { inspect(modified); }, "decompression");
        rejects([&] { inspect(envelope(payload(entries, 19))); }, "feature level");
        for (const QString &path :
             {QString("../outside"), QString("/absolute"), QString("C:\\Windows\\x"),
              QString("a/./b"), QString("a//b"), QString("a\\..\\x")}) {
            rejects([&] { inspect(envelope(payload({{path, "x", {}, {}}}))); }, "Unsafe");
        }
        auto duplicate = entries;
        duplicate.append({"DATA.PAK", "x", {}, {}});
        rejects([&] { inspect(envelope(payload(duplicate))); }, "Duplicate");
        auto linked = entries;
        linked[0].symlink = "elsewhere";
        rejects([&] { inspect(envelope(payload(linked))); }, "symlink entries");
        auto outside = temporary.path() + "/outside";
        fol::writeFile(outside, "outside");
        check(QFile::link(outside, root + "/escape"), "symlink fixture");
        rejects([&] { inspect(envelope(payload({{"escape", "outside", {}, {}}})), true); },
                "escapes root");
        fol::writeFile(manifest, plain);
        rejects([&] { fol::inspectEpicManifest(manifest, {}, true); }, "requires an installation");
        check(!fol::inspectEpicManifest(manifest).contains("all_listed_files_match"),
              "metadata inspection never asserts file verification");
        std::cout << checks << " manifest checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
