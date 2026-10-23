// SPDX-License-Identifier: GPL-3.0-only
// Launcher look ("Glacier"): an icy light ground with soft blurred strips, navy
// panels, one cobalt accent and Work Sans throughout. Painted widgets and the
// application style sheet; no game logic lives here.
#pragma once
#include <QAbstractButton>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>

namespace theme {
// Colours. Text tints come from the blue, never grey. "page" colours are for text
// on the light ground; the rest sit on navy panels. accentSoft is the accent where
// it has to be read on navy (actions, focus rings, progress).
inline const QColor ink("#0d1b3f"), panel("#0f1f4a"), raised("#1b2d63"), accent("#2457ff"),
    accentHover("#3d6bff"), accentDown("#1b46d9"), accentSoft("#8aa8ff"), onAccent("#ffffff"),
    text("#f3f6ff"), muted("#bccaf0"), faint("#8796c2"), page("#0d1b3f"), pageMuted("#44527d"),
    good("#4fe39b"), busy("#f5b942"), bad("#ff8f8f"), idle("#7e8bb3");
// Type scale in pixels.
enum Size { Caption = 13, Body = 15, Label = 17, Heading = 24, Page = 40, Hero = 72 };
constexpr qreal radius = 6;

// Bundled Work Sans weights (gui/fonts, SIL OFL), one family.
inline QString &family() {
    static QString name = "Work Sans";
    return name;
}
inline void loadFonts() {
    for (const char *file : {"WorkSans-Regular.ttf", "WorkSans-Medium.ttf", "WorkSans-SemiBold.ttf",
                             "WorkSans-Bold.ttf", "WorkSans-Black.ttf"}) {
        const auto id = QFontDatabase::addApplicationFont(QString(":/fonts/") + file);
        if (const auto list = QFontDatabase::applicationFontFamilies(id); !list.isEmpty())
            family() = list.first();
    }
}
inline QFont font(int px, QFont::Weight weight = QFont::Medium) {
    QFont f(family());
    f.setPixelSize(px);
    f.setWeight(weight);
    return f;
}
// Caps for the wordmark, titles, tabs and Launch: Black, or Bold when small.
inline QFont display(int px, bool black = true) {
    QFont f(family());
    f.setPixelSize(px);
    f.setWeight(black ? QFont::Black : QFont::Bold);
    f.setLetterSpacing(QFont::PercentageSpacing, black ? 98 : 102);
    return f;
}
// Parallelogram leaning right, like Fortnite's tabs and chips.
inline QPolygonF slant(const QRectF &r, qreal lean) {
    return QPolygonF{{r.left() + lean, r.top()},
                     {r.right(), r.top()},
                     {r.right() - lean, r.bottom()},
                     {r.left(), r.bottom()}};
}
inline void chevrons(QPainter &p, QPointF at, qreal h, const QColor &colour) {
    p.setPen(Qt::NoPen);
    p.setBrush(colour);
    for (int i = 0; i < 2; ++i) {
        const qreal x = at.x() + i * h * 0.62;
        p.drawPolygon(QPolygonF{{x, at.y()}, {x + h * 0.62, at.y() + h / 2}, {x, at.y() + h}});
    }
}

inline QString styleSheet() {
    QString sheet = R"(
QWidget { color: @page; }
QGroupBox, QGroupBox *, QFrame#panel, QFrame#panel *, QMenu, QToolTip, QPlainTextEdit,
QMessageBox, QMessageBox * { color: @text; }
QTabWidget::pane { border: 0; background: transparent; }
QScrollArea, QScrollArea > QWidget#qt_scrollarea_viewport, QScrollArea > QWidget > QWidget {
    background: transparent; border: 0;
}
QLabel { background: transparent; }
QLabel[tone="muted"] { color: @pageMuted; }
QLabel[tone="faint"] { color: @pageMuted; }
QGroupBox QLabel[tone="muted"] { color: @muted; }
QGroupBox QLabel[tone="faint"] { color: @faint; }
QLabel[tone="bad"] { color: @bad; }
QLabel[tone="busy"] { color: @busy; }
QFrame#panel { background: @panel; border-radius: 8px; }
QGroupBox {
    background: @panel; border: 0; border-radius: 8px; margin-top: 16px;
    padding: 46px 18px 16px 18px;
    font-family: "@family"; font-weight: 700; font-size: 16px;
}
QGroupBox::title {
    subcontrol-origin: padding; subcontrol-position: top left; left: 18px; top: 14px;
    color: @text;
}
QGroupBox QWidget { font-family: "@family"; font-weight: 500; font-size: 15px; }
QLineEdit, QSpinBox {
    background: #ffffff; color: @ink; border: 2px solid #ffffff; border-radius: 6px;
    padding: 5px 8px; selection-background-color: @accent; selection-color: #ffffff;
}
QLineEdit:focus, QSpinBox:focus { border-color: @accent; }
QLineEdit[invalid="true"] { border-color: @bad; }
QLineEdit:disabled { background: #b8c5ea; border-color: #b8c5ea; color: #33427a; }
QPushButton {
    background: @raised; color: @text; border: 2px solid transparent; border-radius: 6px;
    padding: 6px 16px; font-family: "@family"; font-weight: 600; font-size: 15px;
}
QPushButton:hover { background: #26397a; }
QPushButton:pressed, QPushButton:checked { background: @panel; }
QPushButton:focus { border-color: @accent; }
QGroupBox QPushButton:focus { border-color: @accentSoft; }
QPushButton:disabled { color: @faint; background: @panel; }
QCheckBox { spacing: 9px; }
QCheckBox:focus { color: @accentSoft; }
QCheckBox::indicator {
    width: 14px; height: 14px; background: #ffffff; border: 2px solid #ffffff; border-radius: 4px;
}
QCheckBox::indicator:checked { background: @accent; border-color: #ffffff; }
QPlainTextEdit {
    background: @panel; color: #d9e2ff; border: 2px solid transparent;
    border-radius: 8px; padding: 8px; selection-background-color: @accent; selection-color: #ffffff;
}
QPlainTextEdit:focus { border-color: @accent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: rgba(13, 27, 63, 0.25); border-radius: 3px; min-height: 36px; }
QScrollBar::handle:vertical:hover { background: rgba(13, 27, 63, 0.45); }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: rgba(13, 27, 63, 0.25); border-radius: 3px; min-width: 36px; }
QPlainTextEdit QScrollBar::handle:vertical, QPlainTextEdit QScrollBar::handle:horizontal {
    background: rgba(255, 255, 255, 0.25);
}
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
QToolTip { background: @ink; border: 0; padding: 6px 8px; }
QMenu { background: @panel; border: 0; padding: 6px; }
QMenu::item { padding: 8px 22px 8px 14px; border-radius: 4px; }
QMenu::item:selected { background: @accent; color: #ffffff; }
QMenu::separator { height: 1px; background: rgba(255, 255, 255, 0.15); margin: 4px 8px; }
QMessageBox { background: @panel; }
)";
    // Longer tokens first, so "@accentSoft" isn't eaten by "@accent".
    const std::array<std::pair<const char *, QString>, 14> tokens{{
        {"@family", family()},
        {"@pageMuted", pageMuted.name()},
        {"@page", page.name()},
        {"@accentSoft", accentSoft.name()},
        {"@accent", accent.name()},
        {"@raised", raised.name()},
        {"@panel", panel.name()},
        {"@text", text.name()},
        {"@muted", muted.name()},
        {"@faint", faint.name()},
        {"@ink", ink.name()},
        {"@busy", busy.name()},
        {"@bad", bad.name()},
        {"@good", good.name()},
    }};
    for (const auto &[token, value] : tokens)
        sheet.replace(token, value);
    return sheet;
}

// Painted controls repaint on hover and show a focus ring only when focus
// arrived from the keyboard.
template <class Base> class Interactive : public Base {
  public:
    using Base::Base;

  protected:
    bool keyboardFocus() const { return ring && this->hasFocus(); }
    void focusInEvent(QFocusEvent *event) override {
        ring = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
               event->reason() == Qt::ShortcutFocusReason;
        Base::focusInEvent(event);
        this->update();
    }
    void focusOutEvent(QFocusEvent *event) override {
        ring = false;
        Base::focusOutEvent(event);
        this->update();
    }
    void enterEvent(QEnterEvent *event) override {
        Base::enterEvent(event);
        this->update();
    }
    void leaveEvent(QEvent *event) override {
        Base::leaveEvent(event);
        this->update();
    }

  private:
    bool ring = false;
};

// Window background: the key art (painted, or the user's image), with scrims
// so text stays readable, dimmed behind the form pages.
class Backdrop : public QWidget {
  public:
    using QWidget::QWidget;
    // Returns false when a non-empty path could not be loaded.
    bool setImage(const QString &path) {
        if (path != imagePath) {
            imagePath = path;
            image = path.isEmpty() ? QImage() : QImage(path);
            update();
        }
        return path.isEmpty() || !image.isNull();
    }
    void setDimmed(bool on) {
        dimmed = on;
        update();
    }
    // Widgets kept in the bottom corners, above the pages: the status block on the
    // left, the launch block on the right.
    void setCorner(QWidget *widget, Qt::Corner where) {
        (where == Qt::BottomLeftCorner ? leftCorner : rightCorner) = widget;
        widget->setParent(this);
        place();
    }

  protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        place();
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r = rect();
        if (!image.isNull()) {
            const auto scaled = QSizeF(image.size()).scaled(r.size(), Qt::KeepAspectRatioByExpanding);
            p.drawImage(QRectF(r.center() - QPointF(scaled.width() / 2, scaled.height() / 2), scaled),
                        image);
        } else {
            if (soft.size() != size() * devicePixelRatioF())
                soft = scene(size() * devicePixelRatioF());
            p.drawImage(r, soft);
        }
        QLinearGradient shade(QPointF(0, r.height()), QPointF(0, r.height() * 0.5));
        shade.setColorAt(0, QColor(13, 27, 63, 18));
        shade.setColorAt(1, QColor(13, 27, 63, 0));
        p.fillRect(r, shade);
        if (dimmed)
            p.fillRect(r, QColor(255, 255, 255, 160));
    }

  private:
    void place() {
        if (leftCorner) {
            leftCorner->adjustSize();
            leftCorner->move(0, height() - leftCorner->height());
            leftCorner->raise();
        }
        if (rightCorner) {
            rightCorner->adjustSize();
            rightCorner->move(width() - rightCorner->width(), height() - rightCorner->height());
            rightCorner->raise();
        }
    }

  public:
    // Icy white-blue with a few diagonal white and cobalt strips, drawn at an
    // eighth of the size and scaled up in steps so it reads as one soft blur.
    static QImage scene(QSize size) {
        // Small targets (announcement banners) need a few pixels of strip to blur.
        const qreal scale = std::max(1.0, 160.0 / (size.width() / 8.0));
        const qreal w = size.width() / 8.0 * scale, h = size.height() / 8.0 * scale;
        QImage small(QSize(int(w) + 1, int(h) + 1), QImage::Format_ARGB32_Premultiplied);
        QPainter p(&small);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient sky(0, 0, w, h);
        sky.setColorAt(0, QColor("#dfeaff"));
        sky.setColorAt(0.55, QColor("#eef4ff"));
        sky.setColorAt(1, QColor("#ffffff"));
        p.fillRect(small.rect(), sky);
        const struct {
            qreal x, width;
            QColor colour;
        } strips[] = {{0.18, 0.06, QColor(255, 255, 255, 240)},
                      {0.36, 0.2, QColor(36, 87, 255, 22)},
                      {0.58, 0.12, QColor(36, 87, 255, 110)},
                      {0.69, 0.04, QColor(255, 255, 255, 255)},
                      {0.86, 0.05, QColor(36, 87, 255, 80)}};
        p.setPen(Qt::NoPen);
        for (const auto &strip : strips) {
            p.save();
            p.translate(w * strip.x, h * 0.5);
            p.rotate(-62);
            // Fades out across its width, so scaling up leaves no hard edge.
            const qreal length = (w + h) * 2, half = h * strip.width / 2;
            QLinearGradient across(0, -half, 0, half);
            auto clear = strip.colour;
            clear.setAlpha(0);
            across.setColorAt(0, clear);
            across.setColorAt(0.5, strip.colour);
            across.setColorAt(1, clear);
            p.setBrush(across);
            p.drawRect(QRectF(-length / 2, -half, length, half * 2));
            p.restore();
        }
        p.end();
        return small.scaled(size / 3, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

  private:
    QImage soft;
    QPointer<QWidget> leftCorner, rightCorner;
    QString imagePath;
    QImage image;
    bool dimmed = false;
};

// State dot for status rows and the top bar.
class Dot : public QWidget {
  public:
    Dot() { setFixedSize(10, 10); }
    void setColour(const QColor &value) {
        colour = value;
        update();
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(colour);
        p.drawEllipse(rect());
    }

  private:
    QColor colour = idle;
};

// Slanted tab: on the light top bar, or on a navy panel (onDark).
class NavButton : public Interactive<QAbstractButton> {
  public:
    explicit NavButton(const QString &label, bool onDark = false) : dark(onDark) {
        setText(label);
        setAccessibleName(label);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFont(display(15, false));
    }
    QSize sizeHint() const override {
        return {QFontMetrics(font()).horizontalAdvance(text()) + 36, 34};
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r(0, 0, width(), height() - 3);
        p.setPen(Qt::NoPen);
        if (isChecked()) {
            p.setBrush(ink);
            p.drawPolygon(slant(r.translated(0, 3), 6));
        }
        const QColor rest = dark ? QColor(255, 255, 255, 0) : QColor(13, 27, 63, 18);
        const QColor hover = dark ? QColor(255, 255, 255, 26) : QColor(13, 27, 63, 36);
        p.setBrush(isChecked() ? accent : underMouse() ? hover : rest);
        p.drawPolygon(slant(r, 6));
        p.setFont(font());
        const QColor label = isChecked() ? onAccent : dark ? theme::text : page;
        p.setPen(label);
        p.drawText(r, Qt::AlignCenter, text());
        if (keyboardFocus()) {
            p.setPen(QPen(isChecked() ? onAccent : dark ? accentSoft : accent, 2));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(slant(r.adjusted(3, 3, -3, -3), 5));
        }
    }

  private:
    bool dark;
};

// Big accent block in the bottom-right corner, joined to the Options button on
// its left. It is also the Stop button while a session runs.
class PlayButton : public Interactive<QPushButton> {
  public:
    enum class Mode { Launch, Stop, Busy };
    PlayButton() {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(300, 96);
    }
    void setMode(Mode value, const QString &label) {
        mode = value;
        setText(label);
        setAccessibleName(label);
        update();
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor fill, ink = onAccent;
        if (!isEnabled() || mode == Mode::Busy) {
            // Not yet: the panels' navy with a quiet label, not a broken grey.
            fill = panel;
            ink = QColor(135, 150, 194, 170);
        } else if (mode == Mode::Stop) {
            fill = isDown() ? panel : underMouse() ? QColor("#26397a") : raised;
            ink = theme::text;
        } else {
            fill = isDown() ? accentDown : underMouse() ? accentHover : accent;
        }
        p.fillRect(rect(), fill);
        // Painted from the display face: the style sheet's button font would win.
        const auto label = text().toUpper();
        p.setFont(display(label.size() > 8 ? 28 : 38));
        const QFontMetrics m(p.font());
        const qreal arrows = 30, gap = 14;
        const qreal x = (width() - (arrows + gap + m.horizontalAdvance(label))) / 2;
        chevrons(p, {x, height() / 2.0 - 11}, 22, ink);
        p.setPen(ink);
        p.drawText(QRectF(x + arrows + gap, 0, width(), height()), Qt::AlignVCenter, label);
        if (keyboardFocus()) {
            p.setPen(QPen(onAccent, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(rect()).adjusted(6, 6, -6, -6));
        }
    }

  private:
    Mode mode = Mode::Launch;
};

// Launcher state in the bottom-left corner, the launch block's mirror: cut at an
// angle on its right, a title, one line of detail, a fix when there is one and
// launch progress.
class StatusBlock : public Interactive<QAbstractButton> {
  public:
    StatusBlock() { setFixedSize(470, 96); }
    void set(const QString &title, const QString &detail, const QColor &tone,
             qreal progress = -1, const QString &action = {}) {
        heading = title;
        line = detail;
        colour = tone;
        done = progress;
        fix = action;
        setText(title);
        setAccessibleName(title + ". " + detail + (fix.isEmpty() ? "" : " " + fix));
        setToolTip(detail);
        setCursor(fix.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
        setFocusPolicy(fix.isEmpty() ? Qt::NoFocus : Qt::StrongFocus);
        update();
    }
    const QString &action() const { return fix; }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal w = width(), h = height();
        p.setPen(Qt::NoPen);
        p.setBrush(!fix.isEmpty() && underMouse() ? raised : panel);
        p.drawPolygon(QPolygonF{{0, 0}, {w - 28, 0}, {w, h}, {0, h}});
        p.setBrush(colour);
        p.drawPolygon(QPolygonF{{w - 35, 0}, {w - 28, 0}, {w, h}, {w - 7, h}});

        const qreal left = 26, right = w - 52;
        const QFont titleFont = display(20, false), lineFont = theme::font(Body);
        const QFontMetrics big(titleFont), small(lineFont);
        // Detail: up to two lines, one when the progress line is showing.
        const int maxLines = done >= 0 ? 1 : 2;
        const qreal width = right - left;
        const int wrapped = small.boundingRect(QRect(0, 0, int(width), 1000), Qt::TextWordWrap,
                                               line).height() / small.lineSpacing();
        const int lines = std::clamp(wrapped, 1, maxLines);
        const auto fitted = wrapped <= maxLines
                                ? line
                                : small.elidedText(line, Qt::ElideRight,
                                                   int(width * lines - small.averageCharWidth() * 4));
        // Centre title, detail and progress as one group in the block.
        const qreal titleH = big.height(), gap = 4, lineH = small.lineSpacing() * lines,
                    barH = done >= 0 ? 14 : 0;
        qreal y = (h - (titleH + gap + lineH + barH)) / 2;

        p.setFont(titleFont);
        p.setPen(theme::text);
        qreal room = width;
        if (!fix.isEmpty()) {
            const QFont actionFont = display(14, false);
            const QFontMetrics am(actionFont);
            const qreal actionW = am.horizontalAdvance(fix);
            const qreal ax = right - actionW, ah = am.height();
            const qreal ay = y + (titleH - ah) / 2;
            chevrons(p, {ax - 22, ay + ah / 2 - 6}, 12, accentSoft);
            p.setFont(actionFont);
            p.setPen(accentSoft);
            p.drawText(QRectF(ax, ay, actionW + 2, ah), Qt::AlignLeft | Qt::AlignVCenter, fix);
            room = width - actionW - 40;
            p.setFont(titleFont);
            p.setPen(theme::text);
        }
        p.drawText(QRectF(left, y, room, titleH), Qt::AlignLeft | Qt::AlignVCenter,
                   big.elidedText(heading, Qt::ElideRight, int(room)));
        y += titleH + gap;
        p.setFont(lineFont);
        p.setPen(muted);
        p.drawText(QRectF(left, y, width, lineH), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   fitted);
        y += lineH;

        if (done >= 0) {
            const QRectF track(left, y + 10, width, 4);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 50));
            p.drawRoundedRect(track, 2, 2);
            p.setBrush(accentSoft);
            p.drawRoundedRect(QRectF(track.topLeft(), QSizeF(track.width() * done, 4)), 2, 2);
        }
        if (keyboardFocus()) {
            p.setPen(QPen(accentSoft, 2));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(QPolygonF{{5, 5}, {w - 34, 5}, {w - 8, h - 5}, {5, h - 5}});
        }
    }

  private:
    QString heading, line, fix;
    QColor colour = idle;
    qreal done = -1;
};

// Options half of the launch block: the angled cut and a gear. Opens a menu.
class OptionsButton : public Interactive<QPushButton> {
  public:
    OptionsButton() {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(104, 96);
        setAccessibleName("Options");
        setToolTip("Options");
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal w = width(), h = height();
        p.setPen(Qt::NoPen);
        p.setBrush(!isEnabled()   ? panel
                   : isDown()     ? accentDown
                   : underMouse() ? accentHover
                                  : accent);
        p.drawPolygon(QPolygonF{{28, 0}, {w, 0}, {w, h}, {0, h}});
        p.fillRect(QRectF(w - 2, 22, 2, h - 44), QColor(0, 0, 0, 46));
        const QPointF centre(w / 2 + 10, h / 2);
        QPainterPath gear;
        gear.addEllipse(centre, 11, 11);
        for (int i = 0; i < 8; ++i) {
            QPainterPath tooth;
            tooth.addRoundedRect(QRectF(-3, -16, 6, 8), 1, 1);
            QTransform t;
            t.translate(centre.x(), centre.y());
            t.rotate(i * 45);
            gear = gear.united(t.map(tooth));
        }
        QPainterPath hole;
        hole.addEllipse(centre, 5, 5);
        p.fillPath(gear.subtracted(hole), onAccent);
        if (keyboardFocus()) {
            p.setPen(QPen(onAccent, 2));
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(QPolygonF{{36, 6}, {w - 8, 6}, {w - 8, h - 6}, {10, h - 6}});
        }
    }
};

// The penguin mark, "PROJECT TUX" in navy and the accent block.
class Brand : public QWidget {
  public:
    Brand() {
        setFixedSize(350, 60);
        setAccessibleName("Project Tux");
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto mark = QIcon(":/project-tux.svg").pixmap(QSize(60, 60), devicePixelRatioF());
        const qreal left = mark.isNull() ? 0 : 68;
        if (!mark.isNull())
            p.drawPixmap(QRectF(0, 0, 60, 60), mark, mark.rect());
        p.setFont(display(34));
        const QFontMetrics m(p.font());
        const QString name = "PROJECT TUX";
        p.setPen(page);
        p.drawText(QPointF(left, 42), name);
        const qreal x = left + m.horizontalAdvance(name) + 8;
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawPolygon(slant(QRectF(x, 21, 28, 20), 4));
        p.setBrush(accent);
        p.drawPolygon(slant(QRectF(x, 18, 28, 20), 4));
    }
};

// Top-bar chip: a state dot and a label in a navy box (the player count, the
// account). Opens the Server page.
class StatusChip : public Interactive<QAbstractButton> {
  public:
    StatusChip() {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
    }
    void set(const QString &label, const QColor &state, const QString &description) {
        dot = state;
        setText(label);
        setToolTip(description);
        setAccessibleName(label);
        setAccessibleDescription(description);
        updateGeometry();
        update();
    }
    QSize sizeHint() const override {
        return {QFontMetrics(display(14, false)).horizontalAdvance(text()) + 46, 34};
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), underMouse() ? raised : panel);
        p.setPen(Qt::NoPen);
        p.setBrush(dot);
        p.drawEllipse(QPointF(18, height() / 2.0), 5, 5);
        p.setFont(display(14, false));
        p.setPen(theme::text);
        p.drawText(QRectF(32, 0, width() - 40, height()), Qt::AlignVCenter, text());
        if (keyboardFocus()) {
            p.setPen(QPen(accentSoft, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(rect()).adjusted(1, 1, -1, -1));
        }
    }

  private:
    QColor dot = idle;
};
} // namespace theme
