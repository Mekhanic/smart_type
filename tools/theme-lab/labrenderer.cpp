/*
 * SPDX-FileCopyrightText: 2026 SmartType Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * labrenderer.cpp — Cairo/Pango rendering with 9-slice PNG support.
 *
 * Ported logic:
 *   - paintTile()     → exact copy of theme.cpp::paintTile()
 *   - renderPanel()   → mirrors inputwindow.cpp layout + theme.cpp::paint()
 *   - LabImageCache   → loads PNG via gdk-pixbuf, SVG via rsvg (if available),
 *                       falls back to cairo_image_surface_create_from_png()
 */
#include "labrenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

namespace smarttype::themelab {

// ═══════════════════════════════════════════════════════════════════════════
// LabImageCache
// ═══════════════════════════════════════════════════════════════════════════

LabImageCache::LabImageCache(std::filesystem::path themeDir)
    : themeDir_(std::move(themeDir)) {}

LabImageCache::~LabImageCache() {
    clear();
}

void LabImageCache::clear() {
    for (auto &[k, s] : cache_) {
        if (s) cairo_surface_destroy(s);
    }
    cache_.clear();
}

void LabImageCache::setThemeDir(const std::filesystem::path &dir) {
    if (dir != themeDir_) {
        clear();
        themeDir_ = dir;
    }
}

cairo_surface_t *LabImageCache::get(const std::string &filename) {
    if (filename.empty()) return nullptr;

    auto it = cache_.find(filename);
    if (it != cache_.end()) return it->second;

    std::filesystem::path path = themeDir_ / filename;
    if (!std::filesystem::exists(path)) {
        cache_[filename] = nullptr;
        return nullptr;
    }

    cairo_surface_t *surf = nullptr;
    std::string ext = path.extension().string();
    // Normalise extension to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".png") {
        // Load via gdk-pixbuf, then manually convert to Cairo ARGB32.
        GError *err = nullptr;
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(path.c_str(), &err);
        if (pb) {
            // Ensure RGBA8
            GdkPixbuf *rgba = pb;
            if (!gdk_pixbuf_get_has_alpha(pb)) {
                rgba = gdk_pixbuf_add_alpha(pb, FALSE, 0, 0, 0);
                g_object_unref(pb);
            }
            int w        = gdk_pixbuf_get_width(rgba);
            int h        = gdk_pixbuf_get_height(rgba);
            int srcStride = gdk_pixbuf_get_rowstride(rgba);
            guchar *srcData = gdk_pixbuf_get_pixels(rgba);

            surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
            cairo_surface_flush(surf);
            unsigned char *dst    = cairo_image_surface_get_data(surf);
            int            dstStride = cairo_image_surface_get_stride(surf);

            // GdkPixbuf: RGBA, non-premultiplied
            // Cairo ARGB32: BGRA, premultiplied, native-endian
            for (int row = 0; row < h; ++row) {
                guchar       *s = srcData + row * srcStride;
                unsigned char *d = dst   + row * dstStride;
                for (int col = 0; col < w; ++col) {
                    guchar r = s[0], g = s[1], b = s[2], a = s[3];
                    // premultiply
                    if (a != 255) {
                        r = static_cast<guchar>((r * a + 127) / 255);
                        g = static_cast<guchar>((g * a + 127) / 255);
                        b = static_cast<guchar>((b * a + 127) / 255);
                    }
                    d[0] = b; d[1] = g; d[2] = r; d[3] = a;
                    s += 4; d += 4;
                }
            }
            cairo_surface_mark_dirty(surf);
            g_object_unref(rgba);
        } else {
            if (err) g_error_free(err);
            // Fallback: cairo native PNG loader
            surf = cairo_image_surface_create_from_png(path.c_str());
            if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
                cairo_surface_destroy(surf);
                surf = nullptr;
            }
        }
    } else if (ext == ".svg") {
        // Rasterise SVG: gdk-pixbuf loads SVG if librsvg loader plugin is installed.
        GError *err = nullptr;
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(path.c_str(), &err);
        if (pb) {
            // Convert with same manual RGBA→ARGB32 path as PNG above
            GdkPixbuf *rgba = pb;
            if (!gdk_pixbuf_get_has_alpha(pb)) {
                rgba = gdk_pixbuf_add_alpha(pb, FALSE, 0, 0, 0);
                g_object_unref(pb);
            }
            int w = gdk_pixbuf_get_width(rgba);
            int h = gdk_pixbuf_get_height(rgba);
            int srcStride = gdk_pixbuf_get_rowstride(rgba);
            guchar *srcData = gdk_pixbuf_get_pixels(rgba);

            surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
            cairo_surface_flush(surf);
            unsigned char *dst      = cairo_image_surface_get_data(surf);
            int            dstStride = cairo_image_surface_get_stride(surf);
            for (int row = 0; row < h; ++row) {
                guchar        *s = srcData + row * srcStride;
                unsigned char *d = dst    + row * dstStride;
                for (int col = 0; col < w; ++col) {
                    guchar r = s[0], g = s[1], b = s[2], a = s[3];
                    if (a != 255) {
                        r = static_cast<guchar>((r * a + 127) / 255);
                        g = static_cast<guchar>((g * a + 127) / 255);
                        b = static_cast<guchar>((b * a + 127) / 255);
                    }
                    d[0] = b; d[1] = g; d[2] = r; d[3] = a;
                    s += 4; d += 4;
                }
            }
            cairo_surface_mark_dirty(surf);
            g_object_unref(rgba);
        } else {
            if (err) g_error_free(err);
        }
    }

    cache_[filename] = surf;
    return surf;
}

// ═══════════════════════════════════════════════════════════════════════════
// LabRenderCtx
// ═══════════════════════════════════════════════════════════════════════════

LabRenderCtx::LabRenderCtx(const std::string &fontDescStr,
                            const std::filesystem::path &themeDir)
    : images(themeDir)
{
    fontMap = pango_cairo_font_map_new();
    context = pango_font_map_create_context(fontMap);
    desc    = pango_font_description_from_string(fontDescStr.c_str());
    if (desc)
        pango_context_set_font_description(context, desc);
}

LabRenderCtx::~LabRenderCtx() {
    if (desc)    pango_font_description_free(desc);
    if (context) g_object_unref(context);
    if (fontMap) g_object_unref(fontMap);
}

int LabRenderCtx::fontHeightPx() const {
    if (!context || !desc) return 14;
    auto *metrics = pango_context_get_metrics(context, desc,
                        pango_context_get_language(context));
    int h = PANGO_PIXELS(pango_font_metrics_get_ascent(metrics) +
                         pango_font_metrics_get_descent(metrics));
    pango_font_metrics_unref(metrics);
    return h > 0 ? h : 14;
}

// ═══════════════════════════════════════════════════════════════════════════
// cairoSetColor
// ═══════════════════════════════════════════════════════════════════════════

void cairoSetColor(cairo_t *cr, const LabColor &c) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
}

// ═══════════════════════════════════════════════════════════════════════════
// paintTile — exact port of production theme.cpp::paintTile()
// ═══════════════════════════════════════════════════════════════════════════

void paintTile(cairo_t *c, int width, int height, double alpha,
               cairo_surface_t *image,
               int marginLeft, int marginTop, int marginRight, int marginBottom) {

    int resizeHeight = cairo_image_surface_get_height(image) - marginTop - marginBottom;
    int resizeWidth  = cairo_image_surface_get_width(image)  - marginLeft - marginRight;

    if (resizeHeight <= 0) resizeHeight = 1;
    if (resizeWidth  <= 0) resizeWidth  = 1;
    if (height < 0) height = resizeHeight;
    if (width  < 0) width  = resizeWidth;

    const auto targetResizeWidth  = width  - marginLeft - marginRight;
    const auto targetResizeHeight = height - marginTop  - marginBottom;
    const double scaleX = static_cast<double>(targetResizeWidth)  / resizeWidth;
    const double scaleY = static_cast<double>(targetResizeHeight) / resizeHeight;

    /* 7 8 9
     * 4 5 6
     * 1 2 3  */

    if (marginLeft && marginBottom) { /* 1 */
        cairo_save(c);
        cairo_translate(c, 0, height - marginBottom);
        cairo_set_source_surface(c, image, 0, -marginTop - resizeHeight);
        cairo_rectangle(c, 0, 0, marginLeft, marginBottom);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    if (marginRight && marginBottom) { /* 3 */
        cairo_save(c);
        cairo_translate(c, width - marginRight, height - marginBottom);
        cairo_set_source_surface(c, image, -marginLeft - resizeWidth, -marginTop - resizeHeight);
        cairo_rectangle(c, 0, 0, marginRight, marginBottom);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    if (marginLeft && marginTop) { /* 7 */
        cairo_save(c);
        cairo_set_source_surface(c, image, 0, 0);
        cairo_rectangle(c, 0, 0, marginLeft, marginTop);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    if (marginRight && marginTop) { /* 9 */
        cairo_save(c);
        cairo_translate(c, width - marginRight, 0);
        cairo_set_source_surface(c, image, -marginLeft - resizeWidth, 0);
        cairo_rectangle(c, 0, 0, marginRight, marginTop);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    if (marginTop && targetResizeWidth > 0) { /* 8 */
        cairo_save(c);
        cairo_translate(c, marginLeft, 0);
        cairo_scale(c, scaleX, 1);
        cairo_set_source_surface(c, image, -marginLeft, 0);
        cairo_rectangle(c, 0, 0, resizeWidth, marginTop);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    if (marginBottom && targetResizeWidth > 0) { /* 2 */
        cairo_save(c);
        cairo_translate(c, marginLeft, height - marginBottom);
        cairo_scale(c, scaleX, 1);
        cairo_set_source_surface(c, image, -marginLeft, -marginTop - resizeHeight);
        cairo_rectangle(c, 0, 0, resizeWidth, marginBottom);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    if (marginLeft && targetResizeHeight > 0) { /* 4 */
        cairo_save(c);
        cairo_translate(c, 0, marginTop);
        cairo_scale(c, 1, scaleY);
        cairo_set_source_surface(c, image, 0, -marginTop);
        cairo_rectangle(c, 0, 0, marginLeft, resizeHeight);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    if (marginRight && targetResizeHeight > 0) { /* 6 */
        cairo_save(c);
        cairo_translate(c, width - marginRight, marginTop);
        cairo_scale(c, 1, scaleY);
        cairo_set_source_surface(c, image, -marginLeft - resizeWidth, -marginTop);
        cairo_rectangle(c, 0, 0, marginRight, resizeHeight);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    if (targetResizeHeight > 0 && targetResizeWidth > 0) { /* 5 */
        cairo_save(c);
        cairo_translate(c, marginLeft, marginTop);
        cairo_scale(c, scaleX, scaleY);
        cairo_set_source_surface(c, image, -marginLeft, -marginTop);
        cairo_pattern_set_filter(cairo_get_source(c), CAIRO_FILTER_NEAREST);
        cairo_rectangle(c, 0, 0, resizeWidth, resizeHeight);
        cairo_clip(c); cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// paintBackground — solid colour fallback (used when no image is set)
// ═══════════════════════════════════════════════════════════════════════════

void paintBackground(cairo_t *cr, int width, int height,
                     const LabColor &fillColor,
                     const LabColor &borderColor,
                     int borderWidth,
                     double borderRadius) {
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    auto roundedRect = [&](double x, double y, double w, double h, double r) {
        if (r <= 0.0) { cairo_rectangle(cr, x, y, w, h); return; }
        r = std::min(r, std::min(w, h) / 2.0);
        cairo_new_sub_path(cr);
        cairo_arc(cr, x + w - r, y + r,     r, -M_PI_2, 0);
        cairo_arc(cr, x + w - r, y + h - r, r, 0,       M_PI_2);
        cairo_arc(cr, x + r,     y + h - r, r, M_PI_2,  M_PI);
        cairo_arc(cr, x + r,     y + r,     r, M_PI,    -M_PI_2);
        cairo_close_path(cr);
    };

    if (borderWidth > 0) {
        cairoSetColor(cr, borderColor);
        roundedRect(0, 0, width, height, borderRadius);
        cairo_fill(cr);
    }
    cairoSetColor(cr, fillColor);
    roundedRect(borderWidth, borderWidth,
                width  - 2 * borderWidth,
                height - 2 * borderWidth,
                std::max(0.0, borderRadius - borderWidth));
    cairo_fill(cr);
    cairo_restore(cr);
}

// ═══════════════════════════════════════════════════════════════════════════
// renderText
// ═══════════════════════════════════════════════════════════════════════════

void renderText(cairo_t *cr, PangoContext *ctx, PangoFontDescription *fontDesc,
                const std::string &text, int x, int y, const LabColor &color) {
    if (text.empty()) return;
    cairo_save(cr);
    cairoSetColor(cr, color);

    auto *layout = pango_layout_new(ctx);
    pango_layout_set_font_description(layout, fontDesc);
    pango_layout_set_text(layout, text.c_str(), static_cast<int>(text.size()));
    pango_layout_set_single_paragraph_mode(layout, true);

    auto *metrics = pango_context_get_metrics(ctx, fontDesc,
                        pango_context_get_language(ctx));
    auto ascent   = pango_font_metrics_get_ascent(metrics);
    pango_font_metrics_unref(metrics);
    auto baseline = pango_layout_get_baseline(layout);
    int yOffset   = PANGO_PIXELS(ascent - baseline);

    cairo_move_to(cr, x, y + yOffset);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    cairo_restore(cr);
}

// ═══════════════════════════════════════════════════════════════════════════
// findActiveThemeConf
// ═══════════════════════════════════════════════════════════════════════════

std::filesystem::path findActiveThemeConf() {
    // 1. Look for smarttype-liquid-glass (primary SmartType theme)
    const char *home = std::getenv("HOME");
    std::vector<std::filesystem::path> searchDirs;
    if (home) {
        searchDirs.push_back(
            std::filesystem::path(home) / ".local/share/fcitx5/themes");
    }
    searchDirs.push_back("/usr/share/fcitx5/themes");

    // Theme names to try, in priority order
    const std::vector<std::string> themeNames = {
        "smarttype-liquid-glass",
        "default-dark",
        "default",
    };

    for (const auto &dir : searchDirs) {
        for (const auto &name : themeNames) {
            auto conf = dir / name / "theme.conf";
            if (std::filesystem::exists(conf))
                return conf;
        }
    }
    return {};
}

// ═══════════════════════════════════════════════════════════════════════════
// renderPanel
// ═══════════════════════════════════════════════════════════════════════════

void renderPanel(cairo_t *cr,
                 unsigned int width,
                 unsigned int height,
                 const LabTheme &theme,
                 LabRenderCtx  &rctx,
                 const std::vector<MockCandidate> &candidates,
                 int highlightIdx,
                 double pillProgress) {

    // 1. Clear
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    const auto &ip  = theme.inputPanel;
    const auto &bg  = ip.background;
    const auto &hl  = ip.highlight;
    const int iW = static_cast<int>(width);
    const int iH = static_cast<int>(height);

    // Clip geometry (production constants from inputwindow.cpp)
    const double clipInsetX = LabAnimConstants::clipInsetX;
    const double clipInsetY = LabAnimConstants::clipInsetY;
    const double clipR      = LabAnimConstants::clipRadiusPx;
    const double panelX = clipInsetX;
    const double panelY = clipInsetY;
    const int    panelW = static_cast<int>(std::ceil(static_cast<double>(iW) - 2.0 * clipInsetX));
    const int    panelH = static_cast<int>(std::ceil(static_cast<double>(iH) - 2.0 * clipInsetY));

    // ── 2. Panel background ──────────────────────────────────────────────
    {
        cairo_save(cr);
        cairo_translate(cr, panelX, panelY);

        cairo_surface_t *bgImg = bg.image.empty() ? nullptr
                                                   : rctx.images.get(bg.image);
        if (bgImg) {
            // 9-slice PNG background (production path)
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            paintTile(cr, panelW, panelH, 1.0, bgImg,
                      bg.margin.left, bg.margin.top,
                      bg.margin.right, bg.margin.bottom);
        } else {
            // Solid colour fallback
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            paintBackground(cr, panelW, panelH,
                            theme.inputPanelBackground_,
                            theme.inputPanelBorder_,
                            bg.borderWidth,
                            clipR);
        }
        cairo_restore(cr);
    }

    if (candidates.empty()) return;

    // ── 3. Layout candidates ─────────────────────────────────────────────
    const int contentL = static_cast<int>(panelX) + bg.margin.left  + ip.contentMargin.left;
    const int contentT = static_cast<int>(panelY) + bg.margin.top   + ip.contentMargin.top;
    const int textL    = ip.textMargin.left;
    const int textT    = ip.textMargin.top;
    const int textR    = ip.textMargin.right;
    const int textB    = ip.textMargin.bottom;

    const int fontH = rctx.fontHeightPx();

    // Label font with size factor
    PangoFontDescription *labelDesc = pango_font_description_copy(rctx.desc);
    {
        int origSz = pango_font_description_get_size(rctx.desc);
        if (origSz > 0) {
            int labelSz = static_cast<int>(origSz * ip.labelTextSizeFactor / 100.0);
            pango_font_description_set_size(labelDesc, labelSz);
        }
    }

    struct CandBox {
        int labelW, textW, commentW;
        int x, width;
    };

    int totalW = 0;
    std::vector<CandBox> boxes;
    boxes.reserve(candidates.size());

    auto measureText = [&](const std::string &t, PangoFontDescription *fd) -> int {
        if (t.empty()) return 0;
        auto *lay = pango_layout_new(rctx.context);
        pango_layout_set_font_description(lay, fd);
        pango_layout_set_text(lay, t.c_str(), -1);
        pango_layout_set_single_paragraph_mode(lay, true);
        int w, h; pango_layout_get_pixel_size(lay, &w, &h);
        g_object_unref(lay);
        return w;
    };

    for (const auto &c : candidates) {
        CandBox box{};
        box.labelW   = measureText(c.label,   labelDesc);
        box.textW    = measureText(c.text,     rctx.desc);
        box.commentW = measureText(c.comment,  labelDesc);
        box.width    = box.labelW + box.textW + box.commentW + textL + textR;
        box.x        = contentL + totalW;
        totalW      += box.width;
        boxes.push_back(box);
    }

    // ── 4. Draw highlight pill (under text) ──────────────────────────────
    if (highlightIdx >= 0 && highlightIdx < static_cast<int>(boxes.size())) {
        const auto &hb = boxes[static_cast<size_t>(highlightIdx)];

        int pillX  = hb.x - hl.margin.left;
        int pillY  = contentT - hl.margin.top;
        int pillW  = hb.width + hl.margin.left + hl.margin.right;
        int pillH  = fontH + textT + textB + hl.margin.top + hl.margin.bottom;

        // Animate X from far-left to target
        int startX = contentL - hl.margin.left;
        // easeOutCubic
        double t   = pillProgress;
        double ease = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
        int animX  = startX + static_cast<int>((pillX - startX) * ease);

        cairo_save(cr);

        // Clip pill to panel rounded rect (production geometry)
        {
            double cx = panelX + hl.clickMargin.left;
            double cy = panelY + hl.clickMargin.top;
            double cw = static_cast<double>(panelW) - hl.clickMargin.left - hl.clickMargin.right;
            double ch = static_cast<double>(panelH) - hl.clickMargin.top  - hl.clickMargin.bottom;
            double r  = clipR;
            cairo_new_sub_path(cr);
            cairo_arc(cr, cx + cw - r, cy + r,     r, -M_PI_2, 0);
            cairo_arc(cr, cx + cw - r, cy + ch - r, r, 0,       M_PI_2);
            cairo_arc(cr, cx + r,      cy + ch - r, r, M_PI_2,  M_PI);
            cairo_arc(cr, cx + r,      cy + r,      r, M_PI,    -M_PI_2);
            cairo_close_path(cr);
            cairo_clip(cr);
        }

        cairo_translate(cr, animX, pillY);

        cairo_surface_t *hlImg = hl.image.empty() ? nullptr
                                                   : rctx.images.get(hl.image);
        if (hlImg) {
            paintTile(cr, pillW, pillH, pillProgress, hlImg,
                      hl.margin.left, hl.margin.top,
                      hl.margin.right, hl.margin.bottom);
        } else {
            // Solid pill fallback
            double r = std::min(static_cast<double>(pillW),
                                static_cast<double>(pillH)) * 0.35;
            paintBackground(cr, pillW, pillH,
                            theme.inputPanelHighlightCandBg_,
                            theme.inputPanelHighlightCandBorder_,
                            hl.borderWidth, r);
        }
        cairo_restore(cr);
    }

    // ── 5. Draw text ─────────────────────────────────────────────────────
    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto &c  = candidates[i];
        const auto &bx = boxes[i];
        bool isHL      = (static_cast<int>(i) == highlightIdx);

        int baseY = contentT + textT;
        int curX  = bx.x + textL;

        const LabColor &labelColor = isHL
            ? theme.inputPanelHighlightCandLabelText_
            : theme.inputPanelCandLabelText_;
        const LabColor &textColor  = isHL
            ? theme.inputPanelHighlightCandText_
            : theme.inputPanelText_;
        const LabColor &cmtColor   = isHL
            ? theme.inputPanelHighlightCandCommentText_
            : theme.inputPanelCandCommentText_;

        if (!c.label.empty()) {
            renderText(cr, rctx.context, labelDesc, c.label, curX, baseY, labelColor);
            curX += bx.labelW;
        }
        if (!c.text.empty()) {
            renderText(cr, rctx.context, rctx.desc,  c.text,  curX, baseY, textColor);
            curX += bx.textW;
        }
        if (!c.comment.empty()) {
            renderText(cr, rctx.context, labelDesc, c.comment, curX, baseY, cmtColor);
        }
    }

    pango_font_description_free(labelDesc);
}

} // namespace smarttype::themelab
