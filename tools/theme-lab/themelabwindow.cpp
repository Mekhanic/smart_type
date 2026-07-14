/*
 * SPDX-FileCopyrightText: 2026 SmartType Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * themelabwindow.cpp — Main window implementation for smarttype-theme-lab.
 */
#include "themelabwindow.h"
#include "labrenderer.h"

#include <QApplication>
#include <QColorDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPalette>

namespace smarttype::themelab {

// ── Helper: LabColor ↔ QColor ──────────────────────────────────────────────

static QColor toQColor(const LabColor &c) {
    return QColor::fromRgbF(
        static_cast<float>(c.r),
        static_cast<float>(c.g),
        static_cast<float>(c.b),
        static_cast<float>(c.a));
}

static LabColor fromQColor(const QColor &qc) {
    return LabColor(qc.redF(), qc.greenF(), qc.blueF(), qc.alphaF());
}

// ── ThemeLabWindow constructor ──────────────────────────────────────────────

ThemeLabWindow::ThemeLabWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SmartType Theme Lab"));
    setMinimumSize(900, 600);

    // Try to auto-load the active SmartType theme for a pixel-accurate preview.
    auto activeConf = findActiveThemeConf();
    if (!activeConf.empty() && theme_.loadFromFile(activeConf)) {
        // loaded — syncEditorFromTheme() will be called after widgets are built
    } else {
        theme_.resetToDefaults();
    }

    // ── Central widget ──────────────────────────────────────────────────
    auto *central = new QWidget(this);
    auto *rootVBox = new QVBoxLayout(central);
    rootVBox->setContentsMargins(8, 8, 8, 8);
    rootVBox->setSpacing(6);

    // Preview area
    preview_ = new ThemePreviewWidget(this);
    preview_->setTheme(&theme_);
    preview_->setFixedHeight(100);
    rootVBox->addWidget(preview_);

    // Splitter: editor | info
    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->addWidget(buildEditorPanel());
    splitter->addWidget(buildInfoPanel());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    rootVBox->addWidget(splitter, 1);

    // Bottom bar
    auto *botBar = new QHBoxLayout();
    loadBtn_  = new QPushButton(QStringLiteral("Load theme.conf…"), central);
    saveBtn_  = new QPushButton(QStringLiteral("Export theme.conf…"), central);
    resetBtn_ = new QPushButton(QStringLiteral("Reset to defaults"), central);
    cycleBtn_ = new QPushButton(QStringLiteral("Cycle highlight →"), central);
    botBar->addWidget(loadBtn_);
    botBar->addWidget(saveBtn_);
    botBar->addWidget(resetBtn_);
    botBar->addStretch();
    botBar->addWidget(cycleBtn_);
    rootVBox->addLayout(botBar);

    setCentralWidget(central);

    // ── Connections ─────────────────────────────────────────────────────
    connect(loadBtn_,  &QPushButton::clicked, this, &ThemeLabWindow::onLoadFile);
    connect(saveBtn_,  &QPushButton::clicked, this, &ThemeLabWindow::onSaveFile);
    connect(resetBtn_, &QPushButton::clicked, this, &ThemeLabWindow::onResetDefaults);
    connect(cycleBtn_, &QPushButton::clicked, this, &ThemeLabWindow::onCycleHighlight);

    syncEditorFromTheme();

    // Show loaded theme name in status bar
    if (!theme_.themeDir.empty()) {
        statusBar()->showMessage(
            QStringLiteral("Loaded: %1  (%2)")
                .arg(QString::fromStdString(theme_.name))
                .arg(QString::fromStdString(theme_.themeDir.string())));
    } else {
        statusBar()->showMessage(QStringLiteral("Ready (default theme — no active theme found)"));
    }
}

ThemeLabWindow::~ThemeLabWindow() = default;

// ── Editor panel ────────────────────────────────────────────────────────────

QWidget *ThemeLabWindow::buildEditorPanel() {
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    auto *container = new QWidget();
    auto *vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(6);

    auto makeGroup = [&](const QString &title) -> QFormLayout * {
        auto *gb = new QGroupBox(title, container);
        auto *fl = new QFormLayout(gb);
        fl->setContentsMargins(6, 4, 6, 4);
        fl->setSpacing(4);
        vbox->addWidget(gb);
        return fl;
    };

    // ── Font ──────────────────────────────────────────────────────────
    auto *fontForm = makeGroup(QStringLiteral("Font"));
    fontEdit_ = new QLineEdit(container);
    fontEdit_->setPlaceholderText(QStringLiteral("e.g. Sans 10"));
    fontForm->addRow(QStringLiteral("Pango font:"), fontEdit_);
    connect(fontEdit_, &QLineEdit::editingFinished, this, &ThemeLabWindow::onParameterChanged);

    labelSizeFactorSpin_ = new QSpinBox(container);
    labelSizeFactorSpin_->setRange(0, 400);
    labelSizeFactorSpin_->setSuffix(QStringLiteral("%"));
    fontForm->addRow(QStringLiteral("Label size factor:"), labelSizeFactorSpin_);
    connect(labelSizeFactorSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ThemeLabWindow::onParameterChanged);

    commentSizeFactorSpin_ = new QSpinBox(container);
    commentSizeFactorSpin_->setRange(0, 400);
    commentSizeFactorSpin_->setSuffix(QStringLiteral("%"));
    fontForm->addRow(QStringLiteral("Comment size factor:"), commentSizeFactorSpin_);
    connect(commentSizeFactorSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ThemeLabWindow::onParameterChanged);

    verticalListCheck_ = new QCheckBox(QStringLiteral("Vertical candidate list"), container);
    fontForm->addRow(QString(), verticalListCheck_);
    connect(verticalListCheck_, &QCheckBox::toggled, this, &ThemeLabWindow::onParameterChanged);

    // ── Text colours ───────────────────────────────────────────────────
    auto *colorForm = makeGroup(QStringLiteral("Text colours"));
    addColorRow(colorForm, QStringLiteral("Normal text"),       theme_.inputPanel.normalColor);
    addColorRow(colorForm, QStringLiteral("Highlight text"),    theme_.inputPanel.highlightColor);
    addColorRow(colorForm, QStringLiteral("Highlight BG"),      theme_.inputPanel.highlightBgColor);
    addColorRow(colorForm, QStringLiteral("Highlight cand."),   theme_.inputPanel.highlightCandColor);

    // ── Panel background ───────────────────────────────────────────────
    auto *bgForm = makeGroup(QStringLiteral("Panel background"));
    addColorRow(bgForm, QStringLiteral("Fill color"),    theme_.inputPanel.background.color);
    addColorRow(bgForm, QStringLiteral("Border color"),  theme_.inputPanel.background.borderColor);
    borderWidthSpin_ = new QSpinBox(container);
    borderWidthSpin_->setRange(0, 20);
    bgForm->addRow(QStringLiteral("Border width (px):"), borderWidthSpin_);
    connect(borderWidthSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ThemeLabWindow::onParameterChanged);

    // ── Highlight pill ─────────────────────────────────────────────────
    auto *hlForm = makeGroup(QStringLiteral("Selection pill (highlight)"));
    addColorRow(hlForm, QStringLiteral("Pill fill"),   theme_.inputPanel.highlight.color);
    addColorRow(hlForm, QStringLiteral("Pill border"), theme_.inputPanel.highlight.borderColor);

    // ── Margins ────────────────────────────────────────────────────────
    auto *marginForm = makeGroup(QStringLiteral("Margins"));
    addMarginGroup(marginForm, QStringLiteral("Content margin"), theme_.inputPanel.contentMargin);
    addMarginGroup(marginForm, QStringLiteral("Text margin"),    theme_.inputPanel.textMargin);
    addMarginGroup(marginForm, QStringLiteral("BG margin"),      theme_.inputPanel.background.margin);
    addMarginGroup(marginForm, QStringLiteral("Pill margin"),    theme_.inputPanel.highlight.margin);

    vbox->addStretch();
    scroll->setWidget(container);
    return scroll;
}

// ── Info panel (animation constants) ────────────────────────────────────────

QWidget *ThemeLabWindow::buildInfoPanel() {
    auto *w = new QWidget();
    auto *vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(4, 4, 4, 4);

    auto *gb = new QGroupBox(QStringLiteral("Animation constants (read-only)"), w);
    auto *fl = new QFormLayout(gb);
    fl->setContentsMargins(6, 4, 6, 4);

    auto addConst = [&](const QString &name, const QString &value) {
        auto *lbl = new QLabel(value);
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fl->addRow(name + QStringLiteral(":"), lbl);
    };

    addConst(QStringLiteral("Pill slide duration"),
             QStringLiteral("%1 ms").arg(LabAnimConstants::slideDurationMs));
    addConst(QStringLiteral("Easing function"),
             QStringLiteral("easeOutCubic"));
    addConst(QStringLiteral("Clip corner radius"),
             QStringLiteral("%1 px").arg(LabAnimConstants::clipRadiusPx));
    addConst(QStringLiteral("Clip inset X"),
             QStringLiteral("%1 px").arg(LabAnimConstants::clipInsetX));
    addConst(QStringLiteral("Clip inset Y"),
             QStringLiteral("%1 px").arg(LabAnimConstants::clipInsetY));
    addConst(QStringLiteral("Correction pulse"),
             QStringLiteral("%1 ms").arg(LabAnimConstants::correctionPulseDurationMs));
    addConst(QStringLiteral("Pulse band width"),
             QStringLiteral("%1% of panel width")
             .arg(static_cast<int>(LabAnimConstants::pulseBandWidthFraction * 100)));
    addConst(QStringLiteral("Pulse peak alpha"),
             QStringLiteral("%1").arg(LabAnimConstants::pulsePeakAlpha));
    addConst(QStringLiteral("Frame interval"),
             QStringLiteral("%1 µs (~60 fps)").arg(LabAnimConstants::frameIntervalUs));

    auto *note = new QLabel(
        QStringLiteral("<i>These values are hard-coded in inputwindow.cpp.<br>"
                       "They cannot be changed via theme.conf.</i>"), w);
    note->setWordWrap(true);

    vbox->addWidget(gb);
    vbox->addWidget(note);
    vbox->addStretch();
    return w;
}

// ── addColorRow ─────────────────────────────────────────────────────────────

void ThemeLabWindow::addColorRow(QFormLayout *layout, const QString &name, LabColor &target) {
    auto *btn = new QPushButton();
    btn->setFixedSize(48, 22);
    btn->setFlat(true);
    btn->setAutoFillBackground(true);

    ColorButton cb { nullptr, btn, &target };
    refreshColorButton(cb);
    colorButtons_.push_back(cb);
    size_t idx = colorButtons_.size() - 1;

    connect(btn, &QPushButton::clicked, this, [this, idx]() {
        auto &cb = colorButtons_[idx];
        QColor initial = toQColor(*cb.target);
        QColor chosen  = QColorDialog::getColor(initial, this,
                             QStringLiteral("Choose colour"),
                             QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            *cb.target = fromQColor(chosen);
            refreshColorButton(cb);
            onParameterChanged();
        }
    });

    layout->addRow(name + QStringLiteral(":"), btn);
}

void ThemeLabWindow::refreshColorButton(ColorButton &cb) {
    QColor qc = toQColor(*cb.target);
    auto pal   = cb.button->palette();
    pal.setColor(QPalette::Button, qc);
    cb.button->setPalette(pal);
    cb.button->setToolTip(QString::fromStdString(cb.target->toHex()));
    cb.button->update();
}

// ── addMarginGroup ───────────────────────────────────────────────────────────

void ThemeLabWindow::addMarginGroup(QFormLayout *layout, const QString &name, LabMargin &target) {
    auto *row = new QHBoxLayout();
    auto makeSpin = [&](int &val) {
        auto *s = new QSpinBox();
        s->setRange(0, 100);
        s->setValue(val);
        s->setFixedWidth(52);
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ThemeLabWindow::onParameterChanged);
        row->addWidget(s);
        return s;
    };

    MarginGroup mg;
    mg.left   = makeSpin(target.left);
    mg.right  = makeSpin(target.right);
    mg.top    = makeSpin(target.top);
    mg.bottom = makeSpin(target.bottom);
    mg.target = &target;

    auto *rowLabels = new QHBoxLayout();
    for (const char *l : {"L", "R", "T", "B"}) {
        auto *lbl = new QLabel(QString::fromLatin1(l));
        lbl->setAlignment(Qt::AlignHCenter);
        lbl->setFixedWidth(52);
        rowLabels->addWidget(lbl);
    }

    auto *container = new QWidget();
    auto *vb = new QVBoxLayout(container);
    vb->setContentsMargins(0, 0, 0, 0);
    vb->setSpacing(0);
    vb->addLayout(rowLabels);
    vb->addLayout(row);

    layout->addRow(name + QStringLiteral(":"), container);
    marginGroups_.push_back(mg);
}

// ── syncEditorFromTheme / syncThemeFromEditor ────────────────────────────────

void ThemeLabWindow::syncEditorFromTheme() {
    blockSignals_ = true;

    fontEdit_->setText(QString::fromStdString(theme_.font));
    labelSizeFactorSpin_->setValue(theme_.inputPanel.labelTextSizeFactor);
    commentSizeFactorSpin_->setValue(theme_.inputPanel.commentTextSizeFactor);
    verticalListCheck_->setChecked(theme_.inputPanel.verticalList);
    borderWidthSpin_->setValue(theme_.inputPanel.background.borderWidth);

    for (auto &mg : marginGroups_) {
        mg.left->setValue(mg.target->left);
        mg.right->setValue(mg.target->right);
        mg.top->setValue(mg.target->top);
        mg.bottom->setValue(mg.target->bottom);
    }
    for (auto &cb : colorButtons_) {
        refreshColorButton(cb);
    }

    blockSignals_ = false;
    theme_.populateColors();
    preview_->setTheme(&theme_);
    preview_->themeChanged();
}

void ThemeLabWindow::syncThemeFromEditor() {
    theme_.font = fontEdit_->text().toStdString();
    theme_.inputPanel.labelTextSizeFactor   = labelSizeFactorSpin_->value();
    theme_.inputPanel.commentTextSizeFactor = commentSizeFactorSpin_->value();
    theme_.inputPanel.verticalList          = verticalListCheck_->isChecked();
    theme_.inputPanel.background.borderWidth = borderWidthSpin_->value();

    for (const auto &mg : marginGroups_) {
        mg.target->left   = mg.left->value();
        mg.target->right  = mg.right->value();
        mg.target->top    = mg.top->value();
        mg.target->bottom = mg.bottom->value();
    }
    // Colour buttons write directly to theme_ via pointer, no copy needed.

    theme_.populateColors();
    preview_->setTheme(&theme_);
    preview_->themeChanged();
}

// ── Slots ────────────────────────────────────────────────────────────────────

void ThemeLabWindow::onParameterChanged() {
    if (blockSignals_) return;
    syncThemeFromEditor();
    statusBar()->showMessage(QStringLiteral("Modified (unsaved)"));
}

void ThemeLabWindow::onLoadFile() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Load theme.conf"), QString(),
        QStringLiteral("Theme config (theme.conf *.conf);;All files (*)"));
    if (path.isEmpty()) return;

    if (!theme_.loadFromFile(path.toStdString())) {
        QMessageBox::warning(this, QStringLiteral("Load failed"),
                             QStringLiteral("Could not parse: ") + path);
        return;
    }
    syncEditorFromTheme();
    statusBar()->showMessage(QStringLiteral("Loaded: ") + path);
}

void ThemeLabWindow::onSaveFile() {
    syncThemeFromEditor();
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export theme.conf"), QStringLiteral("theme.conf"),
        QStringLiteral("Theme config (theme.conf *.conf);;All files (*)"));
    if (path.isEmpty()) return;

    if (!theme_.saveToFile(path.toStdString())) {
        QMessageBox::warning(this, QStringLiteral("Save failed"),
                             QStringLiteral("Could not write: ") + path);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Saved: ") + path);
}

void ThemeLabWindow::onResetDefaults() {
    theme_.resetToDefaults();
    syncEditorFromTheme();
    statusBar()->showMessage(QStringLiteral("Reset to defaults"));
}

void ThemeLabWindow::onCycleHighlight() {
    preview_->cycleHighlight();
}

} // namespace smarttype::themelab
