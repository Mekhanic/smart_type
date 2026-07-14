/*
 * SPDX-FileCopyrightText: 2026 SmartType Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * labrenderer.h — Cairo/Pango rendering helpers for Theme Lab preview.
 *
 * Supports both solid-colour and 9-slice PNG/SVG backgrounds, matching
 * the production Theme::paint() / paintTile() logic in theme.cpp.
 */
#pragma once

#include <cairo.h>
#include <pango/pango.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "labtheme.h"

namespace smarttype::themelab {

// ── Mock candidate data ─────────────────────────────────────────────────────

struct MockCandidate {
    std::string label;   ///< e.g. "1."
    std::string text;    ///< e.g. "привет"
    std::string comment; ///< optional comment
};

// ── Image surface cache ─────────────────────────────────────────────────────

/// Loads and caches cairo_surface_t for PNG/SVG assets relative to theme dir.
class LabImageCache {
public:
    explicit LabImageCache(std::filesystem::path themeDir);
    ~LabImageCache();

    LabImageCache(const LabImageCache &) = delete;
    LabImageCache &operator=(const LabImageCache &) = delete;

    /// Load an image by filename (relative to theme dir). Returns nullptr on failure.
    cairo_surface_t *get(const std::string &filename);

    /// Clear all cached surfaces.
    void clear();

    void setThemeDir(const std::filesystem::path &dir);

private:
    std::filesystem::path themeDir_;
    std::unordered_map<std::string, cairo_surface_t *> cache_;
};

// ── Renderer context ────────────────────────────────────────────────────────

/// Holds Pango context, font description, and image cache for reuse across frames.
struct LabRenderCtx {
    explicit LabRenderCtx(const std::string &fontDesc,
                          const std::filesystem::path &themeDir = {});
    ~LabRenderCtx();

    LabRenderCtx(const LabRenderCtx &) = delete;
    LabRenderCtx &operator=(const LabRenderCtx &) = delete;

    PangoFontMap         *fontMap  = nullptr;
    PangoContext         *context  = nullptr;
    PangoFontDescription *desc     = nullptr;
    LabImageCache         images;

    int fontHeightPx() const;

    void setThemeDir(const std::filesystem::path &dir) { images.setThemeDir(dir); }
};

// ── 9-slice PNG tiling ──────────────────────────────────────────────────────

/**
 * Paint a 9-slice scaled surface — direct port of production paintTile().
 * @param image       Source cairo surface (PNG or SVG rasterised)
 * @param marginL/T/R/B  9-slice margins in pixels
 * @param alpha       Overall opacity
 */
void paintTile(cairo_t *cr, int width, int height, double alpha,
               cairo_surface_t *image,
               int marginLeft, int marginTop, int marginRight, int marginBottom);

// ── Top-level render entry point ─────────────────────────────────────────────

/**
 * Render a full candidate panel preview onto @p cr.
 * Uses image backgrounds when available (matching production appearance),
 * falls back to solid-colour rendering when no image is set.
 *
 * @param cr           Cairo context (already sized to width×height)
 * @param theme        Current theme state
 * @param rctx         Pango render context (also holds image cache)
 * @param candidates   Mock candidates to display
 * @param highlightIdx Index of highlighted candidate (-1 = none)
 * @param pillProgress Slide-pill animation progress [0.0 … 1.0] (1.0 = settled)
 */
void renderPanel(cairo_t *cr,
                 unsigned int width,
                 unsigned int height,
                 const LabTheme &theme,
                 LabRenderCtx  &rctx,
                 const std::vector<MockCandidate> &candidates,
                 int highlightIdx = 0,
                 double pillProgress = 1.0);

// ── Low-level painting helpers ───────────────────────────────────────────────

/// Set Cairo source to a LabColor.
void cairoSetColor(cairo_t *cr, const LabColor &c);

/**
 * Paint solid rounded-rectangle background (used when theme has no image).
 */
void paintBackground(cairo_t *cr, int width, int height,
                     const LabColor &fillColor,
                     const LabColor &borderColor,
                     int borderWidth,
                     double borderRadius = 0.0);

/**
 * Render a single line of text at (x, y) using Pango.
 */
void renderText(cairo_t *cr, PangoContext *ctx, PangoFontDescription *desc,
                const std::string &text,
                int x, int y,
                const LabColor &color);

// ── Theme path discovery ─────────────────────────────────────────────────────

/**
 * Try to find the installed active SmartType theme.conf.
 * Searches:
 *   1. ~/.local/share/fcitx5/themes/<name>/theme.conf  (user-installed)
 *   2. /usr/share/fcitx5/themes/<name>/theme.conf      (system)
 * Returns empty path if not found.
 */
std::filesystem::path findActiveThemeConf();

} // namespace smarttype::themelab
