/*
 * SPDX-FileCopyrightText: 2026 SmartType Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * themepreviewwidget.h — Qt widget that shows a live Cairo-rendered preview
 * of the candidate panel using the current LabTheme.
 */
#pragma once

#include <QWidget>
#include <QTimer>
#include <memory>
#include <vector>
#include "labtheme.h"
#include "labrenderer.h"

namespace smarttype::themelab {

/**
 * ThemePreviewWidget
 *
 * Renders the SmartType candidate panel preview using Cairo/Pango into an
 * offscreen cairo_image_surface, then blits it to the Qt widget via QPainter.
 *
 * The widget accepts a const reference to a LabTheme; call themeChanged()
 * to trigger a repaint after any parameter modification.
 */
class ThemePreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit ThemePreviewWidget(QWidget *parent = nullptr);
    ~ThemePreviewWidget() override;

    /// Set the theme to preview. Widget does NOT take ownership.
    void setTheme(const LabTheme *theme);

    /// Set mock candidates to render.
    void setCandidates(const std::vector<MockCandidate> &candidates);

    /// Set which candidate index is highlighted (for pill animation demo).
    void setHighlightIndex(int idx);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    /// Call when any theme parameter changes — triggers repaint.
    void themeChanged();

    /// Advance highlighted candidate (for cycling demo animation).
    void cycleHighlight();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void rebuildSurface();
    void renderToSurface();

    const LabTheme             *theme_      = nullptr;
    std::unique_ptr<LabRenderCtx> rctx_;
    std::vector<MockCandidate>  candidates_;
    int                         hlIdx_      = 0;

    // Cairo offscreen surface
    struct CairoSurface;
    std::unique_ptr<CairoSurface> surface_;

    int surfaceW_ = 0;
    int surfaceH_ = 0;

    // Pill slide animation
    QTimer  animTimer_;
    double  pillProgress_ = 1.0;
    int     animFrames_   = 0;
};

} // namespace smarttype::themelab
