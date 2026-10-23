// SPDX-License-Identifier: GPL-3.0-only
// Play page news: the joined server's announcements and player count (GET
// /fol/launcher on its backend, see docs/LAUNCHER_FEED.md) and Project Tux's
// own merged changes from GitHub. Server text is only ever painted as plain
// text, and only http(s) links are opened.
#pragma once
#include <QButtonGroup>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

#include "theme.h"

struct Post {
    QString title;
    QDate date;
    QUrl link, image;
};

// Only web links leave the launcher.
inline QUrl webUrl(const QJsonValue &value) {
    const QUrl url(value.toString());
    return url.isValid() && (url.scheme() == "https" || url.scheme() == "http") ? url : QUrl();
}

// Polls the joined server's backend for its player count and announcements.
class ServerFeed {
  public:
    enum class Reach { None, Checking, Offline, Online };
    struct Snapshot {
        Reach reach = Reach::None;
        bool hasFeed = false;
        int players = -1, capacity = -1;
        QString name;
        QList<Post> posts;
    };
    std::function<void(const Snapshot &)> changed;

    ServerFeed() {
        timer.setInterval(30000);
        QObject::connect(&timer, &QTimer::timeout, [this] { poll(); });
    }
    // An empty host stops polling.
    void setHost(const QString &value) {
        if (value == host)
            return;
        host = value;
        snapshot = {};
        snapshot.reach = host.isEmpty() ? Reach::None : Reach::Checking;
        notify();
        if (host.isEmpty()) {
            timer.stop();
            return;
        }
        timer.start();
        poll();
    }

  private:
    void poll() {
        // The backend answers only for the loopback Host the game uses, like the
        // relay's /fol/health check.
        QNetworkRequest request(QUrl(QString("http://%1:3551/fol/launcher").arg(host)));
        request.setRawHeader("Host", "127.0.0.1:3551");
        request.setTransferTimeout(4000);
        auto *reply = net.get(request);
        QObject::connect(reply, &QNetworkReply::finished, [this, reply, asked = host] {
            reply->deleteLater();
            if (asked != host)
                return;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            Snapshot next;
            if (status == 200 && reply->error() == QNetworkReply::NoError)
                next = parse(reply->read(256 * 1024));
            else
                // Any HTTP answer means the backend is up; it just has no feed yet.
                next.reach = status > 0 ? Reach::Online : Reach::Offline;
            snapshot = next;
            notify();
        });
    }
    static Snapshot parse(const QByteArray &data) {
        const auto doc = QJsonDocument::fromJson(data).object();
        Snapshot next;
        next.reach = Reach::Online;
        next.hasFeed = true;
        next.name = doc["name"].toString().simplified().left(40);
        const auto players = doc["players"].toObject();
        next.players = players["online"].toInt(-1);
        next.capacity = players["max"].toInt(-1);
        for (const auto &value : doc["announcements"].toArray()) {
            const auto post = value.toObject();
            const auto title = post["title"].toString().simplified().left(120);
            if (title.isEmpty())
                continue;
            next.posts.append({title, QDate::fromString(post["date"].toString().left(10), Qt::ISODate),
                               webUrl(post["url"]), webUrl(post["image"])});
            if (next.posts.size() == 20)
                break;
        }
        return next;
    }
    void notify() {
        if (changed)
            changed(snapshot);
    }
    QNetworkAccessManager net;
    QTimer timer;
    QString host;
    Snapshot snapshot;
};

// Project Tux's merged pull requests, newest first, as update notes.
class UpdatesFeed {
  public:
    std::function<void(bool ok, const QList<Post> &)> done;
    void fetch() {
        QNetworkRequest request(QUrl("https://api.github.com/repos/Zxre0/Project-Tux/pulls"
                                     "?state=closed&sort=updated&direction=desc&per_page=30"));
        request.setRawHeader("User-Agent", "Project-Tux-launcher");
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setTransferTimeout(8000);
        auto *reply = net.get(request);
        QObject::connect(reply, &QNetworkReply::finished, [this, reply] {
            reply->deleteLater();
            QList<Post> posts;
            const bool ok = reply->error() == QNetworkReply::NoError;
            for (const auto &value : QJsonDocument::fromJson(reply->read(1024 * 1024)).array()) {
                const auto pull = value.toObject();
                if (pull["merged_at"].isNull())
                    continue;
                posts.append({pull["title"].toString().simplified().left(120),
                              QDateTime::fromString(pull["merged_at"].toString(), Qt::ISODate).date(),
                              webUrl(pull["html_url"]), {}});
            }
            std::sort(posts.begin(), posts.end(),
                      [](const Post &a, const Post &b) { return a.date > b.date; });
            if (done)
                done(ok, posts.mid(0, 10));
        });
    }

  private:
    QNetworkAccessManager net;
};

inline QString shortDate(const QDate &date) {
    return date.isValid() ? QLocale().toString(date, "d MMM") : QString();
}

// Top post: its image, or a navy title card when it has none.
class Banner : public theme::Interactive<QAbstractButton> {
  public:
    Banner() { setFixedHeight(132); }
    void present(const Post &value, const QImage &picture) {
        post = value;
        image = picture;
        setText(post.title);
        setAccessibleName(post.title);
        setCursor(post.link.isValid() ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    const Post &current() const { return post; }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r = rect();
        QPainterPath clip;
        clip.addRoundedRect(r, 4, 4);
        p.setClipPath(clip);
        if (!image.isNull()) {
            const auto size = QSizeF(image.size()).scaled(r.size(), Qt::KeepAspectRatioByExpanding);
            p.drawImage(QRectF(r.center() - QPointF(size.width() / 2, size.height() / 2), size),
                        image);
        } else {
            // No picture: the launcher's own lobby art, like the window behind it.
            if (art.size() != size() * devicePixelRatioF())
                art = theme::Backdrop::scene(size() * devicePixelRatioF());
            p.drawImage(r, art);
        }
        QLinearGradient scrim(0, r.height() * 0.25, 0, r.height());
        scrim.setColorAt(0, QColor(15, 31, 74, 0));
        scrim.setColorAt(1, QColor(15, 31, 74, 225));
        p.fillRect(r, scrim);
        p.setFont(theme::display(23));
        p.setPen(theme::text);
        const QRectF words(18, 12, r.width() - 36, r.height() - 44);
        p.drawText(words, Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, post.title);
        p.setFont(theme::font(theme::Body));
        p.setPen(theme::muted);
        p.drawText(QRectF(18, r.height() - 30, r.width() - 36, 22), Qt::AlignLeft | Qt::AlignVCenter,
                   shortDate(post.date));
        if (keyboardFocus() || (underMouse() && post.link.isValid())) {
            p.setClipping(false);
            p.setPen(QPen(theme::accentSoft, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
        }
    }

  private:
    Post post;
    QImage image, art;
};

// One announcement: title on the left, date on the right.
class NewsRow : public theme::Interactive<QAbstractButton> {
  public:
    NewsRow() { setFixedHeight(32); }
    void present(const Post &value) {
        post = value;
        setText(post.title);
        setAccessibleName(post.title + (post.date.isValid() ? ", " + shortDate(post.date) : ""));
        setCursor(post.link.isValid() ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    const Post &current() const { return post; }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = rect();
        if (underMouse() && post.link.isValid()) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 20));
            p.drawRoundedRect(r, 4, 4);
        }
        const auto date = shortDate(post.date);
        p.setFont(theme::font(theme::Body));
        const qreal dateWidth = QFontMetrics(p.font()).horizontalAdvance(date) + 10;
        p.setPen(theme::faint);
        p.drawText(r.adjusted(8, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter, date);
        p.setFont(theme::font(theme::Body, QFont::DemiBold));
        p.setPen(theme::text);
        const QRectF words = r.adjusted(8, 0, -8 - dateWidth, 0);
        p.drawText(words, Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(post.title, Qt::ElideRight, int(words.width())));
        if (keyboardFocus()) {
            p.setPen(QPen(theme::accentSoft, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 4, 4);
        }
    }

  private:
    Post post;
};

// Bottom-left card on the Play page.
class AnnouncementsCard : public QFrame {
  public:
    AnnouncementsCard() {
        setObjectName("panel");
        setFixedWidth(440);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 12);
        layout->setSpacing(10);
        auto *bar = new QHBoxLayout;
        bar->setSpacing(6);
        auto *group = new QButtonGroup(this);
        for (const char *label : {"SERVER", "UPDATES"}) {
            auto *tab = new theme::NavButton(label, true);
            group->addButton(tab, group->buttons().size());
            bar->addWidget(tab);
        }
        group->button(0)->setChecked(true);
        bar->addStretch();
        layout->addLayout(bar);
        QObject::connect(group, &QButtonGroup::idClicked, this, [this](int id) {
            showingUpdates = id == 1;
            render();
        });
        banner = new Banner;
        layout->addWidget(banner);
        QObject::connect(banner, &QAbstractButton::clicked, this, [this] { open(banner->current()); });
        for (int i = 0; i < 4; ++i) {
            auto *row = new NewsRow;
            layout->addWidget(row);
            rows.append(row);
            QObject::connect(row, &QAbstractButton::clicked, this, [this, row] { open(row->current()); });
        }
        empty = new QLabel;
        empty->setTextFormat(Qt::PlainText);
        empty->setWordWrap(true);
        empty->setMinimumHeight(60);
        empty->setFont(theme::font(theme::Label));
        empty->setStyleSheet(QString("color: %1").arg(theme::muted.name()));
        layout->addWidget(empty);
        render();
    }
    void setServer(const ServerFeed::Snapshot &snapshot) {
        server = snapshot;
        render();
    }
    void setUpdates(bool ok, const QList<Post> &posts) {
        updatesLoaded = true;
        updatesOk = ok;
        updates = posts;
        render();
    }

  private:
    void render() {
        QList<Post> posts;
        QString note;
        if (showingUpdates) {
            posts = updates;
            note = !updatesLoaded ? "Loading Project Tux updates…"
                   : !updatesOk   ? "Couldn't load Project Tux updates. Check your connection."
                                  : "No updates yet.";
        } else {
            using Reach = ServerFeed::Reach;
            posts = server.posts;
            note = server.reach == Reach::None       ? "Add a server to see its announcements."
                   : server.reach == Reach::Checking ? "Checking the server…"
                   : server.reach == Reach::Offline  ? "Can't reach the server right now."
                   : !server.hasFeed ? "This server doesn't post announcements yet."
                                     : "No announcements right now.";
        }
        banner->setVisible(!posts.isEmpty());
        if (!posts.isEmpty())
            banner->present(posts.first(), images.value(posts.first().image));
        for (int i = 0; i < rows.size(); ++i) {
            const bool shown = i + 1 < posts.size();
            rows[i]->setVisible(shown);
            if (shown)
                rows[i]->present(posts[i + 1]);
        }
        empty->setText(note);
        empty->setVisible(posts.isEmpty());
        if (!posts.isEmpty())
            fetchImage(posts.first().image);
    }
    void fetchImage(const QUrl &url) {
        if (!url.isValid() || images.contains(url) || pending.contains(url))
            return;
        pending.append(url);
        QNetworkRequest request(url);
        request.setTransferTimeout(8000);
        auto *reply = net.get(request);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
            reply->deleteLater();
            pending.removeAll(url);
            QImage image;
            if (reply->error() == QNetworkReply::NoError)
                image.loadFromData(reply->read(4 * 1024 * 1024));
            images.insert(url, image.isNull() ? QImage() : image.scaledToWidth(
                                                               816, Qt::SmoothTransformation));
            render();
        });
    }
    static void open(const Post &post) {
        if (post.link.isValid())
            QDesktopServices::openUrl(post.link);
    }

    Banner *banner;
    QList<NewsRow *> rows;
    QLabel *empty;
    bool showingUpdates = false, updatesLoaded = false, updatesOk = false;
    ServerFeed::Snapshot server;
    QList<Post> updates;
    QHash<QUrl, QImage> images;
    QList<QUrl> pending;
    QNetworkAccessManager net;
};
