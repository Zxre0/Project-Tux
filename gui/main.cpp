// SPDX-License-Identifier: GPL-3.0-only
// Project Tux launcher: one-click start of the local backend, game server and
// client through scripts/fortnite-match.sh.
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "lawin_tab.h"

#include <csignal>
#include <unistd.h>

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

enum class State { Off, Starting, Ready, Failed };
QString dot(State state, const QString &text) {
    const char *colour = state == State::Ready      ? "#2fbf4f"
                         : state == State::Starting ? "#e0a020"
                         : state == State::Failed   ? "#e04040"
                                                    : "#808080";
    return QString("<span style='color:%1;font-size:16pt'>●</span>&nbsp;%2")
        .arg(colour, text.toHtmlEscaped());
}

class Launcher : public QMainWindow {
  public:
    Launcher() {
        setWindowTitle("Project Tux");
        setMinimumSize(760, 560);
        root = repoRoot();
        data = xdg("XDG_DATA_HOME", ".local/share");
        config = xdg("XDG_CONFIG_HOME", ".config");

        tabs = new QTabWidget;
        tabs->addTab(playTab(), "Play");
        tabs->addTab(hostingTab(), "Hosting");
        lawin = new LawinTab(root);
        tabs->addTab(lawin, "Lawin");
        tabs->addTab(settingsTab(), "Settings");
        QObject::connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
            if (tabs->widget(index) == lawin && !lawinLoaded && !running) {
                lawinLoaded = true;
                lawin->load();
            }
        });
        setCentralWidget(tabs);

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
                             if (server != State::Failed)
                                 server = State::Off;
                             backend = client = State::Off;
                             match.clear();
                             running = false;
                             refresh();
                         });
        killTimer.setSingleShot(true);
        QObject::connect(&killTimer, &QTimer::timeout, this, [this] { signalGroup(SIGKILL); });
        poll.setInterval(2000);
        QObject::connect(&poll, &QTimer::timeout, this, [this] { pollMatch(); });
        refresh();
    }

    void launchNow() { launch(); }
    void showTab(const QString &name) {
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabText(i).compare(name, Qt::CaseInsensitive) == 0)
                tabs->setCurrentIndex(i);
    }

  protected:
    void closeEvent(QCloseEvent *event) override {
        if (running &&
            QMessageBox::question(this, "Project Tux", "Stop the running game and server?") !=
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
    // ---- tabs ----
    QWidget *playTab() {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        auto *title = new QLabel("<h2>Project Tux</h2>Fortnite 12.50 · Battle Royale Solo");
        layout->addWidget(title);

        auto *statusBox = new QGroupBox("Status");
        auto *grid = new QGridLayout(statusBox);
        backendLabel = new QLabel;
        serverLabel = new QLabel;
        clientLabel = new QLabel;
        matchLabel = new QLabel;
        grid->addWidget(backendLabel, 0, 0);
        grid->addWidget(serverLabel, 0, 1);
        grid->addWidget(clientLabel, 0, 2);
        grid->addWidget(matchLabel, 1, 0, 1, 3);
        layout->addWidget(statusBox);

        auto *buttons = new QHBoxLayout;
        launchButton = new QPushButton("Launch");
        launchButton->setMinimumHeight(52);
        auto font = launchButton->font();
        font.setPointSizeF(font.pointSizeF() * 1.4);
        font.setBold(true);
        launchButton->setFont(font);
        stopButton = new QPushButton("Stop");
        stopButton->setMinimumHeight(52);
        auto *logsButton = new QPushButton("Open logs");
        logsButton->setMinimumHeight(52);
        buttons->addWidget(launchButton, 3);
        buttons->addWidget(stopButton, 1);
        buttons->addWidget(logsButton, 1);
        layout->addLayout(buttons);
        QObject::connect(launchButton, &QPushButton::clicked, this, [this] { launch(); });
        QObject::connect(stopButton, &QPushButton::clicked, this, [this] { stop(); });
        QObject::connect(logsButton, &QPushButton::clicked, this, [this] {
            const auto dir = session.isEmpty()
                                 ? xdg("XDG_STATE_HOME", ".local/state") + "/logs/sessions"
                                 : session;
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });

        hint = new QLabel;
        hint->setWordWrap(true);
        layout->addWidget(hint);

        output = new QPlainTextEdit;
        output->setReadOnly(true);
        output->setMaximumBlockCount(5000);
        output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        layout->addWidget(output, 1);
        return page;
    }

    QWidget *hostingTab() {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        hostCheck = new QCheckBox("Host a local game server");
        hostCheck->setChecked(settings.value("hosting/enabled", true).toBool());
        auto font = hostCheck->font();
        font.setBold(true);
        hostCheck->setFont(font);
        layout->addWidget(hostCheck);
        auto *explain = new QLabel(
            "When enabled, Launch first starts a headless Fortnite 12.50 game server "
            "(Erbium) and waits until it is listening, then starts your game. Pressing "
            "Play in the lobby matches you onto it.<br><br>When disabled, only the backend "
            "and your game start: the lobby and locker work, but Play stays in the queue "
            "because there is no server to join.");
        explain->setWordWrap(true);
        layout->addWidget(explain);

        auto *box = new QGroupBox("Game server");
        auto *form = new QFormLayout(box);
        erbiumEdit = new QLineEdit(settings.value("hosting/erbium", defaultErbium()).toString());
        auto *browse = new QPushButton("Browse…");
        auto *row = new QHBoxLayout;
        row->addWidget(erbiumEdit, 1);
        row->addWidget(browse);
        form->addRow("Erbium DLL", row);
        form->addRow("Playlist", new QLabel("Solo (Playlist_DefaultSolo), no cheats, no siphon"));
        form->addRow("Address",
                     new QLabel("127.0.0.1:7777 (isolated, not reachable from outside)"));
        hostStatus = new QLabel;
        form->addRow("Server", hostStatus);
        layout->addWidget(box);
        auto *note = new QLabel(
            "The server shows a small frozen Fortnite window and an Erbium console while "
            "it runs. Leave them open. The bus leaves after the normal warmup.");
        note->setWordWrap(true);
        layout->addWidget(note);
        layout->addStretch();
        hostBox = box;

        QObject::connect(hostCheck, &QCheckBox::toggled, this, [this](bool on) {
            settings.setValue("hosting/enabled", on);
            refresh();
        });
        QObject::connect(erbiumEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
            settings.setValue("hosting/erbium", text);
            refresh();
        });
        QObject::connect(browse, &QPushButton::clicked, this, [this] {
            const auto file =
                QFileDialog::getOpenFileName(this, "Erbium DLL", erbiumEdit->text(), "DLL (*.dll)");
            if (!file.isEmpty())
                erbiumEdit->setText(file);
        });
        return page;
    }

    QWidget *settingsTab() {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        auto *box = new QGroupBox("Paths");
        auto *form = new QFormLayout(box);
        gameEdit = new QLineEdit(
            settings.value("paths/game", home("Documents/Fortnite/++Fortnite+Release-12"))
                .toString());
        protonEdit = new QLineEdit(
            settings.value("paths/proton", data + "/runtimes/experimental-20260924/proton")
                .toString());
        form->addRow("Fortnite 12.50 folder", pathRow(gameEdit, true, "paths/game"));
        form->addRow("Proton script", pathRow(protonEdit, false, "paths/proton"));
        layout->addWidget(box);
        settingsBox = box;

        auto *checks = new QGroupBox("Setup check");
        checkList = new QLabel;
        checkList->setTextFormat(Qt::RichText);
        checkList->setWordWrap(true);
        auto *checksLayout = new QVBoxLayout(checks);
        checksLayout->addWidget(checkList);
        auto *recheck = new QPushButton("Check again");
        checksLayout->addWidget(recheck, 0, Qt::AlignLeft);
        QObject::connect(recheck, &QPushButton::clicked, this, [this] { refresh(); });
        layout->addWidget(checks);
        layout->addWidget(new QLabel(QString("Project folder: %1").arg(root)));
        layout->addStretch();
        return page;
    }

    QWidget *pathRow(QLineEdit *edit, bool directory, const QString &key) {
        auto *widget = new QWidget;
        auto *row = new QHBoxLayout(widget);
        row->setContentsMargins(0, 0, 0, 0);
        auto *browse = new QPushButton("Browse…");
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

    // ---- setup check ----
    QString defaultErbium() const {
        const auto found = QDir(root + "/build-erbium")
                               .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto &dir : found)
            if (QFileInfo::exists(dir.filePath() + "/erbium-solo.dll"))
                return dir.filePath() + "/erbium-solo.dll";
        return root + "/build-erbium/erbium-solo.dll";
    }
    struct Check {
        QString name, path, fix;
        bool needed;
    };
    QList<Check> checks() const {
        const bool host = hostCheck->isChecked();
        return {
            {"Fortnite 12.50",
             gameEdit->text() + "/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe",
             "Set the game folder in Settings", true},
            {"Proton", protonEdit->text(), "Import Proton: see docs/BACKEND.md", true},
            {"Game prefix", data + "/proton-prefixes/12.50/pfx",
             "Run the lobby launch once: see docs/BACKEND.md", true},
            {"Backend runner", root + "/build/fortnite-backend", "Run ./build.sh", true},
            {"Injector", root + "/build/windows/fol-inject.exe",
             "Run bash helpers/windows/build.sh", true},
            {"Router DLL", root + "/build/windows/fol-match-router.dll",
             "Run bash helpers/windows/build.sh", true},
            {"Lawin backend", config + "/backend/lawin.json",
             "Prepare Lawin: see integrations/lawin/README.md", true},
            {"Local login", config + "/backend/local-login.json",
             "Prepare Lawin: see integrations/lawin/README.md", true},
            {"Erbium game server", erbiumEdit->text(), "Build Erbium: see docs/MATCH.md", host},
        };
    }
    bool ready(QStringList *missing = nullptr) const {
        bool ok = true;
        for (const auto &check : checks())
            if (check.needed && !QFileInfo::exists(check.path)) {
                ok = false;
                if (missing)
                    *missing << check.name;
            }
        return ok;
    }

    // ---- state ----
    void refresh() {
        QString list;
        for (const auto &check : checks()) {
            const bool ok = QFileInfo::exists(check.path);
            list += QString("%1 <b>%2</b>%3<br>")
                        .arg(ok             ? "<span style='color:#2fbf4f'>✔</span>"
                             : check.needed ? "<span style='color:#e04040'>✘</span>"
                                            : "<span style='color:#808080'>–</span>",
                             check.name.toHtmlEscaped(),
                             ok             ? QString()
                             : check.needed ? " — " + check.fix.toHtmlEscaped()
                                            : " — not needed (hosting off)");
        }
        checkList->setText(list);

        const bool host = hostCheck->isChecked();
        backendLabel->setText(dot(backend, "Backend"));
        serverLabel->setText(dot(host ? server : State::Off, host ? "Game server" : "Hosting off"));
        clientLabel->setText(dot(client, "Game"));
        matchLabel->setText(match.isEmpty() ? QString()
                                            : "Match: <b>" + match.toHtmlEscaped() + "</b>");
        hostStatus->setText(
            dot(host ? server : State::Off, !host                       ? "Disabled"
                                            : server == State::Ready    ? "Listening"
                                            : server == State::Starting ? "Starting…"
                                            : server == State::Failed   ? "Failed (see logs)"
                                                                        : "Not running"));

        QStringList missing;
        const bool canLaunch = ready(&missing);
        launchButton->setEnabled(!running && canLaunch);
        launchButton->setText(running ? "Running…" : "Launch");
        stopButton->setEnabled(running);
        lawin->setSessionRunning(running);
        hostCheck->setEnabled(!running);
        hostBox->setEnabled(!running);
        settingsBox->setEnabled(!running);
        if (!canLaunch)
            hint->setText("<span style='color:#e04040'>Setup incomplete: " +
                          missing.join(", ").toHtmlEscaped() + ". See the Settings tab.</span>");
        else if (running)
            hint->setText(client == State::Ready
                              ? "In the lobby choose Battle Royale → Solo → Play. Close the game "
                                "or press Stop to end the session."
                              : "Starting… this takes about a minute.");
        else
            hint->setText(host ? "Ready. Launch starts the backend, the game server and your game."
                               : "Ready. Hosting is off: Launch starts the backend and your game.");
    }

    void log(const QString &line) { output->appendPlainText(line); }

    void launch() {
        if (running || !ready())
            return;
        output->clear();
        session.clear();
        match.clear();
        backend = State::Starting;
        server = State::Off;
        client = State::Off;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("FOL_GAME_DIR", gameEdit->text());
        env.insert("FOL_PROTON", protonEdit->text());
        env.insert("FOL_ERBIUM_DLL", erbiumEdit->text());
        process.setProcessEnvironment(env);
        process.setWorkingDirectory(root);
        process.start("/bin/bash", {root + "/scripts/fortnite-match.sh",
                                    hostCheck->isChecked() ? "--both" : "--client"});
        if (!process.waitForStarted(5000)) {
            log("Could not start scripts/fortnite-match.sh: " + process.errorString());
            backend = State::Failed;
            refresh();
            return;
        }
        running = true;
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
        signalGroup(SIGTERM);
        killTimer.start(15000);
    }

    void handleLine(const QString &line) {
        if (line.isEmpty())
            return;
        log(line);
        static const QRegularExpression sessionLine("^Session logs: (.+)$");
        if (const auto m = sessionLine.match(line); m.hasMatch())
            session = m.captured(1);
        else if (line.contains("\"starting-lawin-v2\""))
            backend = State::Starting;
        else if (line.startsWith("Starting headless game server")) {
            backend = State::Ready;
            server = State::Starting;
        } else if (line.startsWith("Game server listening")) {
            server = State::Ready;
            match = "Waiting for players";
        } else if (line.startsWith("Game server exited early") ||
                   line.startsWith("Game server never listened")) {
            server = State::Failed;
        } else if (line.startsWith("Starting client")) {
            backend = State::Ready;
            client = State::Starting;
        } else if (line.startsWith("Client running"))
            client = State::Ready;
        else if (line.startsWith("Client exited"))
            client = State::Off;
        else if (line.startsWith("Backend:"))
            backend = State::Failed;
        refresh();
    }

    // Read the live server log for coarse match progress.
    void pollMatch() {
        if (!running) {
            poll.stop();
            return;
        }
        if (!hostCheck->isChecked() || server != State::Ready)
            return;
        QFile file(data + "/proton-prefixes/12.50/pfx/drive_c/users/steamuser/AppData/Local/"
                          "FortniteGame/Saved/Logs/FortniteGame.log");
        if (!file.open(QIODevice::ReadOnly))
            return;
        if (file.size() > 256 * 1024)
            file.seek(file.size() - 256 * 1024);
        const auto text = QString::fromUtf8(file.readAll());
        const struct {
            const char *marker, *label;
        } phases[] = {{"MatchState changed previous=EnteringMap current=WaitingToStart",
                       "Waiting for players"},
                      {"Join succeeded", "Player joined"},
                      {"current=InProgress", "In progress (warmup)"},
                      {"Set AthenaGamePhase to [Aircraft]", "Battle bus"},
                      {"Set AthenaGamePhase to [SafeZones]", "Storm phases"},
                      {"current=WaitingPostMatch", "Match over"},
                      {"current=LeavingMap", "Server left the match"}};
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
    QString root, data, config, session, match;
    QProcess process;
    QTimer killTimer, poll;
    bool running = false;
    State backend = State::Off, server = State::Off, client = State::Off;
    QLabel *backendLabel, *serverLabel, *clientLabel, *matchLabel, *hint, *hostStatus, *checkList;
    QPushButton *launchButton, *stopButton;
    QPlainTextEdit *output;
    QCheckBox *hostCheck;
    QLineEdit *erbiumEdit, *gameEdit, *protonEdit;
    QGroupBox *hostBox, *settingsBox;
    QTabWidget *tabs;
    LawinTab *lawin;
    bool lawinLoaded = false;
};
} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Project Tux");
    QApplication::setDesktopFileName("project-tux");
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
