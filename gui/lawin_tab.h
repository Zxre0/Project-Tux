// SPDX-License-Identifier: GPL-3.0-only
// "Lawin" tab: edit the local backend account, progress, locker and item shop
// through scripts/lawin-admin.sh (only while no game session is running).
#pragma once
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QVBoxLayout>

#include <functional>

class LawinTab : public QScrollArea {
  public:
    explicit LawinTab(QString root) : root(std::move(root)) {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);

        status = new QLabel("Press Refresh to load your account.");
        status->setWordWrap(true);
        auto *top = new QHBoxLayout;
        auto *refreshButton = new QPushButton("Refresh");
        top->addWidget(status, 1);
        top->addWidget(refreshButton);
        layout->addLayout(top);
        QObject::connect(refreshButton, &QPushButton::clicked, this, [this] { load(); });

        // Account
        auto *account = new QGroupBox("Account");
        auto *accountForm = new QFormLayout(account);
        nameEdit = new QLineEdit;
        nameEdit->setMaxLength(24);
        auto *saveName = new QPushButton("Save name");
        auto *nameRow = new QHBoxLayout;
        nameRow->addWidget(nameEdit, 1);
        nameRow->addWidget(saveName);
        accountForm->addRow("Display name", nameRow);
        accountId = new QLabel;
        accountId->setTextInteractionFlags(Qt::TextSelectableByMouse);
        accountForm->addRow("Account ID", accountId);
        layout->addWidget(account);
        QObject::connect(saveName, &QPushButton::clicked, this,
                         [this] { run({"set-name", nameEdit->text()}, "Name saved"); });

        // Progress
        auto *progress = new QGroupBox("Progress");
        auto *progressForm = new QFormLayout(progress);
        level = spin(1000);
        tier = spin(1000);
        vbucks = spin(1000000000);
        vbucks->setSingleStep(1000);
        battlePass = new QCheckBox("Battle Pass purchased");
        progressForm->addRow("Season level", level);
        progressForm->addRow("Battle Pass tier", tier);
        progressForm->addRow("", battlePass);
        progressForm->addRow("V-Bucks", vbucks);
        auto *saveProgress = new QPushButton("Save progress");
        progressForm->addRow("", saveProgress);
        layout->addWidget(progress);
        QObject::connect(saveProgress, &QPushButton::clicked, this, [this] {
            const QJsonObject stats{{"level", level->value()},
                                    {"book_level", tier->value()},
                                    {"book_purchased", battlePass->isChecked()},
                                    {"vbucks", vbucks->value()}};
            run({"set-stats", QJsonDocument(stats).toJson(QJsonDocument::Compact)},
                "Progress saved");
        });

        // Locker
        auto *lockerBox = new QGroupBox("Locker");
        auto *lockerLayout = new QVBoxLayout(lockerBox);
        lockerSummary = new QLabel;
        lockerSummary->setWordWrap(true);
        lockerLayout->addWidget(lockerSummary);
        auto *lists = new QHBoxLayout;
        auto *addColumn = new QVBoxLayout;
        search = new QLineEdit;
        search->setPlaceholderText("Search cosmetics, e.g. cid_028, renegade, glider…");
        available = new QListWidget;
        available->setSelectionMode(QAbstractItemView::ExtendedSelection);
        auto *grantButton = new QPushButton("Add selected →");
        addColumn->addWidget(new QLabel("<b>All cosmetics</b>"));
        addColumn->addWidget(search);
        addColumn->addWidget(available, 1);
        addColumn->addWidget(grantButton);
        auto *ownedColumn = new QVBoxLayout;
        owned = new QListWidget;
        owned->setSelectionMode(QAbstractItemView::ExtendedSelection);
        auto *removeButton = new QPushButton("Remove selected");
        ownedColumn->addWidget(new QLabel("<b>Your locker</b>"));
        ownedColumn->addWidget(owned, 1);
        ownedColumn->addWidget(removeButton);
        lists->addLayout(addColumn, 1);
        lists->addLayout(ownedColumn, 1);
        lockerLayout->addLayout(lists);
        auto *bulk = new QHBoxLayout;
        auto *grantAll = new QPushButton("Unlock every cosmetic");
        auto *reset = new QPushButton("Reset to starter locker");
        bulk->addWidget(grantAll);
        bulk->addWidget(reset);
        bulk->addStretch();
        lockerLayout->addLayout(bulk);
        lockerLayout->addWidget(new QLabel(
            "<small>Items from seasons after your game's version can't be shown by the game and are skipped "
            "in the locker. Changes apply the next time you launch.</small>"));
        lockerBox->setMinimumHeight(420);
        layout->addWidget(lockerBox);
        QObject::connect(search, &QLineEdit::textChanged, this, [this] { filter(); });
        QObject::connect(grantButton, &QPushButton::clicked, this, [this] {
            QStringList ids{"grant"};
            for (auto *item : available->selectedItems())
                ids << item->text();
            if (ids.size() > 1)
                run(ids, QString("Added %1 item(s)").arg(ids.size() - 1));
        });
        QObject::connect(removeButton, &QPushButton::clicked, this, [this] {
            QStringList ids{"remove"};
            for (auto *item : owned->selectedItems())
                ids << item->text();
            if (ids.size() > 1)
                run(ids, QString("Removed %1 item(s)").arg(ids.size() - 1));
        });
        QObject::connect(grantAll, &QPushButton::clicked, this, [this] {
            if (QMessageBox::question(this, "Unlock every cosmetic",
                                      QString("Add all %1 cosmetics Lawin knows about to your "
                                              "locker? A backup of your profile is kept.")
                                          .arg(cosmetics.size())) == QMessageBox::Yes)
                run({"grant-all"}, "Unlocked every cosmetic");
        });
        QObject::connect(reset, &QPushButton::clicked, this, [this] {
            if (QMessageBox::question(this, "Reset locker",
                                      "Replace your locker with the 8 default outfits? A backup "
                                      "of your profile is kept.") == QMessageBox::Yes)
                run({"reset-locker"}, "Locker reset");
        });

        // Item shop
        auto *shopBox = new QGroupBox("Item shop");
        auto *shopLayout = new QVBoxLayout(shopBox);
        shop = new QTableWidget(0, 3);
        shop->setHorizontalHeaderLabels({"Slot", "Items (comma separated IDs)", "Price (V-Bucks)"});
        shop->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        shop->verticalHeader()->hide();
        shop->setMinimumHeight(220);
        shopLayout->addWidget(shop);
        auto *shopButtons = new QHBoxLayout;
        auto *addDaily = new QPushButton("Add daily slot");
        auto *addFeatured = new QPushButton("Add featured slot");
        auto *removeSlot = new QPushButton("Remove slot");
        auto *saveShop = new QPushButton("Save shop");
        for (auto *button : {addDaily, addFeatured, removeSlot})
            shopButtons->addWidget(button);
        shopButtons->addStretch();
        shopButtons->addWidget(saveShop);
        shopLayout->addLayout(shopButtons);
        layout->addWidget(shopBox);
        QObject::connect(addDaily, &QPushButton::clicked, this, [this] { addSlot("daily"); });
        QObject::connect(addFeatured, &QPushButton::clicked, this, [this] { addSlot("featured"); });
        QObject::connect(removeSlot, &QPushButton::clicked, this,
                         [this] { shop->removeRow(shop->currentRow()); });
        QObject::connect(saveShop, &QPushButton::clicked, this, [this] { writeShop(); });

        layout->addStretch();
        setWidget(page);
        setWidgetResizable(true);
        editors = {account, progress, lockerBox, shopBox};
    }

    // Called by the launcher: editing needs exclusive access to the database.
    void setSessionRunning(bool running) {
        sessionRunning = running;
        updateEnabled();
        if (running)
            status->setText("Stop the game to edit your account (the backend is in use).");
    }
    void load() {
        if (cosmetics.isEmpty())
            run({"cosmetics"}, {});
        run({"shop"}, {});
        run({"info"}, "Loaded");
    }

  private:
    static QSpinBox *spin(int max) {
        auto *box = new QSpinBox;
        box->setRange(0, max);
        return box;
    }
    void updateEnabled() {
        for (auto *widget : editors)
            widget->setEnabled(!sessionRunning && !busy && loaded);
    }

    // Commands run one at a time; each starts and stops its own private database.
    void run(QStringList args, QString done) {
        if (sessionRunning)
            return;
        queue.append({std::move(args), std::move(done)});
        if (!busy)
            next();
    }
    void next() {
        if (queue.isEmpty()) {
            busy = false;
            updateEnabled();
            return;
        }
        busy = true;
        updateEnabled();
        const auto [args, done] = queue.takeFirst();
        status->setText("Working…");
        auto *process = new QProcess(this);
        QObject::connect(
            process, &QProcess::finished, this,
            [this, process, args, done](int, QProcess::ExitStatus) {
                const auto output = process->readAllStandardOutput().trimmed();
                const auto lines = output.split('\n');
                const auto reply = QJsonDocument::fromJson(lines.last()).object();
                process->deleteLater();
                if (!reply["ok"].toBool()) {
                    queue.clear();
                    status->setText(
                        "<span style='color:#e04040'>" +
                        (reply["error"].toString().isEmpty()
                             ? QString::fromUtf8(process->readAllStandardError().left(300))
                             : reply["error"].toString())
                            .toHtmlEscaped() +
                        "</span>");
                } else {
                    apply(reply);
                    if (!done.isEmpty())
                        status->setText(done + ".");
                }
                next();
            });
        process->start("/bin/bash", QStringList{root + "/scripts/lawin-admin.sh"} + args);
    }

    void apply(const QJsonObject &reply) {
        if (reply.contains("cosmetics") && reply["cosmetics"].isArray()) {
            cosmetics.clear();
            for (const auto &id : reply["cosmetics"].toArray())
                cosmetics << id.toString();
            filter();
        }
        if (reply.contains("shop"))
            showShop(reply["shop"].toObject());
        if (reply.contains("username")) {
            loaded = true;
            nameEdit->setText(reply["username"].toString());
            accountId->setText(reply["accountId"].toString());
            level->setValue(reply["level"].toInt());
            tier->setValue(reply["book_level"].toInt());
            battlePass->setChecked(reply["book_purchased"].toBool());
            vbucks->setValue(reply["vbucks"].toInt());
            QStringList counts;
            int total = 0;
            const auto byType = reply["cosmetics"].toObject();
            for (auto it = byType.begin(); it != byType.end(); ++it) {
                total += it.value().toInt();
                counts << QString("%1 %2").arg(it.value().toInt()).arg(it.key().mid(6));
            }
            lockerSummary->setText(
                QString("<b>%1 items</b>: %2").arg(total).arg(counts.join(", ")));
            owned->clear();
            for (const auto &id : reply["owned"].toArray())
                owned->addItem(id.toString());
            updateEnabled();
        }
    }

    void filter() {
        available->clear();
        const auto text = search->text().trimmed();
        int shown = 0;
        for (const auto &id : cosmetics)
            if (text.isEmpty() || id.contains(text, Qt::CaseInsensitive)) {
                available->addItem(id);
                if (++shown == 500)
                    break;
            }
    }

    void showShop(const QJsonObject &config) {
        shop->setRowCount(0);
        for (auto it = config.begin(); it != config.end(); ++it) {
            if (it.key() == "//")
                continue;
            const auto entry = it.value().toObject();
            QStringList items;
            for (const auto &id : entry["itemGrants"].toArray())
                items << id.toString();
            const int row = shop->rowCount();
            shop->insertRow(row);
            auto *slot = new QTableWidgetItem(it.key());
            slot->setFlags(slot->flags() & ~Qt::ItemIsEditable);
            shop->setItem(row, 0, slot);
            shop->setItem(row, 1, new QTableWidgetItem(items.join(", ")));
            shop->setItem(row, 2, new QTableWidgetItem(QString::number(entry["price"].toInt())));
        }
    }
    void addSlot(const QString &kind) {
        int n = 1;
        for (bool used = true; used; ++n) {
            used = false;
            for (int row = 0; row < shop->rowCount(); ++row)
                used |= shop->item(row, 0)->text() == kind + QString::number(n);
            if (!used)
                break;
        }
        const int row = shop->rowCount();
        shop->insertRow(row);
        auto *slot = new QTableWidgetItem(kind + QString::number(n));
        slot->setFlags(slot->flags() & ~Qt::ItemIsEditable);
        shop->setItem(row, 0, slot);
        shop->setItem(row, 1, new QTableWidgetItem);
        shop->setItem(row, 2, new QTableWidgetItem("0"));
        shop->editItem(shop->item(row, 1));
    }
    void writeShop() {
        QJsonObject config{{"//", "BR Item Shop Config"}};
        for (int row = 0; row < shop->rowCount(); ++row) {
            QJsonArray items;
            for (const auto &id : shop->item(row, 1)->text().split(',', Qt::SkipEmptyParts))
                items.append(id.trimmed());
            config[shop->item(row, 0)->text()] =
                QJsonObject{{"itemGrants", items}, {"price", shop->item(row, 2)->text().toInt()}};
        }
        shopFile = std::make_unique<QTemporaryFile>();
        if (!shopFile->open()) {
            status->setText("Could not write a temporary file.");
            return;
        }
        shopFile->write(QJsonDocument(config).toJson());
        shopFile->flush();
        run({"shop-set", shopFile->fileName()}, "Item shop saved");
    }

    QString root;
    QStringList cosmetics;
    QList<QPair<QStringList, QString>> queue;
    bool busy = false, loaded = false, sessionRunning = false;
    std::unique_ptr<QTemporaryFile> shopFile;
    QList<QWidget *> editors;
    QLabel *status, *accountId, *lockerSummary;
    QLineEdit *nameEdit, *search;
    QSpinBox *level, *tier, *vbucks;
    QCheckBox *battlePass;
    QListWidget *available, *owned;
    QTableWidget *shop;
};
