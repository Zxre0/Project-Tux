// SPDX-License-Identifier: GPL-3.0-only
// Project Tux launcher: joins a multiplayer server with the server's accounts
// through scripts/fortnite-match.sh. Client only: the game server, its backend
// and the accounts all live on the server.
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "announcements.h"
#include "discord_presence.h"
#include "theme.h"

#include <algorithm>
#include <csignal>
#include <unistd.h>
#include <utility>

namespace {
QString home(const QString &path) { return QDir::homePath() + "/" + path; }
QString xdg(const char *variable, const QString &fallback) {
    const auto value = qEnvironmentVariable(variable);
    return (value.isEmpty() ? home(fallback) : value) + "/fortnite-on-linux";
}
QString repoRoot() {
    const auto env = qEnvironmentVariable("PROJECT_TUX_ROOT");
    if (!env.isEmpty())
        return env;
    // Installed next to the build tree (build/project-tux) or run from it.
    const QDir app(QCoreApplication::applicationDirPath());
    if (QFileInfo::exists(app.filePath("../scripts/fortnite-match.sh")))
        return QDir::cleanPath(app.filePath(".."));
    return QStringLiteral(PROJECT_TUX_SOURCE_DIR);
}
// Install command for a distribution package, for the setup checklist.
QString installHint(const QString &package) {
    if (!QStandardPaths::findExecutable("dnf").isEmpty())
        return "sudo dnf install " + package;
    if (!QStandardPaths::findExecutable("pacman").isEmpty())
        return "sudo pacman -S " + package;
    if (!QStandardPaths::findExecutable("apt").isEmpty())
        return "sudo apt install " + package;
    return "install the " + package + " package";
}
// Release version embedded in the game executable ("12.50", "3.1"), or empty,
// and its changelist ("3917250"). The ASCII build strings are source paths
// without the changelist; the full "++Fortnite+Release-3.1-CL-3917250" is a
// UTF-16 string further in. Cached per file so the checklist can ask on every
// refresh.
QString gameVersion(const QString &gameDir, QString *changelist = nullptr) {
    static QString cachedKey, cachedVersion, cachedChangelist;
    const QFileInfo exe(gameDir + "/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe");
    const auto key = exe.absoluteFilePath() + "|" + QString::number(exe.size()) + "|" +
                     exe.lastModified().toString(Qt::ISODateWithMs);
    if (key != cachedKey) {
        cachedKey = key;
        cachedVersion.clear();
        cachedChangelist.clear();
        QFile file(exe.absoluteFilePath());
        static const QRegularExpression release(
            "\\+\\+Fortnite\\+Release-([0-9]{1,2}\\.[0-9]{1,2})[-+](?:CL-([0-9]+))?");
        static const QString marker = "++Fortnite+Release-";
        static const QByteArray wide(reinterpret_cast<const char *>(marker.utf16()),
                                     marker.size() * 2);
        QByteArray tail;
        const bool opened = file.open(QIODevice::ReadOnly);
        while (opened && !file.atEnd() && (cachedVersion.isEmpty() || cachedChangelist.isEmpty())) {
            const auto data = tail + file.read(1024 * 1024);
            if (const auto at = data.indexOf(marker.toLatin1()); at >= 0 && cachedVersion.isEmpty())
                if (const auto match = release.match(QString::fromLatin1(data.mid(at, 40)));
                    match.hasMatch())
                    cachedVersion = match.captured(1);
            if (const auto at = data.indexOf(wide); at >= 0) {
                const auto text = data.mid(at, 100);
                const auto match = release.match(QString::fromUtf16(
                    reinterpret_cast<const char16_t *>(text.constData()), text.size() / 2));
                if (match.hasMatch() && !match.captured(2).isEmpty()) {
                    cachedVersion = match.captured(1);
                    cachedChangelist = match.captured(2);
                }
            }
            tail = data.right(128);
        }
    }
    if (changelist)
        *changelist = cachedChangelist;
    return cachedVersion;
}
// Server address check: host or IPv4, optional :port. Empty when valid.
QString addressProblem(const QString &address) {
    static const QRegularExpression shape("^([A-Za-z0-9.-]+)(?::([0-9]{1,5}))?$");
    if (address.isEmpty())
        return {};
    const auto match = shape.match(address);
    if (!match.hasMatch())
        return "Use an IP address or hostname, optionally followed by :port.";
    if (!match.captured(2).isEmpty()) {
        const int port = match.captured(2).toInt();
        if (port < 1 || port > 65535)
            return "The port must be between 1 and 65535.";
    }
    return {};
}
QString portOf(const QString &address) {
    return address.contains(':') ? address.section(':', -1) : QString("7777");
}

enum class State { Off, Starting, Ready, Failed };
void setTone(QLabel *label, const char *tone) {
    label->setProperty("tone", tone);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

class Launcher : public QMainWindow {
    enum Tab { PlayTab, ServerTab, SettingsTab, LogsTab };
    struct Problem {
        QString text;
        Tab fix = LogsTab;
    };
    struct Row {
        theme::Dot *dot;
        QLabel *value;
    };

  public:
    Launcher() {
        setWindowTitle("Project Tux");
        setMinimumSize(1100, 700);
        resize(1280, 800);
        root = repoRoot();
        self = QCoreApplication::applicationFilePath(); // before an update replaces it
        data = xdg("XDG_DATA_HOME", ".local/share");
        config = xdg("XDG_CONFIG_HOME", ".config");

        // The tab bar is replaced by the text tabs in the top bar.
        tabs = new QTabWidget;
        tabs->tabBar()->hide();
        tabs->addTab(playTab(), "Play");
        tabs->addTab(page(serverTab(), "Server"), "Server");
        tabs->addTab(page(settingsTab(), "Settings"), "Settings");
        tabs->addTab(logsTab(), "Logs");
        backdrop = new theme::Backdrop;
        auto *column = new QVBoxLayout(backdrop);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);
        column->addWidget(topBar());
        column->addWidget(tabs, 1);
        setCentralWidget(backdrop);
        // State and Launch sit in the bottom corners above every page, so pages run to
        // the bottom edge.
        status = new theme::StatusBlock;
        QObject::connect(status, &QAbstractButton::clicked, this, [this] {
            if (!status->action().isEmpty())
                tabs->setCurrentIndex(recoverTab);
        });
        backdrop->setCorner(status, Qt::BottomLeftCorner);
        backdrop->setCorner(footer(), Qt::BottomRightCorner);
        QObject::connect(tabs, &QTabWidget::currentChanged, this,
                         [this](int index) { backdrop->setDimmed(index != PlayTab); });
        for (int i = 0; i < tabs->count(); ++i)
            QObject::connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key(Qt::Key_1 + i)), this),
                             &QShortcut::activated, this, [this, i] { tabs->setCurrentIndex(i); });
        QObject::connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this),
                         &QShortcut::activated, this, [this] { primaryAction(); });

        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setChildProcessModifier([] { ::setsid(); }); // own process group for Stop
        QObject::connect(&process, &QProcess::readyReadStandardOutput, this, [this] {
            while (process.canReadLine())
                handleLine(QString::fromUtf8(process.readLine()).trimmed());
        });
        QObject::connect(&process, &QProcess::finished, this,
                         [this](int code, QProcess::ExitStatus) {
                             if (killTimer.isActive())
                                 killTimer.stop();
                             log(QString("— session ended (%1) —").arg(code));
                             if (code != 0 && !stopping && problem.text.isEmpty())
                                 fail(QString("The launch stopped unexpectedly (exit code %1). "
                                              "The log has the details.")
                                          .arg(code),
                                      LogsTab);
                             if (relay != State::Failed)
                                 relay = State::Off;
                             client = State::Off;
                             match.clear();
                             running = stopping = false;
                             refresh();
                         });
        killTimer.setSingleShot(true);
        QObject::connect(&killTimer, &QTimer::timeout, this, [this] { signalGroup(SIGKILL); });
        // Server news and player count follow the address once typing pauses.
        feed.changed = [this](const ServerFeed::Snapshot &snapshot) {
            serverName = snapshot.name;
            news->setServer(snapshot);
            showPlayers(snapshot);
        };
        updates.done = [this](bool ok, const QList<Post> &posts) { news->setUpdates(ok, posts); };
        updates.fetch();
        feedDelay.setSingleShot(true);
        feedDelay.setInterval(700);
        QObject::connect(&feedDelay, &QTimer::timeout, this, [this] {
            feed.setHost(addressProblem(address()).isEmpty() ? host() : QString());
        });
        poll.setInterval(2000);
        QObject::connect(&poll, &QTimer::timeout, this, [this] { pollMatch(); });
        updater.setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(&updater, &QProcess::readyReadStandardOutput, this, [this] {
            const auto text = QString::fromUtf8(updater.readAllStandardOutput());
            updateText += text;
            if (!checking) {
                updateLog->moveCursor(QTextCursor::End);
                updateLog->insertPlainText(text);
                updateLog->ensureCursorVisible();
            }
        });
        QObject::connect(&updater, &QProcess::finished, this,
                         [this](int code, QProcess::ExitStatus) { updateFinished(code); });
        // Look for updates shortly after start (a git fetch; nothing is installed)
        // and offer to install them.
        if (QFileInfo::exists(root + "/update.sh"))
            QTimer::singleShot(1500, this, [this] {
                startupCheck = true;
                runUpdate(true);
            });
        protectSettings();
        refresh();
        launchButton->setFocus();
    }

    void launchNow() { launch(); }
    void showTab(const QString &name) {
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabText(i).compare(name, Qt::CaseInsensitive) == 0)
                tabs->setCurrentIndex(i);
    }

  protected:
    void closeEvent(QCloseEvent *event) override {
        if (updating && !checking &&
            QMessageBox::question(this, "Project Tux",
                                  "An update is still running. Quit anyway? Run ./update.sh "
                                  "again later to finish it.") != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        if (running && QMessageBox::question(this, "Project Tux", "Stop the running game?") !=
                           QMessageBox::Yes) {
            event->ignore();
            return;
        }
        if (running) {
            signalGroup(SIGTERM);
            process.waitForFinished(8000);
            signalGroup(SIGKILL);
        }
        event->accept();
    }

  private:
    // ---- frame ----
    QWidget *topBar() {
        auto *bar = new QWidget;
        bar->setFixedHeight(68);
        auto *row = new QHBoxLayout(bar);
        row->setContentsMargins(22, 6, 22, 0);
        row->setSpacing(6);
        brand = new theme::Brand;
        row->addWidget(brand);
        row->addSpacing(14);
        auto *group = new QButtonGroup(bar);
        for (int i = 0; i < tabs->count(); ++i) {
            auto *button = new theme::NavButton(tabs->tabText(i).toUpper());
            button->setToolTip(QString("%1 (Ctrl+%2)").arg(tabs->tabText(i)).arg(i + 1));
            group->addButton(button, i);
            row->addWidget(button);
        }
        group->button(PlayTab)->setChecked(true);
        QObject::connect(group, &QButtonGroup::idClicked, tabs, &QTabWidget::setCurrentIndex);
        // Keep focus on the tab itself so switching pages never lands it on a page's
        // first button.
        QObject::connect(tabs, &QTabWidget::currentChanged, group, [group](int index) {
            group->button(index)->setChecked(true);
            group->button(index)->setFocus(Qt::OtherFocusReason);
        });
        row->addStretch();
        updateChip = new theme::StatusChip;
        QObject::connect(updateChip, &QAbstractButton::clicked, this,
                         [this] { tabs->setCurrentIndex(SettingsTab); });
        updateChip->hide();
        row->addWidget(updateChip);
        row->addSpacing(6);
        players = new theme::StatusChip;
        QObject::connect(players, &QAbstractButton::clicked, this,
                         [this] { tabs->setCurrentIndex(ServerTab); });
        players->hide();
        row->addWidget(players);
        row->addSpacing(6);
        account = new theme::StatusChip;
        QObject::connect(account, &QAbstractButton::clicked, this,
                         [this] { tabs->setCurrentIndex(ServerTab); });
        row->addWidget(account);
        return bar;
    }

    // The launch block and its Options menu, kept in the bottom-right corner.
    QWidget *footer() {
        auto *bar = new QWidget;
        auto *row = new QHBoxLayout(bar);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);
        auto *options = new theme::OptionsButton;
        auto *menu = new QMenu(options);
        menu->addAction("Server", this, [this] { tabs->setCurrentIndex(ServerTab); });
        menu->addAction("Settings", this, [this] { tabs->setCurrentIndex(SettingsTab); });
        menu->addAction("Logs", this, [this] { tabs->setCurrentIndex(LogsTab); });
        menu->addSeparator();
        menu->addAction("Open log folder", this, [this] { openLogFolder(); });
        QObject::connect(options, &QPushButton::clicked, this, [options, menu] {
            menu->popup(options->mapToGlobal(QPoint(0, -menu->sizeHint().height() - 6)));
        });
        launchButton = new theme::PlayButton;
        launchButton->setToolTip("Ctrl+Enter");
        QObject::connect(launchButton, &QPushButton::clicked, this, [this] { primaryAction(); });
        row->addWidget(options);
        row->addWidget(launchButton);
        return bar;
    }
    void openLogFolder() {
        const auto dir = session.isEmpty()
                             ? xdg("XDG_STATE_HOME", ".local/state") + "/logs/sessions"
                             : session;
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    }

    // Form page: a title over a column of a readable width.
    QWidget *page(QWidget *content, const QString &heading) {
        auto *inner = new QWidget;
        auto *layout = new QVBoxLayout(inner);
        layout->setContentsMargins(48, 18, 48, 120);
        auto *title = new QLabel(heading.toUpper());
        title->setFont(theme::display(theme::Page));
        layout->addWidget(title);
        content->setFixedWidth(680);
        layout->addWidget(content, 1, Qt::AlignLeft);
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(inner);
        return scroll;
    }

    // ---- Play ----
    // The art with the announcements pinned under the Project Tux title. State lives
    // in the corner bar, next to Launch.
    QWidget *playTab() {
        auto *page = new QWidget;
        auto *row = new QHBoxLayout(page);
        row->setContentsMargins(22, 8, 48, 0);
        auto *left = new QVBoxLayout;
        news = new AnnouncementsCard;
        left->addWidget(news);
        left->addStretch();
        row->addLayout(left);
        row->addStretch(1);
        return page;
    }

    // ---- Server ----
    QWidget *serverTab() {
        auto *content = new QWidget;
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *intro = new QLabel("Your host gives you the server address and a login for its "
                                 "admin website. Enter both here, then press Play.");
        intro->setWordWrap(true);
        intro->setFont(theme::font(theme::Label));
        setTone(intro, "muted");
        layout->addWidget(intro);

        auto *box = new QGroupBox("ADDRESS");
        auto *form = new QVBoxLayout(box);
        serverAddress = new QLineEdit(settings.value("multiplayer/address").toString());
        serverAddress->setPlaceholderText("100.64.0.12 or tux.example.net");
        serverAddress->setAccessibleName("Server address");
        serverAddress->setMaximumWidth(420);
        form->addWidget(serverAddress);
        addressNote = new QLabel;
        addressNote->setWordWrap(true);
        form->addWidget(addressNote);
        auto *addressHelp = new QLabel(
            "Add a port only if your host moved the game off the usual 7777, for example "
            "100.64.0.12:7778.");
        addressHelp->setWordWrap(true);
        setTone(addressHelp, "faint");
        form->addWidget(addressHelp);
        layout->addWidget(box);

        auto *accountBox = new QGroupBox("ACCOUNT");
        auto *accountForm = new QFormLayout(accountBox);
        accountForm->setHorizontalSpacing(16);
        accountForm->setVerticalSpacing(10);
        serverLogin = new QLineEdit(settings.value("multiplayer/login").toString());
        serverLogin->setPlaceholderText("nocto@tux");
        serverLogin->setMaximumWidth(320);
        serverPassword = new QLineEdit(settings.value("multiplayer/password").toString());
        serverPassword->setEchoMode(QLineEdit::Password);
        serverPassword->setMaximumWidth(320);
        auto *reveal = new QPushButton("Show");
        reveal->setCheckable(true);
        reveal->setAccessibleName("Show password");
        auto *passwordRow = new QHBoxLayout;
        passwordRow->addWidget(serverPassword);
        passwordRow->addWidget(reveal);
        passwordRow->addStretch();
        accountForm->addRow("Login", serverLogin);
        accountForm->addRow("Password", passwordRow);
        auto *accountHelp =
            new QLabel("Your locker and cosmetics live on the server, so they show up in its "
                       "matches.");
        accountHelp->setWordWrap(true);
        setTone(accountHelp, "faint");
        accountForm->addRow(accountHelp);
        layout->addWidget(accountBox);
        serverBoxes = {box, accountBox};

        auto *note = new QLabel("Playing with friends over the internet works best through a "
                                "VPN such as Tailscale or ZeroTier.");
        note->setWordWrap(true);
        setTone(note, "faint");
        layout->addSpacing(14);
        layout->addWidget(note);
        layout->addStretch();

        QObject::connect(reveal, &QPushButton::toggled, this, [this, reveal](bool on) {
            serverPassword->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
            reveal->setText(on ? "Hide" : "Show");
        });
        QObject::connect(serverAddress, &QLineEdit::textChanged, this, [this](const QString &text) {
            settings.setValue("multiplayer/address", text.trimmed());
            refresh();
        });
        QObject::connect(serverLogin, &QLineEdit::textChanged, this, [this](const QString &text) {
            settings.setValue("multiplayer/login", text.trimmed());
            refresh();
        });
        QObject::connect(serverPassword, &QLineEdit::textChanged, this,
                         [this](const QString &text) {
                             settings.setValue("multiplayer/password", text);
                             protectSettings();
                             refresh();
                         });
        return content;
    }
    QString address() const { return serverAddress->text().trimmed(); }
    QString host() const { return address().section(':', 0, 0); }
    // How the UI names the server: its published name, never its address.
    QString serverCalled(bool sentenceStart = false) const {
        return !serverName.isEmpty() ? serverName : sentenceStart ? "The server" : "the server";
    }

    // ---- Settings ----
    QWidget *settingsTab() {
        auto *content = new QWidget;
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *box = new QGroupBox("GAME");
        auto *form = new QFormLayout(box);
        form->setHorizontalSpacing(16);
        gameEdit = new QLineEdit(
            settings.value("paths/game", home("Documents/Fortnite/++Fortnite+Release-12"))
                .toString());
        protonEdit = new QLineEdit(
            settings.value("paths/proton", data + "/runtimes/experimental-20260924/proton")
                .toString());
        form->addRow("Fortnite folder", pathRow(gameEdit, true, "paths/game"));
        form->addRow("Proton", pathRow(protonEdit, false, "paths/proton"));
        layout->addWidget(box);
        settingsBox = box;

        auto *checks = new QGroupBox("SETUP CHECK");
        auto *checkRows = new QVBoxLayout(checks);
        checkRows->setSpacing(8);
        for (qsizetype i = 0; i < this->checks().size(); ++i) {
            auto *line = new QHBoxLayout;
            line->setSpacing(10);
            auto *dot = new theme::Dot;
            auto *label = new QLabel;
            line->addWidget(dot, 0, Qt::AlignVCenter);
            line->addWidget(label, 1);
            checkRows->addLayout(line);
            checkLines.append({dot, label});
        }
        checkRows->addSpacing(6);
        auto *recheck = new QPushButton("Check again");
        recheck->setMinimumHeight(36);
        QObject::connect(recheck, &QPushButton::clicked, this, [this] { refresh(); });
        checkRows->addWidget(recheck, 0, Qt::AlignLeft);
        layout->addWidget(checks);

        auto *look = new QGroupBox("BACKGROUND");
        auto *lookForm = new QVBoxLayout(look);
        backgroundEdit = new QLineEdit(settings.value("appearance/background").toString());
        backgroundEdit->setPlaceholderText("Default Season 3 art");
        lookForm->addWidget(pathRow(backgroundEdit, false, "appearance/background"));
        backgroundNote = new QLabel("Any PNG or JPG. Leave empty for the default art.");
        backgroundNote->setWordWrap(true);
        setTone(backgroundNote, "faint");
        lookForm->addWidget(backgroundNote);
        layout->addWidget(look);

        // Discord shows "Playing <name of the Discord application with this ID>":
        // Project Tux's own application, "Project Tux Fortnite on Linux".
        auto *discordBox = new QGroupBox("DISCORD");
        auto *discordForm = new QFormLayout(discordBox);
        discordForm->setHorizontalSpacing(16);
        discordCheck = new QCheckBox("Show what you're playing in Discord");
        discordCheck->setChecked(settings.value("discord/enabled", true).toBool());
        discordId = new QLineEdit(
            settings.value("discord/application_id", "1562653073317568513").toString());
        discordId->setPlaceholderText("Discord application ID");
        discordId->setMaximumWidth(260);
        discordForm->addRow(discordCheck);
        discordForm->addRow("Application ID", discordId);
        auto *discordHelp = new QLabel(
            "Shows “Playing Project Tux Fortnite on Linux”. Change the ID only to use your "
            "own application from discord.com/developers/applications.");
        discordHelp->setWordWrap(true);
        setTone(discordHelp, "faint");
        discordForm->addRow(discordHelp);
        layout->addWidget(discordBox);
        QObject::connect(discordCheck, &QCheckBox::toggled, this, [this](bool on) {
            settings.setValue("discord/enabled", on);
            refresh();
        });
        QObject::connect(discordId, &QLineEdit::textChanged, this, [this](const QString &text) {
            settings.setValue("discord/application_id", text.trimmed());
            refresh();
        });
        // Updates: update.sh downloads the latest main from GitHub and rebuilds.
        auto *updateBox = new QGroupBox("UPDATES");
        auto *updateLayout = new QVBoxLayout(updateBox);
        updateLabel = new QLabel("Not checked yet.");
        updateLabel->setWordWrap(true);
        updateLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        updateLayout->addWidget(updateLabel);
        auto *updateRow = new QHBoxLayout;
        checkButton = new QPushButton("Check for updates");
        updateButton = new QPushButton("Update now");
        checkButton->setMinimumHeight(36);
        updateButton->setMinimumHeight(36);
        updateRow->addWidget(checkButton);
        updateRow->addWidget(updateButton);
        updateRow->addStretch();
        updateLayout->addSpacing(6);
        updateLayout->addLayout(updateRow);
        updateLog = new QPlainTextEdit;
        updateLog->setReadOnly(true);
        updateLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        updateLog->setMinimumHeight(160);
        updateLog->setAccessibleName("Update output");
        updateLog->hide();
        updateLayout->addWidget(updateLog);
        layout->addWidget(updateBox);
        updateBox->setVisible(QFileInfo::exists(root + "/update.sh"));
        QObject::connect(checkButton, &QPushButton::clicked, this, [this] { runUpdate(true); });
        QObject::connect(updateButton, &QPushButton::clicked, this, [this] { runUpdate(false); });

        auto *folder = new QLabel(QString("Project Tux folder: %1").arg(root));
        folder->setTextInteractionFlags(Qt::TextSelectableByMouse);
        setTone(folder, "faint");
        layout->addSpacing(14);
        layout->addWidget(folder);
        layout->addStretch();
        return content;
    }

    QWidget *pathRow(QLineEdit *edit, bool directory, const QString &key) {
        auto *widget = new QWidget;
        auto *row = new QHBoxLayout(widget);
        row->setContentsMargins(0, 0, 0, 0);
        auto *browse = new QPushButton("Browse…");
        browse->setAccessibleName("Browse for " + key.section('/', -1));
        row->addWidget(edit, 1);
        row->addWidget(browse);
        QObject::connect(edit, &QLineEdit::textChanged, this, [this, key](const QString &text) {
            settings.setValue(key, text);
            refresh();
        });
        QObject::connect(browse, &QPushButton::clicked, this, [this, edit, directory] {
            const auto chosen =
                directory ? QFileDialog::getExistingDirectory(this, "Folder", edit->text())
                          : QFileDialog::getOpenFileName(this, "File", edit->text());
            if (!chosen.isEmpty())
                edit->setText(chosen);
        });
        return widget;
    }

    // ---- Logs ----
    QWidget *logsTab() {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(48, 18, 48, 120);
        auto *header = new QHBoxLayout;
        auto *title = new QLabel("LOGS");
        title->setFont(theme::display(theme::Page));
        auto *copy = new QPushButton("Copy log");
        auto *folder = new QPushButton("Open log folder");
        header->addWidget(title);
        header->addStretch();
        header->addWidget(copy);
        header->addWidget(folder);
        layout->addLayout(header);
        layout->addSpacing(8);
        output = new QPlainTextEdit;
        output->setReadOnly(true);
        output->setMaximumBlockCount(5000);
        output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        output->setPlaceholderText("Nothing yet. Press Play and the launch output appears here.");
        output->setAccessibleName("Session log");
        layout->addWidget(output, 1);
        QObject::connect(copy, &QPushButton::clicked, this,
                         [this] { QApplication::clipboard()->setText(output->toPlainText()); });
        QObject::connect(folder, &QPushButton::clicked, this, [this] { openLogFolder(); });
        return page;
    }

    // ---- setup check ----
    struct Check {
        QString name, path, fix;
    };
    QList<Check> checks() const {
        const auto pasta = QStandardPaths::findExecutable("pasta");
        const auto version = gameVersion(gameEdit->text());
        QList<Check> list{
            {version.isEmpty() ? "Fortnite" : "Fortnite " + version,
             gameEdit->text() + "/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe",
             "Choose the game folder above."},
            {"Supported build", root + "/profiles/fortnite/" + version + ".toml",
             "Project Tux has no profile for this build yet."},
            {"Proton", protonEdit->text(), "Import Proton: see docs/BACKEND.md."},
            {"Game prefix", data + "/proton-prefixes/" + version + "/pfx",
             "Start the game once from the command line: see docs/BACKEND.md."},
            {"Account relay", root + "/build/fortnite-backend", "Run ./build.sh."},
            {"Injector", root + "/build/windows/fol-inject.exe",
             "Run bash helpers/windows/build.sh."},
            {"Network router", root + "/build/windows/fol-match-router.dll",
             "Run bash helpers/windows/build.sh."},
        };
        // Builds with a sprint_layout (3.1) inject the sprint-by-default helper.
        if (QFile profile(root + "/profiles/fortnite/" + version + ".toml");
            profile.open(QIODevice::ReadOnly) && profile.readAll().contains("\nsprint_layout"))
            list.append({"Sprint helper", root + "/build/windows/fol-sprint.dll",
                         "Run bash helpers/windows/build.sh."});
        list += {
            {"Relay settings", config + "/backend/lawin.json",
             "See integrations/lawin/README.md."},
            {"Network sandbox (pasta)", pasta.isEmpty() ? "/nonexistent" : pasta,
             "Install it: " + installHint("passt") + "."},
        };
        return list;
    }
    bool ready(QStringList *missing = nullptr, Tab *where = nullptr) const {
        QStringList gaps;
        Tab first = SettingsTab;
        if (address().isEmpty() || !addressProblem(address()).isEmpty())
            gaps << "a server address";
        if (serverLogin->text().trimmed().isEmpty() || serverPassword->text().isEmpty())
            gaps << "your login and password";
        if (!gaps.isEmpty())
            first = ServerTab;
        for (const auto &check : checks())
            if (!QFileInfo::exists(check.path))
                gaps << check.name;
        if (missing)
            *missing = gaps;
        if (where)
            *where = first;
        return gaps.isEmpty();
    }

    // ---- state ----
    void refresh() {
        const auto version = gameVersion(gameEdit->text());

        const auto list = checks();
        for (qsizetype i = 0; i < list.size(); ++i) {
            const bool ok = QFileInfo::exists(list[i].path);
            checkLines[i].dot->setColour(ok ? theme::good : theme::bad);
            checkLines[i].value->setText(
                ok ? list[i].name.toHtmlEscaped()
                   : QString("%1 <span style='color:%2'>missing.</span> %3")
                         .arg(list[i].name.toHtmlEscaped(), theme::bad.name(),
                              list[i].fix.toHtmlEscaped()));
            checkLines[i].dot->setAccessibleName(ok ? "Found" : "Missing");
        }

        // Server address feedback.
        const auto problemText = addressProblem(address());
        serverAddress->setProperty("invalid", !problemText.isEmpty());
        serverAddress->style()->unpolish(serverAddress);
        serverAddress->style()->polish(serverAddress);
        if (!problemText.isEmpty()) {
            addressNote->setText(problemText);
            setTone(addressNote, "bad");
        } else if (portOf(address()) == "8090") {
            addressNote->setText("8090 is the admin website, not the game. Remove “:8090” "
                                 "unless your host said otherwise.");
            setTone(addressNote, "busy");
        } else {
            addressNote->clear();
        }
        addressNote->setVisible(!addressNote->text().isEmpty());

        const bool imageOk = backdrop->setImage(backgroundEdit->text().trimmed());
        backgroundNote->setText(imageOk ? "Any PNG or JPG. Leave empty for the default art."
                                        : "That image couldn't be opened, so the default art "
                                          "is showing.");
        setTone(backgroundNote, imageOk ? "faint" : "bad");

        feedDelay.start();

        // Account chip: who you are and how the connection stands.
        const auto login = serverLogin->text().trimmed();
        const bool inGame = client == State::Ready;
        const bool failed = !problem.text.isEmpty();
        const auto state = failed                  ? "Problem: " + problem.text
                           : inGame                ? "In game on " + serverCalled()
                           : relay == State::Ready ? "Signed in to " + serverCalled()
                           : running               ? "Signing in to " + serverCalled() + "…"
                                                   : QString("Not connected");
        account->set(login.isEmpty() ? "SIGN IN" : login.toUpper(),
                     failed                            ? theme::bad
                     : inGame || relay == State::Ready ? theme::good
                     : running                         ? theme::busy
                                                       : theme::idle,
                     state);

        const auto shown = version.isEmpty() ? QString() : version + " ";
        if (running && discordCheck->isChecked())
            discord.show(discordId->text().trimmed(), "Fortnite " + shown + "· Multiplayer",
                         !match.isEmpty() ? match : inGame ? "In the lobby" : "Starting…",
                         startedAt);
        else
            discord.clear();

        // Message, recovery and the main button.
        QStringList missing;
        Tab where = SettingsTab;
        const bool canLaunch = ready(&missing, &where);
        // Corner status: a title, one line of detail, a fix when there is one, and
        // launch progress.
        recoverTab = updating && !checking ? SettingsTab : failed ? problem.fix : where;
        const auto fix = recoverTab == ServerTab     ? QString("OPEN SERVER")
                         : recoverTab == SettingsTab ? QString("OPEN SETTINGS")
                                                     : QString("OPEN LOGS");
        const int step = !running                  ? 0
                         : inGame                  ? 5
                         : client == State::Starting ? 3
                         : relay == State::Ready   ? 2
                                                   : 1;
        const bool installing = updating && !checking;
        if (installing)
            status->set("UPDATING", "Rebuilding Project Tux. This takes a few minutes.",
                        theme::busy, -1, "OPEN SETTINGS");
        else if (stopping)
            status->set("STOPPING", "Closing Fortnite…", theme::busy);
        else if (running && inGame)
            status->set(match.isEmpty() ? "IN THE LOBBY" : match.toUpper(), lobbyMessage(),
                        theme::good);
        else if (running)
            status->set(relay == State::Ready ? "STARTING FORTNITE" : "SIGNING IN",
                        relay == State::Ready ? "Signed in as " + login + "."
                                              : "Connecting to " + serverCalled() + "…",
                        theme::busy, step / 5.0);
        else if (failed)
            status->set("LAUNCH FAILED", problem.text, theme::bad, -1, fix);
        else if (!canLaunch)
            status->set("SETUP NEEDED", "Add " + missing.join(", ") + ".", theme::busy, -1, fix);
        else
            status->set("READY", "Join " + serverCalled() + ".", theme::good);

        if (stopping)
            launchButton->setMode(theme::PlayButton::Mode::Busy, "Stopping…");
        else if (running)
            launchButton->setMode(theme::PlayButton::Mode::Stop, "Stop");
        else
            launchButton->setMode(theme::PlayButton::Mode::Launch, failed ? "Try again" : "Launch");
        launchButton->setEnabled(stopping || installing ? false : running || canLaunch);
        checkButton->setEnabled(!running && !updating);
        updateButton->setEnabled(!running && !updating && updateCount > 0);
        updateButton->setToolTip(running ? "Stop the game first" : QString());
        updateChip->setVisible(updateCount > 0);
        updateChip->set("UPDATE AVAILABLE", theme::accentSoft,
                        updateCount == 1 ? "1 update available. Install it in Settings"
                                         : QString("%1 updates available. Install them in "
                                                   "Settings")
                                               .arg(updateCount));

        settingsBox->setEnabled(!running);
        for (auto *box : serverBoxes)
            box->setEnabled(!running);
    }
    QString lobbyMessage() const {
        if (match == "Battle bus")
            return "On the battle bus. Thank the bus driver.";
        if (match == "Storm phases")
            return "In a match. The storm is closing in.";
        if (!match.isEmpty())
            return "In a match on " + serverCalled() + ".";
        return "In the lobby. Pick Battle Royale and press Play to join " + serverCalled() + ".";
    }
    void fail(const QString &text, Tab fix) {
        if (problem.text.isEmpty())
            problem = {text, fix};
    }

    void showPlayers(const ServerFeed::Snapshot &snapshot) {
        using Reach = ServerFeed::Reach;
        players->setVisible(snapshot.reach != Reach::None);
        const auto where = serverCalled();
        if (snapshot.reach == Reach::Checking)
            players->set("CHECKING…", theme::busy, "Checking " + where);
        else if (snapshot.reach == Reach::Offline)
            players->set("SERVER OFFLINE", theme::bad, serverCalled(true) + " isn't answering");
        else if (snapshot.players < 0)
            players->set("SERVER ONLINE", theme::good, serverCalled(true) + " is online");
        else
            players->set(snapshot.capacity > 0
                             ? QString("%1/%2 PLAYING").arg(snapshot.players).arg(snapshot.capacity)
                             : QString("%1 PLAYING").arg(snapshot.players),
                         theme::good,
                         QString("%1 player%2 on %3")
                             .arg(snapshot.players)
                             .arg(snapshot.players == 1 ? "" : "s")
                             .arg(where));
    }

    void log(const QString &line) { output->appendPlainText(line); }

    // ---- updates ----
    void runUpdate(bool check) {
        if (running || updating)
            return;
        updating = true;
        checking = check;
        updateText.clear();
        if (check) {
            updateLabel->setText("Checking for updates…");
        } else {
            updateLabel->setText("Updating… this takes a few minutes while it rebuilds.");
            updateLog->clear();
            updateLog->show();
        }
        updater.setWorkingDirectory(root);
        updater.start("/bin/bash", check ? QStringList{root + "/update.sh", "--check"}
                                         : QStringList{root + "/update.sh"});
        refresh();
    }
    void updateFinished(int code) {
        updating = false;
        const auto lines = updateText.trimmed().split('\n');
        const auto last = lines.isEmpty() ? QString() : lines.last().trimmed();
        const auto error = [](const QString &text) {
            return QString("<span style='color:%1'>%2</span>").arg(theme::bad.name(), text);
        };
        if (checking) {
            // update.sh --check: 0 up to date, 100 updates available, else an error.
            updateCount = code == 100 ? qMax(1, lines.first().section(' ', 0, 0).toInt()) : 0;
            if (code == 0 || code == 100) {
                QString text = "<b>" + lines.first().toHtmlEscaped() + "</b>";
                for (const auto &line : lines.mid(1))
                    text += "<br>" + line.trimmed().toHtmlEscaped();
                updateLabel->setText(text);
            } else {
                updateLabel->setText(error("Couldn't check: " + last.toHtmlEscaped()));
            }
            if (std::exchange(startupCheck, false) && code == 100 && !running) {
                refresh();
                offerUpdate(lines);
                return;
            }
        } else if (code == 0 && updateText.startsWith("Up to date")) {
            updateCount = 0;
            updateLabel->setText("<b>" + last.toHtmlEscaped() + "</b>");
        } else if (code == 0) {
            updateCount = 0;
            updateLabel->setText("<b>Updated.</b> Restart Project Tux to use the new version.");
            if (QMessageBox::question(this, "Project Tux",
                                      "Project Tux was updated. Restart it now?") ==
                QMessageBox::Yes) {
                QProcess::startDetached(self, {"--tab", "Settings"});
                close();
                return;
            }
        } else {
            updateLabel->setText(
                error("Update failed: " + last.toHtmlEscaped() + " (full output below)"));
        }
        refresh();
    }

    // After the startup check finds updates: install now or later (Settings).
    void offerUpdate(const QStringList &lines) {
        QMessageBox box(QMessageBox::Information, "Project Tux",
                        "<b>" + lines.first().chopped(1).toHtmlEscaped() + "</b>",
                        QMessageBox::NoButton, this);
        box.setInformativeText("Install now? Project Tux rebuilds, which takes a few "
                               "minutes, then asks to restart.");
        box.setDetailedText(lines.mid(1).replaceInStrings(QRegularExpression("^\\s+"), "")
                                .join('\n'));
        auto *now = box.addButton("Update now", QMessageBox::AcceptRole);
        box.addButton("Later", QMessageBox::RejectRole);
        box.setDefaultButton(now);
        box.exec();
        if (box.clickedButton() != now || running)
            return;
        tabs->setCurrentIndex(SettingsTab);
        QTimer::singleShot(0, this, [this] { runUpdate(false); });
    }

    void primaryAction() {
        if (stopping)
            return;
        running ? stop() : launch();
    }

    void launch() {
        if (running || (updating && !checking) || !ready())
            return;
        output->clear();
        session.clear();
        match.clear();
        problem = {};
        relay = State::Starting;
        client = State::Off;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("FOL_GAME_DIR", gameEdit->text());
        env.insert("FOL_PROTON", protonEdit->text());
        env.insert("FOL_MP_MODE", "join");
        env.insert("FOL_MP_SERVER", address());
        env.insert("FOL_MP_ACCOUNTS", "server");
        env.insert("FOL_MP_LOGIN", serverLogin->text().trimmed());
        env.insert("FOL_MP_PASSWORD", serverPassword->text());
        process.setProcessEnvironment(env);
        process.setWorkingDirectory(root);
        process.start("/bin/bash", {root + "/scripts/fortnite-match.sh", "--client"});
        if (!process.waitForStarted(5000)) {
            log("Could not start scripts/fortnite-match.sh: " + process.errorString());
            relay = State::Failed;
            fail("The launch script couldn't start: " + process.errorString() + ".", LogsTab);
            refresh();
            return;
        }
        running = true;
        startedAt = QDateTime::currentSecsSinceEpoch();
        poll.start();
        refresh();
    }

    void signalGroup(int signal) {
        const auto pid = process.processId();
        if (pid > 0)
            ::kill(-pid, signal);
    }
    void stop() {
        if (!running)
            return;
        log("— stopping —");
        stopping = true;
        signalGroup(SIGTERM);
        killTimer.start(15000);
        refresh();
    }

    // Turns the script's output into state and, for failures, a message that
    // names the problem and where to fix it.
    void handleLine(const QString &line) {
        if (line.isEmpty())
            return;
        log(line);
        static const QRegularExpression sessionLine("^Session logs: (.+)$");
        static const QRegularExpression clientExit("^Client exited \\(([0-9]+)\\)");
        if (const auto m = sessionLine.match(line); m.hasMatch())
            session = m.captured(1);
        else if (line.startsWith("Multiplayer: signing in") ||
                 line.contains("\"starting-lawin-v2\""))
            relay = State::Starting;
        else if (line.startsWith("Using the accounts on"))
            relay = State::Ready;
        else if (line.startsWith("Starting client")) {
            relay = State::Ready;
            client = State::Starting;
        } else if (line.startsWith("Client running"))
            client = State::Ready;
        else if (const auto m = clientExit.match(line); m.hasMatch()) {
            client = State::Off;
            if (m.captured(1) != "0" && !stopping)
                fail(QString("Fortnite closed with error code %1.").arg(m.captured(1)), LogsTab);
        } else if (line.startsWith("Backend: cannot reach the server's accounts")) {
            relay = State::Failed;
            fail("Couldn't reach the accounts on " + serverCalled() +
                     ". Check that the server is running and the address is right.",
                 ServerTab);
        } else if (line.startsWith("Backend:")) {
            relay = State::Failed;
            fail("Signing in stopped: " + line.mid(9) + ".", LogsTab);
        } else if (line.startsWith("Cannot resolve multiplayer server")) {
            relay = State::Failed;
            fail("Couldn't find that server. Check the address in the Server tab.", ServerTab);
        } else if (line.startsWith("Using the server's accounts needs")) {
            relay = State::Failed;
            fail("Enter the login and password your host gave you.", ServerTab);
        } else if (line.startsWith("Multiplayer needs pasta")) {
            relay = State::Failed;
            fail("Install the passt package so the game can reach the server.", SettingsTab);
        } else if (line.startsWith("Multiplayer: pasta could not attach") ||
                   line.startsWith("Multiplayer: no network inside the sandbox")) {
            relay = State::Failed;
            fail("The game sandbox couldn't get a network connection.", LogsTab);
        } else if (line.startsWith("No supported Fortnite build")) {
            relay = State::Failed;
            fail("This Fortnite build isn't supported. Choose another game folder.", SettingsTab);
        }
        refresh();
    }

    // Read the client's log for coarse match progress.
    void pollMatch() {
        if (!running) {
            poll.stop();
            return;
        }
        if (client != State::Ready)
            return;
        QFile file(data + "/proton-prefixes/" + gameVersion(gameEdit->text()) +
                   "/pfx/drive_c/users/steamuser/AppData/Local/FortniteGame/Saved/Logs/"
                   "FortniteGame.log");
        if (!file.open(QIODevice::ReadOnly))
            return;
        if (file.size() > 256 * 1024)
            file.seek(file.size() - 256 * 1024);
        const auto text = QString::fromUtf8(file.readAll());
        const struct {
            const char *marker, *label;
        } phases[] = {{"Join succeeded", "Joined the match"},
                      {"Set AthenaGamePhase to [Aircraft]", "Battle bus"},
                      {"Set AthenaGamePhase to [SafeZones]", "Storm phases"}};
        qsizetype best = -1;
        QString label;
        for (const auto &phase : phases) {
            const auto at = text.lastIndexOf(phase.marker);
            if (at > best) {
                best = at;
                label = phase.label;
            }
        }
        if (!label.isEmpty() && label != match) {
            match = label;
            refresh();
        }
    }

    QSettings settings{"ProjectTux", "launcher"};
    // The settings hold the server password: keep the file owner-only.
    // QSettings rewrites it atomically and keeps the existing permissions.
    void protectSettings() {
        settings.sync();
        QFile::setPermissions(settings.fileName(),
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    QString root, self, data, config, session, match, serverName, updateText;
    QProcess updater;
    QLabel *updateLabel;
    QPushButton *checkButton, *updateButton;
    QPlainTextEdit *updateLog;
    bool updating = false, checking = false, startupCheck = false;
    int updateCount = 0;
    DiscordPresence discord;
    qint64 startedAt = 0;
    QProcess process;
    QTimer killTimer, poll;
    bool running = false, stopping = false;
    State relay = State::Off, client = State::Off;
    Problem problem;
    Tab recoverTab = SettingsTab;
    theme::Backdrop *backdrop;
    theme::Brand *brand;
    theme::StatusChip *updateChip, *players, *account;
    AnnouncementsCard *news;
    ServerFeed feed;
    UpdatesFeed updates;
    QTimer feedDelay;
    QLabel *addressNote, *backgroundNote;
    theme::StatusBlock *status;
    QList<Row> checkLines;
    theme::PlayButton *launchButton;
    QPlainTextEdit *output;
    QCheckBox *discordCheck;
    QLineEdit *discordId, *gameEdit, *protonEdit, *serverAddress, *serverLogin, *serverPassword,
        *backgroundEdit;
    QGroupBox *settingsBox;
    QList<QGroupBox *> serverBoxes;
    QTabWidget *tabs;
};
} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Project Tux");
    QApplication::setDesktopFileName("project-tux");
    QApplication::setWindowIcon(QIcon(":/project-tux.svg"));
    theme::loadFonts();
    QApplication::setFont(theme::font(theme::Body));
    app.setStyleSheet(theme::styleSheet());
    Launcher window;
    window.show();
    // --launch: start immediately (used by the desktop entry's "Play now" action).
    const auto args = QCoreApplication::arguments();
    if (const auto at = args.indexOf("--tab"); at > 0 && at + 1 < args.size())
        window.showTab(args[at + 1]);
    if (args.contains("--launch"))
        QTimer::singleShot(0, &window, [&window] { window.launchNow(); });
    return app.exec();
}
