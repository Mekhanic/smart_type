/*
 * SPDX-FileCopyrightText: 2026 SmartType Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * labtheme.cpp — INI load/save and colour resolution for LabTheme.
 *
 * Uses a minimal hand-written INI parser so we have zero dependency on
 * fcitx-config. The format matches the theme.conf written by production
 * Theme::load() — sections like [InputPanel], [InputPanel/Background] etc.
 */
#include "labtheme.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace smarttype::themelab {

// ── LabColor ────────────────────────────────────────────────────────────────

static unsigned hexByte(char hi, char lo) {
    auto hexNibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a') + 10u;
        if (c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A') + 10u;
        return 0u;
    };
    return (hexNibble(hi) << 4u) | hexNibble(lo);
}

bool LabColor::parseHex(const std::string &hex) {
    const char *s = hex.c_str();
    if (*s == '#') ++s;
    size_t len = std::strlen(s);
    if (len == 6) {
        r = hexByte(s[0], s[1]) / 255.0;
        g = hexByte(s[2], s[3]) / 255.0;
        b = hexByte(s[4], s[5]) / 255.0;
        a = 1.0;
        return true;
    }
    if (len == 8) {
        r = hexByte(s[0], s[1]) / 255.0;
        g = hexByte(s[2], s[3]) / 255.0;
        b = hexByte(s[4], s[5]) / 255.0;
        a = hexByte(s[6], s[7]) / 255.0;
        return true;
    }
    return false;
}

std::string LabColor::toHex() const {
    char buf[10];
    auto clamp = [](double v) { return static_cast<int>(std::clamp(v, 0.0, 1.0) * 255.0 + 0.5); };
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x",
                  clamp(r), clamp(g), clamp(b), clamp(a));
    return buf;
}

// ── Minimal INI parser ──────────────────────────────────────────────────────

// Flat representation: key = "Section/SubSection/Key", value = string.
using IniMap = std::map<std::string, std::string>;

static std::string trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return std::string(s.substr(start, end - start));
}

static IniMap parseIni(std::istream &in) {
    IniMap result;
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        auto t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;
        if (t[0] == '[') {
            size_t end = t.find(']');
            if (end != std::string::npos)
                section = t.substr(1, end - 1);
            continue;
        }
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));
        std::string fullKey = section.empty() ? key : section + "/" + key;
        result[fullKey] = val;
    }
    return result;
}

// ── Helpers to extract typed values from IniMap ─────────────────────────────

static std::string iniGet(const IniMap &m, const std::string &key, const std::string &def = "") {
    auto it = m.find(key);
    return it != m.end() ? it->second : def;
}

static LabColor iniColor(const IniMap &m, const std::string &key, const LabColor &def) {
    auto s = iniGet(m, key);
    if (s.empty()) return def;
    LabColor c;
    if (c.parseHex(s)) return c;
    return def;
}

static std::optional<LabColor> iniOptColor(const IniMap &m, const std::string &key) {
    auto s = iniGet(m, key);
    if (s.empty()) return std::nullopt;
    LabColor c;
    if (c.parseHex(s)) return c;
    return std::nullopt;
}

static int iniInt(const IniMap &m, const std::string &key, int def) {
    auto s = iniGet(m, key);
    if (s.empty()) return def;
    int v = def;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

static bool iniBool(const IniMap &m, const std::string &key, bool def) {
    auto s = iniGet(m, key);
    if (s.empty()) return def;
    return s == "True" || s == "true" || s == "1";
}

static LabMargin iniMargin(const IniMap &m, const std::string &prefix) {
    return LabMargin {
        iniInt(m, prefix + "/Left",   0),
        iniInt(m, prefix + "/Right",  0),
        iniInt(m, prefix + "/Top",    0),
        iniInt(m, prefix + "/Bottom", 0)
    };
}

static LabBackgroundConfig iniBackground(const IniMap &m, const std::string &prefix) {
    LabBackgroundConfig cfg;
    cfg.image       = iniGet(m, prefix + "/Image");
    cfg.color       = iniColor(m, prefix + "/Color",       LabColor(1,1,1,1));
    cfg.borderColor = iniColor(m, prefix + "/BorderColor", LabColor(1,1,1,0));
    cfg.borderWidth = iniInt(m, prefix + "/BorderWidth", 0);
    cfg.margin      = iniMargin(m, prefix + "/Margin");
    cfg.overlayClipMargin = iniMargin(m, prefix + "/OverlayClipMargin");
    cfg.overlay     = iniGet(m, prefix + "/Overlay");
    return cfg;
}

static LabHighlightConfig iniHighlight(const IniMap &m, const std::string &prefix) {
    LabHighlightConfig cfg;
    static_cast<LabBackgroundConfig &>(cfg) = iniBackground(m, prefix);
    cfg.clickMargin = iniMargin(m, prefix + "/HighlightClickMargin");
    return cfg;
}

static LabActionConfig iniAction(const IniMap &m, const std::string &prefix) {
    LabActionConfig cfg;
    cfg.image       = iniGet(m, prefix + "/Image");
    cfg.clickMargin = iniMargin(m, prefix + "/ClickMargin");
    return cfg;
}

// ── LabTheme::loadFromFile ──────────────────────────────────────────────────

bool LabTheme::loadFromFile(const std::filesystem::path &confPath) {
    std::ifstream in(confPath);
    if (!in) return false;

    // Remember directory so renderer can find PNG/SVG assets.
    themeDir = confPath.parent_path();

    IniMap m = parseIni(in);

    name        = iniGet(m, "Metadata/Name",        "custom");
    author      = iniGet(m, "Metadata/Author",       "");
    description = iniGet(m, "Metadata/Description",  "");


    auto &ip = inputPanel;
    ip.normalColor        = iniColor(m, "InputPanel/NormalColor",             LabColor(0,0,0,1));
    ip.highlightColor     = iniColor(m, "InputPanel/HighlightColor",          LabColor(1,1,1,1));
    ip.highlightBgColor   = iniColor(m, "InputPanel/HighlightBackgroundColor",LabColor(0.647,0.647,0.647,1));
    ip.highlightCandColor = iniColor(m, "InputPanel/HighlightCandidateColor", LabColor(1,1,1,1));

    ip.candidateLabelColor          = iniOptColor(m, "InputPanel/CandidateLabelColor");
    ip.highlightCandLabelColor      = iniOptColor(m, "InputPanel/HighlightCandidateLabelColor");
    ip.candidateCommentColor        = iniOptColor(m, "InputPanel/CandidateCommentColor");
    ip.highlightCandCommentColor    = iniOptColor(m, "InputPanel/HighlightCandidateCommentColor");

    ip.labelTextSizeFactor   = iniInt(m,  "InputPanel/LabelTextSizeFactor",   100);
    ip.commentTextSizeFactor = iniInt(m,  "InputPanel/CommentTextSizeFactor",  75);
    ip.fullWidthHighlight    = iniBool(m, "InputPanel/FullWidthHighlight",    true);

    ip.contentMargin = iniMargin(m, "InputPanel/ContentMargin");
    ip.textMargin    = iniMargin(m, "InputPanel/TextMargin");
    ip.shadowMargin  = iniMargin(m, "InputPanel/ShadowMargin");

    ip.background = iniBackground(m, "InputPanel/Background");
    ip.highlight  = iniHighlight(m,  "InputPanel/Highlight");

    ip.prev = iniAction(m, "InputPanel/PrevPage");
    ip.next = iniAction(m, "InputPanel/NextPage");

    populateColors();
    return true;
}

// ── LabTheme::saveToFile ────────────────────────────────────────────────────

static void writeColor(std::ostream &out, const std::string &key, const LabColor &c) {
    out << key << "=" << c.toHex() << "\n";
}

static void writeMargin(std::ostream &out, const std::string &section, const LabMargin &m) {
    out << "\n[" << section << "]\n";
    out << "Left="   << m.left   << "\n";
    out << "Right="  << m.right  << "\n";
    out << "Top="    << m.top    << "\n";
    out << "Bottom=" << m.bottom << "\n";
}

static void writeBackground(std::ostream &out, const std::string &prefix,
                             const LabBackgroundConfig &cfg) {
    out << "\n[" << prefix << "]\n";
    if (!cfg.image.empty())
        out << "Image=" << cfg.image << "\n";
    writeColor(out, "Color",       cfg.color);
    writeColor(out, "BorderColor", cfg.borderColor);
    out << "BorderWidth=" << cfg.borderWidth << "\n";
    if (!cfg.overlay.empty())
        out << "Overlay=" << cfg.overlay << "\n";
    writeMargin(out, prefix + "/Margin", cfg.margin);
}

bool LabTheme::saveToFile(const std::filesystem::path &confPath) const {
    std::ofstream out(confPath);
    if (!out) return false;

    out << "[Metadata]\n";
    out << "Name="        << name        << "\n";
    out << "Version=1\n";
    out << "Author="      << author      << "\n";
    out << "Description=" << description << "\n";

    const auto &ip = inputPanel;

    out << "\n[InputPanel]\n";
    writeColor(out, "NormalColor",                ip.normalColor);
    writeColor(out, "HighlightColor",             ip.highlightColor);
    writeColor(out, "HighlightBackgroundColor",   ip.highlightBgColor);
    writeColor(out, "HighlightCandidateColor",    ip.highlightCandColor);
    if (ip.candidateLabelColor)
        writeColor(out, "CandidateLabelColor",    *ip.candidateLabelColor);
    if (ip.highlightCandLabelColor)
        writeColor(out, "HighlightCandidateLabelColor", *ip.highlightCandLabelColor);
    if (ip.candidateCommentColor)
        writeColor(out, "CandidateCommentColor",  *ip.candidateCommentColor);
    if (ip.highlightCandCommentColor)
        writeColor(out, "HighlightCandidateCommentColor", *ip.highlightCandCommentColor);
    out << "LabelTextSizeFactor="   << ip.labelTextSizeFactor   << "\n";
    out << "CommentTextSizeFactor=" << ip.commentTextSizeFactor << "\n";
    out << "FullWidthHighlight="    << (ip.fullWidthHighlight ? "True" : "False") << "\n";

    writeMargin(out, "InputPanel/ContentMargin", ip.contentMargin);
    writeMargin(out, "InputPanel/TextMargin",    ip.textMargin);
    writeBackground(out, "InputPanel/Background", ip.background);

    out << "\n[InputPanel/Highlight]\n";
    writeColor(out, "Color",       ip.highlight.color);
    writeColor(out, "BorderColor", ip.highlight.borderColor);
    out << "BorderWidth=" << ip.highlight.borderWidth << "\n";
    writeMargin(out, "InputPanel/Highlight/Margin",      ip.highlight.margin);
    writeMargin(out, "InputPanel/Highlight/HighlightClickMargin", ip.highlight.clickMargin);

    if (!ip.prev.image.empty()) {
        out << "\n[InputPanel/PrevPage]\n";
        out << "Image=" << ip.prev.image << "\n";
        writeMargin(out, "InputPanel/PrevPage/ClickMargin", ip.prev.clickMargin);
    }
    if (!ip.next.image.empty()) {
        out << "\n[InputPanel/NextPage]\n";
        out << "Image=" << ip.next.image << "\n";
        writeMargin(out, "InputPanel/NextPage/ClickMargin", ip.next.clickMargin);
    }

    out.flush();
    return out.good();
}

// ── LabTheme::populateColors ────────────────────────────────────────────────

void LabTheme::populateColors() {
    const auto &ip = inputPanel;

    inputPanelBackground_         = ip.background.color;
    inputPanelBorder_             = ip.background.borderColor;
    inputPanelHighlightCandBg_    = ip.highlight.color;
    inputPanelHighlightCandBorder_= ip.highlight.borderColor;
    inputPanelText_               = ip.normalColor;
    inputPanelHighlightText_      = ip.highlightColor;
    inputPanelHighlightCandText_  = ip.highlightCandColor;

    // Label colors (fall back to normal/highlight)
    inputPanelCandLabelText_          = ip.candidateLabelColor.value_or(ip.normalColor);
    inputPanelHighlightCandLabelText_ = ip.highlightCandLabelColor.value_or(ip.highlightCandColor);

    // Comment colors (fall back to normal/highlight at 60% alpha)
    if (ip.candidateCommentColor) {
        inputPanelCandCommentText_ = *ip.candidateCommentColor;
    } else {
        inputPanelCandCommentText_ = ip.normalColor;
        inputPanelCandCommentText_.a *= 0.6;
    }
    if (ip.highlightCandCommentColor) {
        inputPanelHighlightCandCommentText_ = *ip.highlightCandCommentColor;
    } else {
        inputPanelHighlightCandCommentText_ = ip.highlightCandColor;
        inputPanelHighlightCandCommentText_.a *= 0.6;
    }
}

// ── LabTheme::resetToDefaults ───────────────────────────────────────────────

void LabTheme::resetToDefaults() {
    *this = LabTheme{};
    // Default theme mirrors themes/default/theme.conf.in
    name = "custom";
    font = "Sans 10";

    auto &ip = inputPanel;
    ip.normalColor        = LabColor(0,   0,   0,   1);
    ip.highlightColor     = LabColor(1,   1,   1,   1);
    ip.highlightCandColor = LabColor(1,   1,   1,   1);
    ip.highlightBgColor   = LabColor(0.502, 0.502, 0.502, 1);  // #808080

    ip.contentMargin = LabMargin{ 2, 2, 2, 2 };
    ip.textMargin    = LabMargin{ 5, 5, 5, 5 };

    ip.background.color       = LabColor(1,   1,   1,   1);    // #ffffff
    ip.background.borderColor = LabColor(0.753, 0.753, 0.753, 1); // #c0c0c0
    ip.background.borderWidth = 2;
    ip.background.margin      = LabMargin{ 2, 2, 2, 2 };

    ip.highlight.color  = LabColor(0.502, 0.502, 0.502, 1);    // #808080
    ip.highlight.margin = LabMargin{ 5, 5, 5, 5 };

    populateColors();
}

} // namespace smarttype::themelab
