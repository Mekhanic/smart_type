/*
 * SPDX-FileCopyrightText: 2026 SmartType Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * themepreviewwidget.cpp — Qt widget that shows a live Cairo-rendered preview.
 */
#include "themepreviewwidget.h"

#include <QPainter>
#include <QResizeEvent>
#include <cairo.h>
#include <cstring>

namespace smarttype::themelab {

// ── CairoSurface PIMPL ────────────────────────────────────────────────────

struct ThemePreviewWidget::CairoSurface {
    cairo_surface_t *surface = nullptr;
    cairo_t         *cr      = nullptr;

    CairoSurface(int width, int height) {
        surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        cr      = cairo_create(surface);
    }
    ~CairoSurface() {
        if (cr)      cairo_destroy(cr);
        if (surface) cairo_surface_destroy(surface);
    }

    int width()  const { return cairo_image_surface_get_width(surface); }
    int height() const { return cairo_image_surface_get_height(surface); }
};

// ── Default mock candidates ────────────────────────────────────────────────

static const std::vector<MockCandidate> kDefaultCandidates = {
    { "1.", "привет",    "" },
    { "2.", "приветик",  "" },
    { "3.", "приветствие", "" },
};

// ── ThemePreviewWidget ─────────────────────────────────────────────────────

ThemePreviewWidget::ThemePreviewWidget(QWidget *parent)
    : QWidget(parent)
    , candidates_(kDefaultCandidates)
{
    setMinimumSize(320, 60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Pill animation timer (~60fps)
    connect(&animTimer_, &QTimer::timeout, this, [this]() {
        constexpr int totalFrames = 12; // 200ms / 16ms
        pillProgress_ = std::min(1.0, pillProgress_ + 1.0 / totalFrames);
        update();
        if (pillProgress_ >= 1.0)
            animTimer_.stop();
    });
}

ThemePreviewWidget::~ThemePreviewWidget() = default;

void ThemePreviewWidget::setTheme(const LabTheme *theme) {
    theme_ = theme;
    if (theme_) {
        rctx_ = std::make_unique<LabRenderCtx>(theme_->font, theme_->themeDir);
    } else {
        rctx_.reset();
    }
    themeChanged();
}

void ThemePreviewWidget::setCandidates(const std::vector<MockCandidate> &candidates) {
    candidates_ = candidates;
    update();
}

void ThemePreviewWidget::setHighlightIndex(int idx) {
    if (idx != hlIdx_) {
        // Restart pill slide animation from left
        pillProgress_ = 0.0;
        animTimer_.start(16);
    }
    hlIdx_ = idx;
    update();
}

QSize ThemePreviewWidget::sizeHint() const {
    return { 500, 60 };
}

QSize ThemePreviewWidget::minimumSizeHint() const {
    return { 200, 48 };
}

void ThemePreviewWidget::themeChanged() {
    if (theme_) {
        // Rebuild render context with possibly new font and themeDir
        rctx_ = std::make_unique<LabRenderCtx>(theme_->font, theme_->themeDir);
    }
    renderToSurface();
    update();
}

void ThemePreviewWidget::cycleHighlight() {
    if (candidates_.empty()) return;
    setHighlightIndex((hlIdx_ + 1) % static_cast<int>(candidates_.size()));
}

void ThemePreviewWidget::paintEvent(QPaintEvent * /*event*/) {
    if (!surface_) {
        rebuildSurface();
        renderToSurface();
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Convert cairo ARGB32 → QImage
    // Cairo uses BGRA native-endian; Qt ARGB32_Premultiplied is the same on LE.
    auto *data = cairo_image_surface_get_data(surface_->surface);
    int stride  = cairo_image_surface_get_stride(surface_->surface);
    QImage img(data, surface_->width(), surface_->height(), stride,
               QImage::Format_ARGB32_Premultiplied);

    // Center vertically if widget is taller than surface
    int yOffset = std::max(0, (height() - surface_->height()) / 2);
    painter.drawImage(QPoint(0, yOffset), img);
}

void ThemePreviewWidget::resizeEvent(QResizeEvent * /*event*/) {
    rebuildSurface();
    renderToSurface();
}

void ThemePreviewWidget::rebuildSurface() {
    int w = std::max(1, width());
    int h = std::max(1, height());
    if (surface_ && surface_->width() == w && surface_->height() == h)
        return;
    surface_ = std::make_unique<CairoSurface>(w, h);
}

void ThemePreviewWidget::renderToSurface() {
    if (!surface_) rebuildSurface();
    if (!theme_ || !rctx_) return;

    renderPanel(surface_->cr,
                static_cast<unsigned>(surface_->width()),
                static_cast<unsigned>(surface_->height()),
                *theme_,
                *rctx_,
                candidates_,
                hlIdx_,
                pillProgress_);
    cairo_surface_flush(surface_->surface);
}

} // namespace smarttype::themelab
