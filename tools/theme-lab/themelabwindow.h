/*
 * SPDX-FileCopyrightText: 2026 SmartType Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * themelabwindow.h — Main Qt Widgets window for smarttype-theme-lab.
 *
 * Layout:
 *   ┌────────────────────────────────────────────────────────────┐
 *   │  [Preview panel — ThemePreviewWidget]                      │
 *   ├────────────────┬───────────────────────────────────────────┤
 *   │  Editor        │  Info panel                               │
 *   │  (left)        │  (animation constants, read-only)         │
 *   └────────────────┴───────────────────────────────────────────┘
 *   [Load theme.conf] [Save theme.conf] [Reset to defaults] [Cycle HL]
 */
#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFormLayout>
#include <QStatusBar>
#include <memory>
#include "labtheme.h"
#include "themepreviewwidget.h"

namespace smarttype::themelab {

class ThemeLabWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ThemeLabWindow(QWidget *parent = nullptr);
    ~ThemeLabWindow() override;

private slots:
    void onLoadFile();
    void onSaveFile();
    void onResetDefaults();
    void onCycleHighlight();

    /// Called whenever any editor widget changes value.
    void onParameterChanged();

private:
    // ── Theme state ──────────────────────────────────────────────────────
    LabTheme theme_;

    // ── Widgets ──────────────────────────────────────────────────────────
    ThemePreviewWidget *preview_ = nullptr;

    // Colour buttons (click → QColorDialog)
    struct ColorButton {
        QLabel       *label  = nullptr;
        QPushButton  *button = nullptr;
        LabColor     *target = nullptr;
    };
    std::vector<ColorButton> colorButtons_;

    // Margin spinboxes: each group has 4 spinboxes (L R T B)
    struct MarginGroup {
        QSpinBox *left   = nullptr;
        QSpinBox *right  = nullptr;
        QSpinBox *top    = nullptr;
        QSpinBox *bottom = nullptr;
        LabMargin *target = nullptr;
    };
    std::vector<MarginGroup> marginGroups_;

    // Font controls
    QLineEdit *fontEdit_           = nullptr;
    QSpinBox  *labelSizeFactorSpin_  = nullptr;
    QSpinBox  *commentSizeFactorSpin_ = nullptr;
    QCheckBox *verticalListCheck_  = nullptr;
    QSpinBox  *borderWidthSpin_    = nullptr;

    // Bottom bar
    QPushButton *loadBtn_   = nullptr;
    QPushButton *saveBtn_   = nullptr;
    QPushButton *resetBtn_  = nullptr;
    QPushButton *cycleBtn_  = nullptr;

    // ── Builder helpers ──────────────────────────────────────────────────
    QWidget *buildEditorPanel();
    QWidget *buildInfoPanel();

    /// Add a colour row to @p layout: label + coloured button.
    void addColorRow(QFormLayout *layout, const QString &name, LabColor &target);

    /// Add a margin group (4 spinboxes) to @p layout.
    void addMarginGroup(QFormLayout *layout, const QString &name, LabMargin &target);

    /// Refresh all editor widgets from theme_ state (called after load/reset).
    void syncEditorFromTheme();

    /// Refresh theme_ state from editor widgets and notify preview.
    void syncThemeFromEditor();

    /// Update the background color of a color button to reflect current value.
    void refreshColorButton(ColorButton &cb);

    bool blockSignals_ = false;
};

} // namespace smarttype::themelab
