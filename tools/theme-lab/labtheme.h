/*
 * SPDX-FileCopyrightText: 2026 SmartType Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * labtheme.h — Standalone theme model for smarttype-theme-lab.
 *
 * No Fcitx5 headers included. Mirrors the production ThemeConfig/Theme
 * structure so that the preview renderer can use identical painting logic.
 */
#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace smarttype::themelab {

// ── Color ──────────────────────────────────────────────────────────────────

struct LabColor {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 1.0;

    LabColor() = default;
    LabColor(double r, double g, double b, double a = 1.0)
        : r(r), g(g), b(b), a(a) {}

    /// Parse "#RRGGBB" or "#RRGGBBAA" hex string. Returns false on failure.
    bool parseHex(const std::string &hex);

    /// Serialise to "#RRGGBBAA" hex string.
    std::string toHex() const;

    bool operator==(const LabColor &o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
};

// ── Margin ─────────────────────────────────────────────────────────────────

struct LabMargin {
    int left   = 0;
    int right  = 0;
    int top    = 0;
    int bottom = 0;

    LabMargin() = default;
    LabMargin(int l, int r, int t, int b)
        : left(l), right(r), top(t), bottom(b) {}
};

// ── Background / Highlight image config ────────────────────────────────────

struct LabBackgroundConfig {
    std::string image;        ///< Optional PNG path relative to theme dir
    LabColor    color;        ///< Fill color (used when no image)
    LabColor    borderColor;  ///< Border color
    int         borderWidth = 0;
    LabMargin   margin;
    LabMargin   overlayClipMargin;
    std::string overlay;
};

struct LabHighlightConfig : LabBackgroundConfig {
    LabMargin clickMargin;
};

// ── Action (prev/next page button) ─────────────────────────────────────────

struct LabActionConfig {
    std::string image;
    LabMargin   clickMargin;
};

// ── Input Panel ────────────────────────────────────────────────────────────

struct LabInputPanelConfig {
    // Text colours
    LabColor normalColor          { 0.0, 0.0, 0.0, 1.0 };
    LabColor highlightColor       { 1.0, 1.0, 1.0, 1.0 };
    LabColor highlightBgColor     { 0.647, 0.647, 0.647, 1.0 };
    LabColor highlightCandColor   { 1.0, 1.0, 1.0, 1.0 };

    std::optional<LabColor> candidateLabelColor;
    std::optional<LabColor> highlightCandLabelColor;
    std::optional<LabColor> candidateCommentColor;
    std::optional<LabColor> highlightCandCommentColor;

    // Font size factors (percent, 0–400)
    int labelTextSizeFactor   = 100;
    int commentTextSizeFactor = 75;

    // Layout
    bool fullWidthHighlight = true;
    bool verticalList       = false;

    // Margins
    LabMargin contentMargin;
    LabMargin textMargin { 5, 5, 5, 5 };
    LabMargin shadowMargin;

    // Backgrounds
    LabBackgroundConfig  background;
    LabHighlightConfig   highlight;

    // Action buttons
    LabActionConfig prev;
    LabActionConfig next;
};

// ── Full theme model ────────────────────────────────────────────────────────

struct LabTheme {
    std::string name        = "custom";
    std::string author;
    std::string description;

    LabInputPanelConfig inputPanel;

    // Font (Pango font description string, e.g. "Sans 10")
    std::string font = "Sans 10";

    // Directory containing theme assets (PNG/SVG files).
    // Set automatically by loadFromFile().
    std::filesystem::path themeDir;

    // Computed colours (mirrors Theme::populateColor)
    LabColor inputPanelBackground_;
    LabColor inputPanelBorder_;
    LabColor inputPanelHighlightCandBg_;
    LabColor inputPanelHighlightCandBorder_;
    LabColor inputPanelText_;
    LabColor inputPanelHighlightText_;
    LabColor inputPanelHighlightCandText_;
    LabColor inputPanelCandLabelText_;
    LabColor inputPanelHighlightCandLabelText_;
    LabColor inputPanelCandCommentText_;
    LabColor inputPanelHighlightCandCommentText_;

    /// Load parameters from an INI theme.conf file. Returns false on error.
    bool loadFromFile(const std::filesystem::path &confPath);

    /// Export current state to a theme.conf INI file.
    bool saveToFile(const std::filesystem::path &confPath) const;

    /// Resolve computed colours from config (call after loading or editing).
    void populateColors();

    /// Reset all fields to defaults.
    void resetToDefaults();
};

// ── Animation constants (read-only, hard-coded in production) ───────────────

struct LabAnimConstants {
    static constexpr int    slideDurationMs        = 200;
    static constexpr int    correctionPulseDurationMs = 120;
    static constexpr double clipRadiusPx           = 14.0;
    static constexpr double clipInsetX             = 18.0;
    static constexpr double clipInsetY             = 17.0;
    static constexpr double pulseBandWidthFraction = 0.40;
    static constexpr double pulsePeakAlpha         = 0.30;
    static constexpr int    frameIntervalUs        = 16000;  // ~60 fps
};

} // namespace smarttype::themelab
