// SPDX-License-Identifier: GPL-3.0-only
// Discord Rich Presence over the desktop client's local IPC socket. Discord shows
// "Playing <application name>"; the name is set on the Discord application whose
// ID is used here. Nothing is sent when Discord isn't running.
#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QtEndian>

class DiscordPresence {
  public:
    DiscordPresence() {
        QObject::connect(&socket, &QLocalSocket::connected, [this] {
            send(0, QJsonObject{{"v", 1}, {"client_id", applicationId}});
        });
        QObject::connect(&socket, &QLocalSocket::readyRead, [this] { receive(); });
        QObject::connect(&socket, &QLocalSocket::disconnected, [this] {
            ready = false;
            sent.clear();
        });
    }
    // Show (or update) the activity. An empty ID or a missing Discord does nothing.
    void show(const QString &id, const QString &details, const QString &state, qint64 start) {
        QJsonObject activity{{"details", details}, {"state", state},
                             {"timestamps", QJsonObject{{"start", start}}}};
        wanted = activity;
        if (id.isEmpty())
            return;
        if (id != applicationId) {
            socket.abort();
            applicationId = id;
        }
        if (socket.state() == QLocalSocket::UnconnectedState)
            connectToDiscord();
        else if (ready)
            flush();
    }
    void clear() {
        wanted = {};
        if (ready)
            flush();
    }

  private:
    void connectToDiscord() {
        // Native, Flatpak (Discord, Vesktop) and Snap clients, discord-ipc-0..9.
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR", QDir::tempPath());
        for (const auto &dir : {runtime, runtime + "/app/com.discordapp.Discord",
                                runtime + "/app/dev.vencord.Vesktop", runtime + "/snap.discord",
                                QDir::tempPath()})
            for (int i = 0; i < 10; ++i) {
                const auto path = dir + "/discord-ipc-" + QString::number(i);
                if (QFileInfo(path).exists()) {
                    socket.connectToServer(path);
                    return;
                }
            }
    }
    void flush() {
        const auto payload = QJsonDocument(wanted).toJson(QJsonDocument::Compact);
        if (payload == sent)
            return;
        sent = payload;
        QJsonObject args{{"pid", QCoreApplication::applicationPid()}};
        if (!wanted.isEmpty())
            args["activity"] = wanted;
        send(1, QJsonObject{{"cmd", "SET_ACTIVITY"}, {"args", args},
                            {"nonce", QString::number(++nonce)}});
    }
    // Frame: little-endian opcode and length, then JSON.
    void send(quint32 opcode, const QJsonObject &body) {
        const auto json = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QByteArray frame(8, '\0');
        qToLittleEndian<quint32>(opcode, frame.data());
        qToLittleEndian<quint32>(quint32(json.size()), frame.data() + 4);
        socket.write(frame + json);
    }
    void receive() {
        buffer += socket.readAll();
        while (buffer.size() >= 8) {
            const auto opcode = qFromLittleEndian<quint32>(buffer.constData());
            const auto length = qFromLittleEndian<quint32>(buffer.constData() + 4);
            if (length > 64 * 1024) {
                socket.abort();
                buffer.clear();
                return;
            }
            if (quint32(buffer.size()) < 8 + length)
                return;
            const auto json = buffer.mid(8, length);
            buffer.remove(0, 8 + length);
            if (opcode == 3) { // ping
                socket.write(pong(json));
            } else if (opcode == 2) { // close: wrong ID or client refused
                socket.disconnectFromServer();
            } else if (opcode == 1 && QJsonDocument::fromJson(json)["evt"] == "READY") {
                ready = true;
                flush();
            }
        }
    }
    static QByteArray pong(const QByteArray &json) {
        QByteArray frame(8, '\0');
        qToLittleEndian<quint32>(4, frame.data());
        qToLittleEndian<quint32>(quint32(json.size()), frame.data() + 4);
        return frame + json;
    }

    QLocalSocket socket;
    QString applicationId;
    QJsonObject wanted;
    QByteArray buffer, sent;
    bool ready = false;
    int nonce = 0;
};
