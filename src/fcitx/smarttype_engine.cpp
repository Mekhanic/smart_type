#include "smarttype/corrector.hpp"
#include "smarttype/decision_log.hpp"
#include "smarttype/fcitx_safety.hpp"
#include "smarttype/personal_store.hpp"
#include "smarttype/text.hpp"
#include "smarttype/version.hpp"
#include <thread>
#include <chrono>
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>

#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/trackableobject.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonfactory.h>
#include "../ui/smarttypeui/classicui_public.h"
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <fcitx/candidatelist.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iomanip>
#include <iostream>

extern char** environ;

namespace {

class SmartTypeEngine;
class SmartTypeState;

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&timer);
    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string json_escape(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (c >= 0x20) result.push_back(static_cast<char>(c));
        }
    }
    return result;
}

std::vector<uint32_t> utf8_to_utf32_local(std::string_view input) {
    std::vector<uint32_t> result;
    for (std::size_t i = 0; i < input.size(); ) {
        unsigned char c = input[i];
        if (c < 0x80) {
            result.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < input.size()) {
                uint32_t cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(input[i + 1]) & 0x3F);
                result.push_back(cp);
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < input.size()) {
                uint32_t cp = ((c & 0x0F) << 12) |
                              ((static_cast<unsigned char>(input[i + 1]) & 0x3F) << 6) |
                              (static_cast<unsigned char>(input[i + 2]) & 0x3F);
                result.push_back(cp);
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < input.size()) {
                uint32_t cp = ((c & 0x07) << 18) |
                              ((static_cast<unsigned char>(input[i + 1]) & 0x3F) << 12) |
                              ((static_cast<unsigned char>(input[i + 2]) & 0x3F) << 6) |
                              (static_cast<unsigned char>(input[i + 3]) & 0x3F);
                result.push_back(cp);
            }
            i += 4;
        } else {
            i += 1;
        }
    }
    return result;
}

class SmartTypeUiClient final {
public:
    explicit SmartTypeUiClient(SmartTypeEngine* engine);
    ~SmartTypeUiClient();

    void update(fcitx::InputContext* input_context, std::string_view composing,
                const std::vector<std::string>& candidates_list, int selected_index);
    void flash(fcitx::InputContext* input_context, std::string_view correction);
    void hide(bool force = false);
    void select(int index);
    void disconnect();

private:
    std::string socket_path() const;
    void spawn_renderer();
    bool ensure_connected();
    void send(std::string_view data);

    SmartTypeEngine* engine_;
    fcitx::Instance* instance_{nullptr};
    int fd_{-1};
    bool spawn_attempted_{false};
    std::string read_buffer_;
    std::unique_ptr<fcitx::EventSourceIO> io_event_;
    fcitx::TrackableObjectReference<fcitx::InputContext> active_context_;
};

class SmartTypeCandidateWord final : public fcitx::CandidateWord {
public:
    SmartTypeCandidateWord(std::string word, bool /*selected*/, std::function<void()> on_select)
        : fcitx::CandidateWord(fcitx::Text(word, fcitx::TextFormatFlag::NoFlag)),
          on_select_(std::move(on_select)) {}
    void select(fcitx::InputContext*) const override {
        on_select_();
    }
private:
    std::function<void()> on_select_;
};


bool is_attaching_punctuation(uint32_t value) {
    switch (value) {
    case '.': case ',': case '!': case '?':
        return true;
    default:
        return false;
    }
}

bool is_sentence_terminal(uint32_t value) {
    return value == '.' || value == '?' || value == '!';
}

bool is_opening_delimiter(uint32_t value) {
    return value == '(' || value == '[' || value == '{' || value == '"' ||
           value == 0x00AB || value == 0x2018 || value == 0x201C;
}

bool is_smart_typography_prefix(uint32_t value) {
    return value == '-' || value == '<' || value == '>';
}

std::string smart_typography_replacement(uint32_t value) {
    switch (value) {
    case '-': return "—";
    case '<': return "«";
    case '>': return "»";
    default: return {};
    }
}

bool is_letter(uint32_t value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= 0x0410 && value <= 0x044F) || value == 0x0401 || value == 0x0451;
}

std::optional<uint32_t> russian_symbol_for_physical_key(
    int code, fcitx::KeyStates states) {
    const bool shift = states.test(fcitx::KeyState::Shift);
    const bool caps = states.test(fcitx::KeyState::CapsLock);
    const bool uppercase = shift != caps;
    switch (code) {
    // XKB keycodes observed on the supported GTK/DBus and X11 frontends.
    case 34: return uppercase ? 0x0425 : 0x0445;  // AD11: х / Х
    case 35: return uppercase ? 0x042A : 0x044A;  // AD12: ъ / Ъ
    case 47: return uppercase ? 0x0416 : 0x0436;  // AC10: ж / Ж
    case 48: return uppercase ? 0x042D : 0x044D;  // AC11: э / Э
    case 49: return uppercase ? 0x0401 : 0x0451;  // TLDE: ё / Ё
    case 59: return uppercase ? 0x0411 : 0x0431;  // AB08: б / Б
    case 60: return uppercase ? 0x042E : 0x044E;  // AB09: ю / Ю
    case 61: return shift ? ',' : '.';             // AB10: , / .
    default: return std::nullopt;
    }
}

uint32_t normalize_layout_unicode(uint32_t unicode, int physical_code,
                                  fcitx::KeyStates states, bool want_en) {
    const bool is_cyrillic =
        (unicode >= 0x0410 && unicode <= 0x044F) ||
        unicode == 0x0401 || unicode == 0x0451;
    const bool is_latin =
        (unicode >= 'A' && unicode <= 'Z') ||
        (unicode >= 'a' && unicode <= 'z');
    if (!want_en) {
        if (const auto physical = russian_symbol_for_physical_key(physical_code, states)) {
            return *physical;
        }
        if (is_latin) {
            const std::string mapped =
                smarttype::translate_layout(fcitx::utf8::UCS4ToUTF8(unicode));
            if (!mapped.empty() && fcitx::utf8::lengthValidated(mapped) == 1) {
                return fcitx::utf8::getChar(mapped);
            }
        }
    } else if (is_cyrillic) {
        const std::string mapped =
            smarttype::translate_layout(fcitx::utf8::UCS4ToUTF8(unicode));
        if (!mapped.empty() && fcitx::utf8::lengthValidated(mapped) == 1) {
            return fcitx::utf8::getChar(mapped);
        }
    }
    return unicode;
}

uint32_t uppercase_letter(uint32_t value) {
    if (value >= 'a' && value <= 'z') return value - ('a' - 'A');
    if (value >= 0x0430 && value <= 0x044F) return value - 0x20;
    if (value == 0x0451) return 0x0401;
    return value;
}

bool is_ascii_word(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}

bool is_single_cyrillic_letter(std::string_view value) {
    if (fcitx::utf8::lengthValidated(value) != 1) return false;
    const auto cp = fcitx::utf8::getChar(value);
    return (cp >= 0x0410 && cp <= 0x044F) || cp == 0x0401 || cp == 0x0451;
}

bool has_number_before_space(fcitx::InputContext* input_context) {
    if (!input_context->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText)) return false;
    const auto surrounding = input_context->surroundingText();
    if (!surrounding.isValid()) return false;
    const auto& text = surrounding.text();
    std::size_t cursor = std::min<std::size_t>(surrounding.cursor(), text.size());
    if (cursor == 0 || text[cursor - 1] != ' ') return false;
    while (cursor > 0 && text[cursor - 1] == ' ') --cursor;
    return cursor > 0 && text[cursor - 1] >= '0' && text[cursor - 1] <= '9';
}

bool is_test_environment() {
    static bool is_test = []() {
        const char* val = std::getenv("SMARTTYPE_INTEGRATION_TEST");
        return val && std::string_view(val) == "1";
    }();
    return is_test;
}

bool environment_enabled(const char* name) {
    const char* value = std::getenv(name);
    return value && (std::string_view(value) == "1" || std::string_view(value) == "true");
}

bool is_code_program(std::string_view program) {
    constexpr std::string_view names[] = {
        "code", "sublime", "notepad", "vim", "emacs", "nano", "clion", "idea", "studio"
    };
    return std::any_of(std::begin(names), std::end(names), [program](std::string_view name) {
        return program == name || program.find("." + std::string(name)) != std::string_view::npos;
    });
}

bool use_external_ui(const smarttype::PersonalStore* store) {
    if (const char* env = std::getenv("SMARTTYPE_EXTERNAL_UI")) {
        return std::string_view(env) == "1" || std::string_view(env) == "true";
    }
    return store ? store->setting_enabled("external_ui", false) : false;
}

bool is_logging_enabled(const smarttype::PersonalStore* store) {
    if (const char* env = std::getenv("SMARTTYPE_DEBUG")) {
        return std::string_view(env) == "1" || std::string_view(env) == "true";
    }
    return store ? store->setting_enabled("diagnostics", false) : false;
}

bool is_terminal_excluded_context(
    fcitx::InputContext* input_context, const smarttype::PersonalStore* store) {
    if (!store || !store->setting_enabled("disable_in_terminals") ||
        environment_enabled("SMARTTYPE_ENABLE_IN_TERMINALS")) {
        return false;
    }
    const auto flags = input_context->capabilityFlags();
    if (flags.test(fcitx::CapabilityFlag::Terminal)) {
        return true;
    }
    std::string program = input_context->program();
    std::transform(program.begin(), program.end(), program.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });

    const char* terminals[] = {
        "konsole", "qterminal", "lxterminal", "kitty", "alacritty", "wezterm", "xterm",
        "gnome-terminal", "gnome-console", "xfce4-terminal", "mate-terminal", "tilix",
        "termite", "terminator", "terminology", "urxvt", "roxterm", "sakura", "foot",
        "kgx", "ptyxis", "yakuake", "guake", "tilda", "cool-retro-term", "ghostty",
        "blackbox", "cosmic-term", "contour", "rio", "tabby", "hyper", "warp", "st"
    };
    for (const char* name : terminals) {
        bool match = false;
        if (std::strcmp(name, "st") == 0) {
            match = (program == "st");
        } else {
            match = (program.find(name) != std::string::npos);
        }
        if (match) {
            return true;
        }
    }

    return false;
}

bool is_disabled_context(fcitx::InputContext* input_context, const smarttype::PersonalStore* store) {
    if (store && !store->setting_enabled("enabled")) return true;
    const auto flags = input_context->capabilityFlags();
    if (flags.test(fcitx::CapabilityFlag::Password) ||
           flags.test(fcitx::CapabilityFlag::Sensitive) ||
           flags.test(fcitx::CapabilityFlag::Disable)) return true;
    if (is_terminal_excluded_context(input_context, store)) {
        return true;
    }

    std::string program = input_context->program();
    std::transform(program.begin(), program.end(), program.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });

    // Raw XIM contexts without client preedit cannot provide a safe editing
    // transaction. In particular, LibreOffice's generic VCL backend feeds
    // fabricated Backspace events back into XIM, so an autocorrection may
    // duplicate or delete arbitrary text. Fail closed: the application still
    // receives ordinary key events, while SmartType waits for a supported
    // GTK/Qt Fcitx frontend.
    if (smarttype::unsafe_raw_xim_context(
            input_context->frontendName(),
            flags.test(fcitx::CapabilityFlag::Preedit))) {
        return true;
    }

    if (program.find("plasmashell") != std::string::npos ||
        program.find("krunner") != std::string::npos) {
        return true;
    }
    
    if (store && store->setting_enabled("disable_in_code") &&
        !environment_enabled("SMARTTYPE_ENABLE_IN_CODE") && is_code_program(program)) {
        return true;
    }
    if (store && store->is_app_blacklisted(program)) {
        return true;
    }
    return false;
}

class SmartTypeState final : public fcitx::InputContextProperty {
public:
    SmartTypeState(SmartTypeEngine* engine, fcitx::InputContext* input_context);

    ~SmartTypeState() override;

    // Keep logical IM aligned with the actual Fcitx entry / IC IM.
    void sync_logical_input_method(std::string_view reason);
    void sync_logical_from_entry(const fcitx::InputMethodEntry& entry, std::string_view reason,
                                 bool from_activate = false);
    void on_activate(const fcitx::InputMethodEntry& entry);

    void key_event(fcitx::KeyEvent& event);
    void reset(bool commit = false, bool force_discard = false);
    void on_cursor_rect_changed();
    void select_and_commit(int index);
    bool undo_external() { return undo_last(); }
    [[nodiscard]] const std::string& composing_text() const { return buffer_; }
    [[nodiscard]] const std::string& active_input_method() const {
        return effective_input_method();
    }

private:
    static bool is_smarttype_im(std::string_view name) {
        return name == "smarttype" || name == "smarttype-us";
    }
    static std::string kde_layout_idx_for_im(std::string_view im) {
        // Matches kxkbrc LayoutList=us,ru and fcitx5-layout-sync TARGETS.
        return (im == "smarttype-us") ? "0" : "1";
    }
    [[nodiscard]] std::string resolve_actual_smarttype_im() const;
    [[nodiscard]] const std::string& effective_input_method() const {
        return pending_layout_input_method_.empty() ? active_input_method_
                                                    : pending_layout_input_method_;
    }
    // Physical switch path. Client-preedit callers defer until commit/clear.
    void apply_programmatic_layout_switch(const std::string& new_im, std::string_view reason);
    void defer_layout_switch(const std::string& new_im, std::string_view reason);
    void flush_deferred_layout_switch(std::string_view reason);
    bool cancel_deferred_layout_switch(std::string_view reason);
    // ST-042: drop Fcitx compact IM toast ("RU"/"EN") after auto layout switch.
    void suppress_layout_label_im_info();
    struct Undo {
        std::string original;
        std::string replacement;
        std::string delimiter;
        std::string previous_word;
        bool layout_switched{false};
        std::string original_im;
        std::string original_layout_idx;

        Undo(std::string orig, std::string repl, std::string delim, std::string prev,
             bool switched = false, std::string orig_im = "", std::string orig_layout = "")
            : original(std::move(orig)), replacement(std::move(repl)), delimiter(std::move(delim)),
              previous_word(std::move(prev)), layout_switched(switched),
              original_im(std::move(orig_im)), original_layout_idx(std::move(orig_layout)) {}
    };

    struct LastCommit {
        std::string original;
        std::string context;
        int committed_length{0};
    };

    struct DelayedCommit {
        std::string original_word;
        std::string corrected_word;
        std::string delimiter;
        std::string previous_word;
        bool layout_switched{false};
        std::string original_im;
        std::string original_layout_idx;
        std::unique_ptr<fcitx::EventSourceTime> timer;
        std::chrono::steady_clock::time_point start_time;

        DelayedCommit(std::string orig, std::string repl, std::string delim, std::string prev,
                      bool switched = false, std::string orig_im = "", std::string orig_layout = "")
            : original_word(std::move(orig)), corrected_word(std::move(repl)), delimiter(std::move(delim)),
              previous_word(std::move(prev)), layout_switched(switched),
              original_im(std::move(orig_im)), original_layout_idx(std::move(orig_layout)),
              start_time(std::chrono::steady_clock::now()) {}
    };

    std::optional<DelayedCommit> delayed_commit_;
    void commit_delayed(bool update = true);

    void update_preedit();
    std::vector<std::string> get_current_suggestions() const;
    void schedule_preedit();
    void finish_word(const std::string& delimiter);
    void remember_context(const std::string& text);
    bool undo_last();
    void confirm_correction();
    void commit_undo();
    bool supports_preedit_formatting() const;
    void clear_client_preedit_before_commit();
    void commit_literal(const std::string& text);
    void commit_candidate(const std::string& candidate_word);

    // Chromium-family clients advertise SurroundingText but often ignore
    // deleteSurroundingText (ST-022/ST-026: "F ns А ты что"). Prefer Backspace.
    [[nodiscard]] bool is_gnome_ibus_proxy() const {
        std::string program = input_context_->program();
        std::transform(program.begin(), program.end(), program.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        const bool compositor_program = program.find("gnome-shell") != std::string::npos;
        return (input_context_->frontendName() == "ibus" && compositor_program) ||
               (is_test_environment() && compositor_program);
    }
    [[nodiscard]] bool surrounding_delete_is_unreliable() const {
        // Mutter's IBus bridge can reject character offsets computed from its
        // asynchronously updated surrounding-text snapshot. There is no delete
        // acknowledgement, so treating that request as successful corrupts the
        // document. Use forwarded Backspaces for single-word undo instead.
        if (is_gnome_ibus_proxy()) {
            return true;
        }
        std::string program = input_context_->program();
        std::transform(program.begin(), program.end(), program.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        static constexpr std::string_view kChromeLike[] = {
            "chrome", "chromium", "msedge", "microsoft-edge", "brave", "vivaldi",
            "opera",  "yandex_browser", "yandex-browser", "electron",
        };
        for (const auto needle : kChromeLike) {
            if (program.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    void erase_committed(std::size_t char_length, [[maybe_unused]] std::size_t byte_length) {
        if (char_length == 0) {
            return;
        }
        if (input_context_->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText) &&
            !surrounding_delete_is_unreliable()) {
            input_context_->deleteSurroundingText(-static_cast<int>(char_length), char_length);
        } else {
            for (std::size_t index = 0; index < char_length; ++index) {
                input_context_->forwardKey(fcitx::Key(FcitxKey_BackSpace));
            }
        }
    }
    // ST-041: drop Latin leftovers left in SurroundingText after preedit rewrite.
    std::size_t erase_leaked_latin_token_before_cursor(std::string_view expected_token);
    [[nodiscard]] bool uses_client_preedit() const {
        std::string program = input_context_->program();
        std::transform(program.begin(), program.end(), program.begin(),
                       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        // Chromium can materialise the current Wayland preedit when a controlled
        // web input is re-rendered, then ask the IME to reset.  Re-publishing our
        // still-complete preedit on the next key produces cumulative text such as
        // "М" + "Ма" + "Маха".  The engine already has a Chromium-safe immediate
        // path: commit each literal key, retain only the logical word in buffer_,
        // and use forwarded Backspaces for corrections at the word boundary.
        static constexpr std::string_view kUnstableClientPreeditPrograms[] = {
            "chrome", "chromium", "msedge", "microsoft-edge", "brave", "vivaldi",
            "opera", "yandex_browser", "yandex-browser", "electron",
        };
        for (const auto needle : kUnstableClientPreeditPrograms) {
            if (program.find(needle) != std::string::npos) {
                return false;
            }
        }
        if (program.find("emojier") != std::string::npos ||
            program.find("copyq") != std::string::npos) {
            return false;
        }
        return input_context_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit);
    }

    SmartTypeEngine* engine_;
    fcitx::InputContext* input_context_;
    std::string buffer_;
    std::deque<std::string> context_;
    std::optional<Undo> undo_;

    // Члены класса для отслеживания ручной коррекции
    std::optional<LastCommit> last_commit_;
    int backspace_count_{0};
    bool tracking_manual_correction_{false};
    int selected_candidate_{0};
    bool pending_auto_space_{false};
    int auto_punctuation_length_{0};
    uint32_t smart_typography_prefix_{0};
    bool last_input_was_digit_{false};
    bool pending_number_space_{false};
    bool awaiting_sentence_start_{true};
    bool protected_sequence_{false};
    bool candidates_dismissed_{false};
    bool candidate_anchor_valid_{false};
    int candidate_anchor_x_{0};
    int candidate_anchor_y_{0};
    std::chrono::steady_clock::time_point last_key_event_time_{};
    std::unique_ptr<fcitx::EventSourceTime> suggestion_timer_;
    std::unique_ptr<fcitx::EventSourceTime> undo_timer_;

    [[nodiscard]] bool is_word_character(uint32_t value) const;
    void check_and_switch_layout(const std::string& original, const std::string& committed);
    // ST-021: rewrite trailing wrong-layout words before cursor (e.g. "F ns " → "А ты ").
    // On success updates context_ and commits the rewritten prefix into the document.
    // Sets undo_original_prefix / undo_replacement_prefix (no trailing space) for undo.
    bool try_rewrite_layout_phrase_prefix(bool to_russian, std::string& undo_original_prefix,
                                         std::string& undo_replacement_prefix);

    mutable std::vector<std::string> suggestions_cache_;
    mutable std::string cache_buffer_;
    mutable std::deque<std::string> cache_context_;
    std::string active_input_method_;
    // A proactive correction may translate client preedit immediately, but the
    // physical Fcitx/KDE switch must wait until that preedit is committed and
    // cleared. Otherwise Qt/Chromium can commit the old-layout preedit on IM
    // deactivation (Kate: ghb + translated remainder -> ghbвет).
    std::string pending_layout_input_method_;
    int64_t last_shift_time_{0};
    bool layout_switched_for_current_word_{false};
    bool programmatic_switch_in_progress_{false};
    uint64_t switch_generation_{0};
    std::unique_ptr<fcitx::EventSourceTime> switch_guard_timer_;
    // Proactive mid-word switch episode (Gboard/Caramba-like):
    // - undo_active: first Backspace restores pre-switch snapshot (not full retranslate of live buffer)
    // - post_buffer: buffer right after switch; further typing clears undo_active so BS deletes one char
    bool layout_switch_undo_active_{false};
    std::string layout_switch_undo_im_;
    std::string layout_switch_undo_layout_idx_;
    std::string layout_switch_undo_buffer_;   // pre-switch text (restore target)
    std::string layout_switch_post_buffer_;   // post-switch text (expire undo when grown)
    void clear_layout_switch_episode();
};

class SmartTypeEngine final : public fcitx::InputMethodEngineV2 {
    static void dladdr_anchor() {}

public:
    explicit SmartTypeEngine(fcitx::Instance* instance)
        : instance_(instance), store_(), ui_client_(this), state_factory_([this](fcitx::InputContext& context) {
              return new SmartTypeState(this, &context);
          }) {
        Dl_info info;
        std::string so_path = "unknown";
        union {
            void (*fn)();
            void* ptr;
        } cast_u;
        cast_u.fn = &SmartTypeEngine::dladdr_anchor;
        if (dladdr(cast_u.ptr, &info) && info.dli_fname) {
            so_path = info.dli_fname;
        }
        std::string ui_path = "smarttype-ui";
        if (const char* home = std::getenv("HOME"); home && *home) {
            const std::string local = std::string(home) + "/.local/bin/smarttype-ui";
            if (access(local.c_str(), X_OK) == 0) ui_path = local;
        }
        std::string theme_path = "unknown";
        if (const char* home = std::getenv("HOME"); home && *home) {
            const std::string local_theme = std::string(home) + "/.local/share/fcitx5/themes/smarttype-liquid-glass";
            if (access(local_theme.c_str(), F_OK) == 0) theme_path = local_theme;
        }
        // instance()->currentUI() returns the name of the addon that Fcitx
        // has selected as the active UI, not just what is loaded.
        const std::string active_ui = instance_->currentUI();

        if (is_logging_enabled(&store_)) {
            std::cerr << "[SmartType Startup] PID: " << getpid() << "\n"
                      << "  Library path: " << so_path << "\n"
                      << "  Version Hash: " << smarttype::GIT_COMMIT_HASH << "\n"
                      << "  Build Time  : " << smarttype::BUILD_TIMESTAMP << "\n"
                      << "  UI Binary   : " << ui_path << "\n"
                      << "  Theme Path  : " << theme_path << "\n"
                      << "  Active Fcitx UI (currentUI): " << (active_ui.empty() ? "(none yet)" : active_ui) << std::endl;
        }

        const auto stored_rules = store_.rules();
        for (auto iterator = stored_rules.rbegin(); iterator != stored_rules.rend(); ++iterator) {
            corrector_.add_rule(*iterator);
        }
        for (const auto& word : store_.all_words()) corrector_.add_personal_word(word);
        instance_->inputContextManager().registerProperty("smarttypeState", &state_factory_);
        cursor_rect_watcher_ = instance_->watchEvent(
            fcitx::EventType::InputContextCursorRectChanged,
            fcitx::EventWatcherPhase::Default,
            [this](fcitx::Event& raw_event) {
                auto& cursor_event = static_cast<fcitx::InputContextEvent&>(raw_event);
                if (auto* state = cursor_event.inputContext()->propertyFor(&state_factory_)) {
                    state->on_cursor_rect_changed();
                }
            });
        last_undo_request_ = store_.string_setting("undo_request_id");
        control_timer_ = instance_->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, fcitx::now(CLOCK_MONOTONIC) + 300000, 1000,
            [this](fcitx::EventSourceTime* source, uint64_t) {
                const auto request = store_.string_setting("undo_request_id");
                if (!request.empty() && request != last_undo_request_) {
                    last_undo_request_ = request;
                    if (auto* context = active_context_.get(); context && context->hasFocus()) {
                        if (auto* state = context->propertyFor(&state_factory_)) {
                            state->undo_external();
                        }
                    }
                }
                source->setNextInterval(300000);
                return true;
            });
    }

    void keyEvent(const fcitx::InputMethodEntry& entry, fcitx::KeyEvent& event) override {
        if (event.isRelease()) return;
        const auto& program = event.inputContext()->program();
        if (program != last_program_) {
            last_program_ = program;
            store_.set_string_setting("current_app", program);
        }
        active_context_ = event.inputContext()->watch();
        auto* state = event.inputContext()->propertyFor(&state_factory_);
        // Entry is authoritative for this key once any in-flight switch has settled.
        state->sync_logical_from_entry(entry, "keyEvent");
        if (is_logging_enabled(&store_)) {
            std::cerr << "[SmartType Event] uniqueName: " << entry.uniqueName()
                      << " logical: " << state->active_input_method()
                      << " sym: " << event.key().sym()
                      << " code: " << event.key().code()
                      << " raw_sym: " << event.rawKey().sym()
                      << " raw_code: " << event.rawKey().code()
                      << " orig_sym: " << event.origKey().sym()
                      << " orig_code: " << event.origKey().code() << std::endl;
        }
        state->key_event(event);
    }

    void setCurrentInputMethod(fcitx::InputContext* ic, const std::string& imName) {
        instance_->setCurrentInputMethod(ic, imName, false);
    }

    fcitx::FactoryFor<SmartTypeState>& state_factory() { return state_factory_; }

    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override {
        auto* state = event.inputContext()->propertyFor(&state_factory_);
        // Focus-in / IM activate: align logical IM only — do not force KDE/Fcitx loops.
        state->on_activate(entry);
    }

    void reset(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        event.inputContext()->propertyFor(&state_factory_)->reset(false);
    }

    void deactivate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        event.inputContext()->propertyFor(&state_factory_)->reset(false);
    }

    smarttype::Corrector& corrector() { return corrector_; }
    smarttype::PersonalStore& store() { return store_; }
    smarttype::DecisionLog& decision_log() { return decision_log_; }
    SmartTypeUiClient& ui_client() { return ui_client_; }
    fcitx::Instance* instance() { return instance_; }

private:
    fcitx::Instance* instance_;
    smarttype::Corrector corrector_;
    smarttype::PersonalStore store_;
    smarttype::DecisionLog decision_log_;
    SmartTypeUiClient ui_client_;
    fcitx::FactoryFor<SmartTypeState> state_factory_;
    std::string last_program_;
    std::string last_undo_request_;
    fcitx::TrackableObjectReference<fcitx::InputContext> active_context_;
    std::unique_ptr<fcitx::EventSourceTime> control_timer_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> cursor_rect_watcher_;
};

SmartTypeState::SmartTypeState(SmartTypeEngine* engine, fcitx::InputContext* input_context)
    : engine_(engine), input_context_(input_context) {
    // Cannot call Instance::inputMethod(), frontendName(), or other virtual IC
    // methods here: the InputContext subclass is still under construction
    // (registerInputContext → property factory). Calling frontendName() caused
    // __cxa_pure_virtual / SIGABRT crash loops when diagnostics logging was on.
    // Real IM is applied on activate() / first keyEvent via sync_logical_*.
    // Empty sentinel until the first sync so we never treat an unsynced state as
    // a hard-coded "smarttype" decision.
    active_input_method_.clear();
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Layout] state_created"
                  << " ic=" << static_cast<const void*>(input_context_)
                  << " logical=(pending_sync)" << std::endl;
    }
}

SmartTypeState::~SmartTypeState() {
    switch_guard_timer_.reset();
    if (!is_test_environment()) {
        engine_->ui_client().hide(true);
    }
}

std::string SmartTypeState::resolve_actual_smarttype_im() const {
    // Safe only after IC registration completes (activate / keyEvent / after ctor).
    const std::string current = engine_->instance()->inputMethod(input_context_);
    if (is_smarttype_im(current)) {
        return current;
    }
    // Non-SmartType global IM: keep a previous SmartType logical value if any.
    if (is_smarttype_im(active_input_method_)) {
        return active_input_method_;
    }
    return "smarttype";
}

void SmartTypeState::sync_logical_input_method(std::string_view reason) {
    const std::string actual = resolve_actual_smarttype_im();
    if (actual == active_input_method_) {
        return;
    }
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Layout] sync_logical"
                  << " reason=" << reason
                  << " program=" << input_context_->program()
                  << " frontend=" << input_context_->frontendName()
                  << " ic=" << static_cast<const void*>(input_context_)
                  << " from=" << active_input_method_
                  << " to=" << actual
                  << " fcitx_im=" << engine_->instance()->inputMethod(input_context_)
                  << std::endl;
    }
    active_input_method_ = actual;
}

void SmartTypeState::sync_logical_from_entry(const fcitx::InputMethodEntry& entry,
                                            std::string_view reason, bool from_activate) {
    const std::string& name = entry.uniqueName();
    if (programmatic_switch_in_progress_) {
        if (!is_smarttype_im(name)) {
            return;
        }
        if (name == active_input_method_) {
            // Our switch completed (activate of the target IM).
            programmatic_switch_in_progress_ = false;
            switch_guard_timer_.reset();
            if (is_logging_enabled(&engine_->store())) {
                std::cerr << "[SmartType Layout] switch_settled"
                          << " reason=" << reason
                          << " im=" << name << std::endl;
            }
            return;
        }
        if (from_activate) {
            // External setCurrentInputMethod while a previous guard was still held.
            if (is_logging_enabled(&engine_->store())) {
                std::cerr << "[SmartType Layout] switch_override"
                          << " reason=" << reason
                          << " from=" << active_input_method_
                          << " to=" << name << std::endl;
            }
            active_input_method_ = name;
            programmatic_switch_in_progress_ = false;
            switch_guard_timer_.reset();
            return;
        }
        // Stale keyEvent entry during in-flight switch — ignore.
        return;
    }
    if (is_smarttype_im(name)) {
        if (!pending_layout_input_method_.empty() &&
            name == pending_layout_input_method_) {
            // The target became active externally before our word boundary.
            pending_layout_input_method_.clear();
        }
        if (active_input_method_ != name) {
            if (is_logging_enabled(&engine_->store())) {
                std::cerr << "[SmartType Layout] sync_from_entry"
                          << " reason=" << reason
                          << " program=" << input_context_->program()
                          << " frontend=" << input_context_->frontendName()
                          << " ic=" << static_cast<const void*>(input_context_)
                          << " from=" << active_input_method_
                          << " to=" << name << std::endl;
            }
            active_input_method_ = name;
        }
        return;
    }
    sync_logical_input_method(reason);
}

void SmartTypeState::on_activate(const fcitx::InputMethodEntry& entry) {
    sync_logical_from_entry(entry, "activate", /*from_activate=*/true);
}

void SmartTypeState::clear_layout_switch_episode() {
    layout_switch_undo_active_ = false;
    layout_switched_for_current_word_ = false;
    layout_switch_undo_im_.clear();
    layout_switch_undo_layout_idx_.clear();
    layout_switch_undo_buffer_.clear();
    layout_switch_post_buffer_.clear();
}

void SmartTypeState::suppress_layout_label_im_info() {
    // Fcitx Behavior.ShowInputMethodInformation + CompactInputMethodInformation
    // paints Label=RU / Label=EN into auxUp for ~1s after IM switch. That row
    // is rendered above candidates and grows the panel upward (ST-042).
    // Clear only when aux is purely that toast — leave real aux/preedit alone.
    auto& panel = input_context_->inputPanel();
    const std::string aux = panel.auxUp().toString();
    static constexpr std::string_view kLayoutToasts[] = {
        "RU",
        "EN",
        "SmartType Russian",
        "SmartType English",
        "SmartType Русский",
        "SmartType Английский",
    };
    bool is_layout_toast = false;
    for (const auto label : kLayoutToasts) {
        if (aux == label) {
            is_layout_toast = true;
            break;
        }
    }
    if (!is_layout_toast) {
        return;
    }
    panel.setAuxUp(fcitx::Text());
    input_context_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Layout] suppress_im_info toast='" << aux << "'"
                  << std::endl;
    }
}

void SmartTypeState::apply_programmatic_layout_switch(const std::string& new_im,
                                                     std::string_view reason) {
    if (!is_smarttype_im(new_im)) {
        return;
    }

    pending_layout_input_method_.clear();

    const std::string before_im = engine_->instance()->inputMethod(input_context_);
    const std::string before_logical = active_input_method_;
    const uint64_t gen = ++switch_generation_;
    programmatic_switch_in_progress_ = true;
    active_input_method_ = new_im;

    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Layout] programmatic_switch"
                  << " gen=" << gen
                  << " reason=" << reason
                  << " program=" << input_context_->program()
                  << " frontend=" << input_context_->frontendName()
                  << " ic=" << static_cast<const void*>(input_context_)
                  << " fcitx_before=" << before_im
                  << " logical_before=" << before_logical
                  << " requested=" << new_im
                  << std::endl;
    }

    engine_->setCurrentInputMethod(input_context_, new_im);
    // setCurrentInputMethod fires InputContextSwitchInputMethod (reason Other)
    // which synchronously sets auxUp to the compact Label. Clear it now so a
    // subsequent flush-after-update_preedit path does not leave RU/EN visible.
    suppress_layout_label_im_info();

    // KDE indicator: fire-and-forget (does not touch SmartTypeState).
    // Skip in integration tests — busctl children can keep the test process alive under ctest.
    if (!is_test_environment()) {
        const std::string layout_idx = kde_layout_idx_for_im(new_im);
        [[maybe_unused]] int res = std::system(("busctl --user call org.kde.keyboard /Layouts org.kde.KeyboardLayouts setLayout u " +
                     layout_idx + " >/dev/null 2>&1 &")
                        .c_str());
    }

    // Keep reset/deactivate from wiping buffer until the IM transition settles.
    // Prefer clearing the guard from activate() when the target entry arrives.
    // Backup one-shot timer only outside integration tests (event-loop timers
    // have interacted poorly with ctest process lifetime in the past).
    switch_guard_timer_.reset();
    if (!is_test_environment()) {
        auto& loop = engine_->instance()->eventLoop();
        switch_guard_timer_ = loop.addTimeEvent(
            CLOCK_MONOTONIC, fcitx::now(CLOCK_MONOTONIC) + 150000, 0,
            [this, gen](fcitx::EventSourceTime*, uint64_t) {
                if (gen == switch_generation_) {
                    programmatic_switch_in_progress_ = false;
                    if (is_logging_enabled(&engine_->store())) {
                        std::cerr << "[SmartType Layout] switch_guard_clear gen=" << gen
                                  << " logical=" << active_input_method_
                                  << " fcitx_im="
                                  << engine_->instance()->inputMethod(input_context_)
                                  << std::endl;
                    }
                }
                switch_guard_timer_.reset();
                return false;  // one-shot — do not re-arm
            });
    }

    if (is_logging_enabled(&engine_->store())) {
        const std::string after_im = engine_->instance()->inputMethod(input_context_);
        std::cerr << "[SmartType Layout] programmatic_switch_result"
                  << " gen=" << gen
                  << " fcitx_after=" << after_im
                  << " logical_after=" << active_input_method_
                  << " ok=" << (after_im == new_im ? "true" : "false") << std::endl;
    }
}

void SmartTypeState::defer_layout_switch(const std::string& new_im,
                                         std::string_view reason) {
    if (!is_smarttype_im(new_im) || new_im == active_input_method_) {
        pending_layout_input_method_.clear();
        return;
    }
    pending_layout_input_method_ = new_im;
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Layout] switch_deferred"
                  << " reason=" << reason
                  << " program=" << input_context_->program()
                  << " frontend=" << input_context_->frontendName()
                  << " physical=" << active_input_method_
                  << " target=" << pending_layout_input_method_
                  << " buffer='" << buffer_ << "'" << std::endl;
    }
}

void SmartTypeState::flush_deferred_layout_switch(std::string_view reason) {
    if (pending_layout_input_method_.empty()) {
        return;
    }
    // A delayed correction is still client preedit. Switching here would
    // recreate the exact transaction-boundary bug ST-041 is fixing.
    if (!buffer_.empty() || delayed_commit_.has_value()) {
        return;
    }
    if (uses_client_preedit() &&
        !input_context_->inputPanel().clientPreedit().toString().empty()) {
        return;
    }

    const std::string target = pending_layout_input_method_;
    pending_layout_input_method_.clear();
    apply_programmatic_layout_switch(target, reason);
}

bool SmartTypeState::cancel_deferred_layout_switch(std::string_view reason) {
    if (pending_layout_input_method_.empty()) {
        return false;
    }
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Layout] proactive_switch_cancelled"
                  << " reason=" << reason
                  << " physical=" << active_input_method_
                  << " target=" << pending_layout_input_method_ << std::endl;
    }
    pending_layout_input_method_.clear();
    return true;
}

std::vector<std::string> SmartTypeState::get_current_suggestions() const {
    if (!engine_->store().setting_enabled("suggestions")) {
        return {};
    }
    if (buffer_ == cache_buffer_ && context_ == cache_context_) {
        return suggestions_cache_;
    }
    cache_buffer_ = buffer_;
    cache_context_ = context_;

    std::vector<std::string> suggestions;
    std::vector<std::string> context(context_.begin(), context_.end());
    if (!buffer_.empty()) {
        suggestions = engine_->corrector().get_candidates(buffer_, context, &engine_->store());
        const bool accidental_case = engine_->store().setting_enabled("accidental_case");
        const bool layout_enabled = engine_->store().setting_enabled("layout_correction", true) &&
                                    engine_->store().string_setting("layout_mode", "suggest") != "disabled";
        if (!accidental_case || !layout_enabled) {
            const auto preview = engine_->corrector().decide(buffer_, context, &engine_->store());
            if ((preview.reason == "accidental mixed case" && !accidental_case) ||
                (preview.reason == "layout correction" && !layout_enabled)) {
                suggestions.clear();
            }
        }
    } else if (!context_.empty()) {
        const std::string& last_word = context_.back();
        suggestions = engine_->store().next_words(last_word, 3);
    }
    suggestions_cache_ = suggestions;
    return suggestions;
}

std::size_t SmartTypeState::erase_leaked_latin_token_before_cursor(
    std::string_view expected_token) {
    if (is_gnome_ibus_proxy()) {
        return 0;
    }
    if (!input_context_->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText)) {
        return 0;
    }
    const auto surrounding = input_context_->surroundingText();
    if (!surrounding.isValid() || surrounding.cursor() == 0) {
        return 0;
    }
    const std::vector<uint32_t> text_u32 = utf8_to_utf32_local(surrounding.text());
    const unsigned int cur = surrounding.cursor();
    if (cur > text_u32.size()) {
        return 0;
    }
    unsigned int start = cur;
    while (start > 0 && is_word_character(text_u32[start - 1])) {
        --start;
    }
    const std::size_t char_len = static_cast<std::size_t>(cur - start);
    if (char_len == 0) {
        return 0;
    }
    const auto token_begin = fcitx::utf8::nextNChar(
        surrounding.text().begin(), static_cast<std::size_t>(start));
    const auto token_end = fcitx::utf8::nextNChar(
        surrounding.text().begin(), static_cast<std::size_t>(cur));
    const std::string actual_token(token_begin, token_end);
    if (smarttype::lowercase_ru(actual_token) !=
        smarttype::lowercase_ru(expected_token)) {
        // Some frontends exclude the live preedit from SurroundingText. Never
        // mistake the preceding, already committed English word for a leaked
        // copy of the current layout-typo token.
        return 0;
    }
    bool has_latin = false;
    for (unsigned int i = start; i < cur; ++i) {
        const uint32_t ch = text_u32[i];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            has_latin = true;
            break;
        }
    }
    if (!has_latin) {
        return 0;
    }
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Layout] erase_leaked_latin_token"
                  << " chars=" << char_len
                  << " program=" << input_context_->program() << std::endl;
    }
    erase_committed(char_len, 0);
    return char_len;
}

void SmartTypeState::update_preedit() {
    auto& panel = input_context_->inputPanel();
    // ST-041: while client preedit is composing, never panel.reset().
    // reset() briefly clears clientPreedit; Qt/Chromium then drop or partially
    // commit the old-layout prefix (live: ghbdtn → "ghbdет" / "веет").
    const bool composing =
        uses_client_preedit() && (!buffer_.empty() || delayed_commit_.has_value());
    if (!composing) {
        panel.reset();
    } else {
        panel.setAuxUp(fcitx::Text());
        panel.setAuxDown(fcitx::Text());
        panel.setPreedit(fcitx::Text());
    }

    if (is_disabled_context(input_context_, &engine_->store())) {
        if (!is_test_environment()) {
            engine_->ui_client().hide(true);
        }
        if (composing) {
            panel.setCandidateList(nullptr);
            panel.setClientPreedit(fcitx::Text());
        }
        input_context_->updatePreedit();
        input_context_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        return;
    }

    std::vector<std::string> suggestions = get_current_suggestions();
    if (candidates_dismissed_) {
        suggestions.clear();
    }

    if (!suggestions.empty()) {
        const int sel = std::min<int>(selected_candidate_, static_cast<int>(suggestions.size()) - 1);
        const bool ext_ui = use_external_ui(&engine_->store());

        // Determine the render path:
        //   native server UI  — frontend has zwp_input_method_v2 (wayland_v2) or zwp_input_panel_v1
        //                       SmartTypeUI creates a native input-popup surface
        //   client-side UI    — frontend is "dbus": Qt IM plugin routes candidates back into the
        //                       client widget; no server-side popup surface is possible
        //   external QML      — smarttype-ui process (legacy path, external_ui=true)
        const std::string_view frontend = input_context_->frontendName();
        const auto caps = input_context_->capabilityFlags();
        const bool client_side_panel = caps.test(fcitx::CapabilityFlag::ClientSideInputPanel);
        const bool native_wayland = (frontend == "wayland_v2" || frontend == "wayland");

        std::string render_path;
        if (ext_ui) {
            render_path = "external QML (smarttype-ui process)";
        } else if (native_wayland) {
            render_path = "native server UI (zwp popup surface)";
        } else {
            render_path = "client-side UI (no server popup — DBus frontend)";
        }

        // ── SmartType renderer diagnostic ────────────────────────────────────
        if (is_logging_enabled(&engine_->store())) {
            std::cerr << "[SmartType] "
                      << "ActiveUI: " << engine_->instance()->currentUI() << " | "
                      << "Frontend: " << frontend << " | "
                      << "RenderPath: " << render_path << " | "
                      << "ClientSideInputPanel: " << (client_side_panel ? "true" : "false") << " | "
                      << "Caps[Password]: " << caps.test(fcitx::CapabilityFlag::Password) << " "
                      << "[Terminal]: " << caps.test(fcitx::CapabilityFlag::Terminal) << " "
                      << "[Disable]: " << caps.test(fcitx::CapabilityFlag::Disable) << " | "
                      << "Program: " << input_context_->program() << " | "
                      << "Candidates: " << suggestions.size() << " sel=" << sel << "\n";
        }
        // ────────────────────────────────────────────────────────────────────

        if (!ext_ui) {
            auto candidate_list = std::make_unique<fcitx::CommonCandidateList>();
            candidate_list->setPageSize(3);
            for (std::size_t index = 0; index < suggestions.size(); ++index) {
                const auto& sugg = suggestions[index];
                candidate_list->append<SmartTypeCandidateWord>(
                    sugg, static_cast<int>(index) == sel, [this, sugg]() {
                    commit_candidate(sugg);
                });
            }
            // `setGlobalCursorIndex` is available in Fcitx 5.1.7 (Ubuntu
            // 24.04) as well as newer releases; `setCursorIndex` was added
            // later and made the source build unnecessarily distro-specific.
            candidate_list->setGlobalCursorIndex(sel);
            panel.setCandidateList(std::move(candidate_list));
        } else {
            engine_->ui_client().update(input_context_, buffer_, suggestions, sel);
        }
    } else {
        if (composing) {
            panel.setCandidateList(nullptr);
        }
        if (use_external_ui(&engine_->store())) {
            engine_->ui_client().hide();
        }
    }

    if (uses_client_preedit() && (!buffer_.empty() || delayed_commit_.has_value())) {
        fcitx::Text text;
        if (delayed_commit_.has_value()) {
            if (is_logging_enabled(&engine_->store())) {
                std::cerr << "[LOG SmartType] highlight preedit sent: text='"
                          << (delayed_commit_->corrected_word + delayed_commit_->delimiter)
                          << "' target_app='" << input_context_->program() << "'" << std::endl;
            }
            text.append(delayed_commit_->corrected_word + delayed_commit_->delimiter,
                        fcitx::TextFormatFlag::HighLight);
            if (!buffer_.empty()) {
                text.append(buffer_, fcitx::TextFormatFlag::NoFlag);
            }
            text.setCursor(delayed_commit_->corrected_word.size() +
                           delayed_commit_->delimiter.size() + buffer_.size());
        } else {
            text.append(buffer_, fcitx::TextFormatFlag::NoFlag);
            text.setCursor(buffer_.size());
        }
        if (input_context_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
            panel.setClientPreedit(text);
        } else {
            panel.setPreedit(text);
        }
    }

    input_context_->updatePreedit();
    input_context_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    // Keep the original caret as long as this candidate generation is open.
    // A click in the client can cause an InputPanel refresh before the matching
    // CursorRectChanged event is dispatched. Replacing the anchor here made
    // that external click look like an internal refresh, so the native Wayland
    // popup followed the new line instead of being dismissed.
    if (panel.candidateList() && !candidate_anchor_valid_) {
        const auto& rect = input_context_->cursorRect();
        candidate_anchor_x_ = rect.left();
        candidate_anchor_y_ = rect.top();
        candidate_anchor_valid_ = true;
    }
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[LOG SmartType] input panel updated: visible=" << (panel.candidateList() ? "true" : "false") << std::endl;
    }
}

void SmartTypeState::on_cursor_rect_changed() {
    if (!candidate_anchor_valid_ || candidates_dismissed_ ||
        input_context_->inputPanel().candidateList() == nullptr) {
        return;
    }
    const auto& rect = input_context_->cursorRect();
    if (rect.left() == candidate_anchor_x_ && rect.top() == candidate_anchor_y_) {
        return;
    }
    // XIM commits each literal key before publishing the matching caret rect.
    // That delayed rect is part of typing, not a mouse relocation. Keep this
    // exception X11-only and short so an ordinary later click still dismisses
    // the panel instead of moving it.
    if (engine_->store().setting_enabled("x11_normalize_layout", false)) {
        const auto since_key = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_key_event_time_);
        if (since_key.count() >= 0 && since_key.count() <= 180) {
            candidate_anchor_x_ = rect.left();
            candidate_anchor_y_ = rect.top();
            return;
        }
    }
    // A cursor-rect change without a corresponding SmartType panel update is
    // an external caret relocation (most commonly a mouse click).  The
    // Wayland input-popup protocol follows that rect, so accepting it makes
    // the still-open candidate panel visibly jump to the click position.
    //
    // Do not use a time window after a key event here: a quick mouse click can
    // occur inside that window and used to re-anchor the panel instead of
    // dismissing it.  Normal typing publishes a fresh panel through
    // update_preedit(), which establishes a new anchor before the next list is
    // displayed.
    // The client has moved the document caret away from the composition.
    // Dropping only CandidateList leaves buffer_ and clientPreedit alive; the
    // next delimiter then commits the old candidate at the new caret. Force a
    // non-committing reset so the click cancels the whole stale transaction.
    reset(false, true);
}

void SmartTypeState::schedule_preedit() {
    suggestion_timer_.reset();
    suggestion_timer_ = engine_->instance()->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC, fcitx::now(CLOCK_MONOTONIC) + 60000, 0,
        [this](fcitx::EventSourceTime*, uint64_t) {
            if (input_context_->hasFocus()) update_preedit();
            return false;
        });
}

void SmartTypeState::commit_candidate(const std::string& candidate_word) {
    const std::string original = buffer_;
    const std::string previous = context_.empty() ? "" : context_.back();
    if (!buffer_.empty() && !uses_client_preedit()) {
        const std::size_t L = fcitx::utf8::length(buffer_);
        erase_committed(L, buffer_.size());
    }
    clear_client_preedit_before_commit();
    buffer_.clear();
    input_context_->commitString(candidate_word + " ");
    pending_auto_space_ = true;
    if (!original.empty() && original == candidate_word &&
        engine_->store().setting_enabled("learning")) {
        engine_->store().add_word(original);
        engine_->corrector().add_personal_word(original);
    }
    if (!context_.empty() && engine_->store().setting_enabled("learning")) {
        engine_->store().add_transition(context_.back(), candidate_word);
    }
    remember_context(candidate_word);
    
    if (!original.empty() && original != candidate_word) {
        undo_ = Undo{original, candidate_word, " ", previous};
        engine_->store().add_history(original, candidate_word, input_context_->program(), "selected");
        check_and_switch_layout(original, candidate_word);
    } else {
        undo_.reset();
    }
    last_commit_.reset();
    tracking_manual_correction_ = false;
    selected_candidate_ = 0;
    
    update_preedit();
    flush_deferred_layout_switch("proactive_candidate_commit");
}

void SmartTypeState::remember_context(const std::string& text) {
    std::istringstream stream(text);
    for (std::string word; stream >> word;) {
        context_.push_back(std::move(word));
        while (context_.size() > 3) context_.pop_front();
    }
}

bool SmartTypeState::is_word_character(uint32_t value) const {
    if ((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
        (value >= 0x0410 && value <= 0x044F) || value == 0x0401 || value == 0x0451 ||
        value == '-' || value == '\'') {
        return true;
    }
    if (effective_input_method() == "smarttype-us") {
        return value == ';' || value == ',' || value == '.' ||
               value == '[' || value == ']' || value == '`';
    }
    return false;
}

void SmartTypeState::finish_word(const std::string& delimiter) {
    // ST-021: remember if this token already flipped layout mid-word (proactive).
    // clear_layout_switch_episode() must still run so undo flags do not leak to the next word.
    const bool had_proactive_layout_switch =
        layout_switched_for_current_word_ || layout_switch_undo_active_;
    const std::string proactive_from_im = layout_switch_undo_im_;
    // Pre-switch latin/cyr form of the current token (e.g. "xnj" before "что").
    const std::string proactive_token_original = layout_switch_undo_buffer_;
    // After undoing proactive, buffer is Latin again while layout_switched is still true.
    // Remember that before clear so Space does not immediately re-apply layout.
    bool refuse_layout_reapply = false;
    if (layout_switched_for_current_word_ && !buffer_.empty()) {
        bool buffer_is_latin = true;
        for (unsigned char c : buffer_) {
            if (c >= 0x80) {
                buffer_is_latin = false;
                break;
            }
        }
        refuse_layout_reapply = buffer_is_latin;
    }
    clear_layout_switch_episode();

    if (buffer_.empty()) {
        cancel_deferred_layout_switch("empty_word_boundary");
        input_context_->commitString(delimiter);
        return;
    }
    std::string word_part = buffer_;
    std::string trailing_punct = "";
    while (!word_part.empty()) {
        char back = word_part.back();
        if (back == ';' || back == ',' || back == '.' || back == '/' ||
            back == '[' || back == ']' || back == '`' || back == '\'') {
            trailing_punct = back + trailing_punct;
            word_part.pop_back();
        } else {
            break;
        }
    }
    if (word_part.empty()) {
        word_part = buffer_;
        trailing_punct = "";
    }

    std::vector<std::string> context(context_.begin(), context_.end());
    auto decision = engine_->corrector().decide(word_part, context, &engine_->store());
    if (decision.action == smarttype::Action::replace &&
        engine_->store().is_rule_disabled(word_part, decision.candidate)) {
        decision.action = smarttype::Action::suggest;
        decision.reason += " (disabled by user)";
    }
    const std::string previous_word = context.empty() ? "" : context.back();
    if (decision.action == smarttype::Action::replace &&
        engine_->store().should_demote_correction(word_part, decision.candidate, previous_word)) {
        decision.action = smarttype::Action::suggest;
        decision.reason += " (demoted by undo ratio)";
    }
    if (decision.reason == "accidental mixed case" &&
        !engine_->store().setting_enabled("accidental_case")) {
        decision.action = smarttype::Action::keep;
        decision.candidate = word_part;
    }
    if (decision.reason == "layout correction") {
        const bool layout_enabled = engine_->store().setting_enabled("layout_correction", true) &&
                                    engine_->store().string_setting("layout_mode", "suggest") != "disabled";
        const bool layout_auto = layout_enabled &&
                                 engine_->store().string_setting("layout_mode", "suggest") == "auto";
        
        const std::size_t len = fcitx::utf8::length(word_part);
        bool allow_auto = layout_auto;
        if (allow_auto) {
            bool target_has_cyrillic = false;
            for (unsigned char c : decision.candidate) {
                if (c >= 0x80) {
                    target_has_cyrillic = true;
                    break;
                }
            }
            // Safety rules for auto-correction:
            if (len <= 1) {
                allow_auto = false;  // single letters stay manual/suggest
            } else if (len == 2) {
                // EN→RU 2-letter ("gh"→"пи") stays suggest-only (high false-positive rate).
                // RU→EN 2-letter ("вщ"→"do") is safe enough and unblocks phrases like
                // "вщ нщг" → "do you" without relying on SurroundingText rewrite.
                allow_auto = !target_has_cyrillic;
            } else if (len == 3) {
                // EN→RU 3-letter without Cyrillic context used to be suggest-only.
                // That blocked everyday cases like "xnj"→"что" and broke phrase
                // rewrite ("F ns xnj" never got a layout-corr trigger on Space).
                // Allow auto when the layout candidate is a real dictionary word.
                if (target_has_cyrillic) {
                    if (engine_->corrector().is_dictionary_word(decision.candidate)) {
                        allow_auto = true;
                    } else {
                        bool has_cyrillic_context = false;
                        for (unsigned char c : previous_word) {
                            if (c >= 0x80) {
                                has_cyrillic_context = true;
                                break;
                            }
                        }
                        if (!has_cyrillic_context) {
                            allow_auto = false;
                        }
                    }
                }
            }
        }
        
        if (!layout_enabled) {
            decision.action = smarttype::Action::keep;
            decision.candidate = word_part;
        } else if (!allow_auto && decision.action == smarttype::Action::replace) {
            decision.action = smarttype::Action::suggest;
        }
    }
    // After undoing a mid-word proactive switch, buffer is Latin again. Do not
    // re-apply the same layout fix on Space for this word.
    if (refuse_layout_reapply && decision.action == smarttype::Action::replace &&
        decision.reason == "layout correction") {
        decision.action = smarttype::Action::keep;
        decision.candidate = word_part;
        decision.reason = "layout episode already handled";
    }
    if (!engine_->store().setting_enabled("autocorrect") &&
        decision.action == smarttype::Action::replace) {
        decision.action = smarttype::Action::suggest;
        decision.reason += " (autocorrect disabled)";
    }
    if (engine_->store().setting_enabled("diagnostics", false)) {
        engine_->decision_log().write(word_part, decision);
        engine_->store().add_diagnostic(word_part, decision.candidate,
                                        smarttype::action_name(decision.action), decision.reason,
                                        decision.confidence, input_context_->program());
    }

    const bool is_layout_corr = (decision.reason == "layout correction" && decision.action == smarttype::Action::replace);
    const std::string final_trailing = is_layout_corr ? smarttype::translate_layout(trailing_punct) : trailing_punct;
    const std::string final_delimiter = is_layout_corr ? smarttype::translate_layout(delimiter) : delimiter;
    const std::string replacement_word = decision.action == smarttype::Action::replace ? decision.candidate : word_part;
    const std::string final_replacement = replacement_word + final_trailing;

    // ST-021: when this token is a layout fix (or was fixed mid-word), also rewrite the
    // preceding wrong-layout run still before the cursor ("F ns xnj" → "А ты что").
    std::string phrase_undo_orig;
    std::string phrase_undo_repl;
    bool phrase_rewritten = false;
    {
        bool candidate_has_cyrillic = false;
        for (unsigned char c : final_replacement) {
            if (c >= 0x80) {
                candidate_has_cyrillic = true;
                break;
            }
        }
        bool original_is_latin = true;
        for (unsigned char c : word_part) {
            if (c >= 0x80) {
                original_is_latin = false;
                break;
            }
        }
        bool want_phrase = false;
        bool to_russian = false;
        if (is_layout_corr && original_is_latin != candidate_has_cyrillic) {
            want_phrase = true;
            to_russian = original_is_latin && candidate_has_cyrillic;
        } else if (had_proactive_layout_switch) {
            // Buffer already translated (e.g. xnj→что / ершт→thin mid-word).
            want_phrase = true;
            if (!proactive_from_im.empty()) {
                to_russian = (proactive_from_im == "smarttype-us");
            } else {
                to_russian = candidate_has_cyrillic;
            }
        } else if (!context_.empty() && decision.action == smarttype::Action::keep) {
            // e.g. "think" kept as EN dict while preceding "ш" is reverse-layout gibberish.
            if (!candidate_has_cyrillic && original_is_latin &&
                engine_->corrector().is_dictionary_word(final_replacement)) {
                if (engine_->corrector().try_layout_retranslate(context_.back(), false)) {
                    want_phrase = true;
                    to_russian = false;
                }
            } else if (candidate_has_cyrillic && !original_is_latin &&
                       engine_->corrector().is_dictionary_word(final_replacement)) {
                if (engine_->corrector().try_layout_retranslate(context_.back(), true)) {
                    want_phrase = true;
                    to_russian = true;
                }
            }
        }
        if (want_phrase) {
            // ST-026: while the current token is still client preedit ("что"),
            // forwarded Backspaces (Chrome) or deletes eat that preedit first and
            // only partially remove "F ns " → "FА ты что". Clear preedit first;
            // buffer_ still holds the token for the later commit.
            if (uses_client_preedit() && !buffer_.empty()) {
                auto& panel = input_context_->inputPanel();
                panel.setClientPreedit(fcitx::Text());
                input_context_->updatePreedit();
            }
            phrase_rewritten =
                try_rewrite_layout_phrase_prefix(to_russian, phrase_undo_orig, phrase_undo_repl);
        }
    }
    // Expanded undo payload covers the whole rewritten phrase + current token.
    // Prefer the pre-proactive form of the current token when available ("xnj", not "что").
    const std::string current_token_original =
        (had_proactive_layout_switch && !proactive_token_original.empty())
            ? proactive_token_original
            : word_part;
    // Proactive mid-word (xnj→что) uses Action::keep on Space — still must be undoable.
    const bool expand_layout_undo =
        is_layout_corr || phrase_rewritten || had_proactive_layout_switch;
    const std::string layout_undo_original =
        phrase_rewritten ? (phrase_undo_orig + " " + current_token_original)
                         : current_token_original;
    const std::string layout_undo_replacement =
        phrase_rewritten ? (phrase_undo_repl + " " + final_replacement) : final_replacement;

    // Добавляем запись перехода (биграммы)
    if (!context.empty() && engine_->store().setting_enabled("learning")) {
        engine_->store().add_transition(context.back(), final_replacement);
    }

    // Проверяем, не является ли это ручным исправлением предыдущего слова
    if (engine_->store().setting_enabled("learning") && tracking_manual_correction_ && last_commit_) {
        bool is_dict_word = engine_->corrector().is_dictionary_word(last_commit_->original);
        if (!is_dict_word && fcitx::utf8::length(last_commit_->original) >= 3) {
            if (backspace_count_ == last_commit_->committed_length) {
                if (final_replacement != last_commit_->original && !final_replacement.empty()) {
                    engine_->store().add_learned_rule(last_commit_->original, final_replacement, 0.99,
                                                      last_commit_->context);
                    engine_->corrector().add_rule({last_commit_->original, final_replacement, 0.99, std::nullopt});
                    engine_->store().add_learned_word(final_replacement);
                    engine_->corrector().add_personal_word(final_replacement);
                }
            }
        }
    }

    // Умное самообучение новым словам
    if (engine_->store().setting_enabled("learning") &&
        decision.action != smarttype::Action::replace && !buffer_.empty() &&
        !engine_->corrector().is_protected(buffer_)) {
        const auto suggestions = engine_->corrector().get_candidates(buffer_, context, &engine_->store());
        if (suggestions.empty() && engine_->store().observe_unknown_word(buffer_)) {
            engine_->corrector().add_personal_word(buffer_);
        }
    }

    // Determine correction feedback mode based on frontend
    const std::string_view fe = input_context_->frontendName();
    const auto caps = input_context_->capabilityFlags();
    const bool client_side_panel = caps.test(fcitx::CapabilityFlag::ClientSideInputPanel);
    const bool native_wayland_fe = (fe == "wayland_v2" || fe == "wayland");
    const bool is_correction = (decision.action == smarttype::Action::replace);

    // Feedback mode:
    //   native-panel-pulse  — wayland/wayland_v2: commit immediately, SmartTypeUI renders sweep
    //   inline-preedit      — dbus+ClientSideInputPanel: delayed commit with preedit highlight
    //   immediate           — fallback (no highlight supported)
    std::string feedback_mode;
    if (native_wayland_fe) {
        feedback_mode = "native-panel-pulse";
    } else if (uses_client_preedit() && client_side_panel && supports_preedit_formatting()) {
        feedback_mode = "inline-preedit";
    } else {
        feedback_mode = "immediate";
    }

    if (is_correction && is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Correction] "
                  << "frontend=" << fe << " | "
                  << "ClientSideInputPanel=" << (client_side_panel ? "true" : "false") << " | "
                  << "feedback_mode=" << feedback_mode << " | "
                  << "original='" << buffer_ << "' → corrected='" << final_replacement << "' | "
                  << "program=" << input_context_->program() << "\n";
    }

    if (uses_client_preedit()) {
        // ST-041: drop Latin leftovers that toolkits committed during preedit rewrite.
        if (had_proactive_layout_switch || is_layout_corr) {
            erase_leaked_latin_token_before_cursor(current_token_original);
        }

        const std::string committed = final_replacement;

        if (is_correction &&
            engine_->store().setting_enabled("inline_correction_flash", true)) {

            if (native_wayland_fe) {
                // ── Native Wayland path: commit immediately, trigger panel pulse ──
                // No delayed commit — preedit highlight is not supported on wayland_v2.
                // The correction pulse runs independently in SmartTypeUI (WaylandInputWindow).
                if (is_logging_enabled(&engine_->store())) {
                    std::cerr << "[SmartType Correction] commit_reason=immediate | "
                              << "candidate_list_changed=yes | visibility_changed=no\n";
                }

                commit_delayed(false); // cancel any previous delayed commit
                input_context_->commitString(committed + final_delimiter);

                // Trigger native panel pulse (light sweep in SmartTypeUI)
                if (engine_->store().setting_enabled("inline_correction_flash", true)) {
                    engine_->ui_client().flash(input_context_, final_replacement);
                }

                const std::string original = std::move(buffer_);
                buffer_.clear();
                const std::string undo_from =
                    expand_layout_undo ? layout_undo_original : original;
                const std::string undo_to =
                    expand_layout_undo ? layout_undo_replacement : final_replacement;
                undo_ = Undo{undo_from, undo_to, final_delimiter,
                             context.empty() ? "" : context.back()};
                pending_auto_space_ = final_delimiter == " ";
                engine_->store().add_history(undo_from, undo_to,
                                             input_context_->program(), "native-panel-pulse");
                const std::string old_im = effective_input_method();
                check_and_switch_layout(word_part, final_replacement);
                if (undo_ && effective_input_method() != old_im) {
                    undo_->layout_switched = true;
                    undo_->original_im = old_im;
                    undo_->original_layout_idx = (old_im == "smarttype-us") ? "0" : "1";
                }
                last_commit_.reset();

            } else if (client_side_panel && supports_preedit_formatting()) {
                // ── DBus / client-side path: delayed commit with inline highlight ──
                // Keep the candidate list visible unchanged during the 160 ms flash.
                // Only preedit changes; visibility is NOT toggled.
                if (is_logging_enabled(&engine_->store())) {
                    std::cerr << "[SmartType Correction] commit_reason=delayed(160ms) | "
                              << "candidate_list_changed=no | visibility_changed=no\n";
                }

                commit_delayed(false);
                engine_->ui_client().flash(input_context_, final_replacement);

                const std::string original = std::move(buffer_);
                buffer_.clear();
                const std::string undo_from =
                    expand_layout_undo ? layout_undo_original : original;
                const std::string undo_to =
                    expand_layout_undo ? layout_undo_replacement : final_replacement;

                const std::string old_im = effective_input_method();
                check_and_switch_layout(word_part, final_replacement);
                bool switched = false;
                std::string original_im = "";
                std::string original_layout_idx = "";
                if (effective_input_method() != old_im) {
                    switched = true;
                    original_im = old_im;
                    original_layout_idx = (old_im == "smarttype-us") ? "0" : "1";
                }

                delayed_commit_ = DelayedCommit(undo_from, undo_to, final_delimiter,
                                                context.empty() ? "" : context.back(),
                                                switched, original_im, original_layout_idx);

                auto &loop = engine_->instance()->eventLoop();
                const auto flash_start = std::chrono::steady_clock::now();
                delayed_commit_->timer = loop.addTimeEvent(
                    CLOCK_MONOTONIC, fcitx::now(CLOCK_MONOTONIC) + 160000, 0,
                    [this, flash_start](fcitx::EventSourceTime*, uint64_t) {
                        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - flash_start).count();
                        if (is_logging_enabled(&engine_->store())) {
                            std::cerr << "[SmartType Correction] delayed_commit fired | "
                                      << "flash_elapsed_ms=" << elapsed_ms << "\n";
                        }
                        commit_delayed(true);
                        return true;
                    });

                pending_auto_space_ = final_delimiter == " ";
                engine_->store().add_history(undo_from, undo_to,
                                             input_context_->program(), "inline-preedit-flash");
                last_commit_.reset();
            } else {
                // ── Fallback: immediate commit, no highlight ──
                if (is_logging_enabled(&engine_->store())) {
                    std::cerr << "[SmartType Correction] commit_reason=immediate-no-format | "
                              << "candidate_list_changed=yes | visibility_changed=no\n";
                }
                clear_client_preedit_before_commit();
                input_context_->commitString(committed + final_delimiter);
                engine_->ui_client().flash(input_context_, final_replacement);
                const std::string original = std::move(buffer_);
                buffer_.clear();
                const std::string undo_from =
                    expand_layout_undo ? layout_undo_original : original;
                const std::string undo_to =
                    expand_layout_undo ? layout_undo_replacement : final_replacement;
                undo_ = Undo{undo_from, undo_to, final_delimiter,
                             context.empty() ? "" : context.back()};
                pending_auto_space_ = final_delimiter == " ";
                engine_->store().add_history(undo_from, undo_to,
                                             input_context_->program(), "immediate-fallback");
                const std::string old_im = effective_input_method();
                check_and_switch_layout(word_part, final_replacement);
                if (undo_ && effective_input_method() != old_im) {
                    undo_->layout_switched = true;
                    undo_->original_im = old_im;
                    undo_->original_layout_idx = (old_im == "smarttype-us") ? "0" : "1";
                }
                last_commit_.reset();
            }
        } else {
            // No correction flash, or Action::keep after proactive mid-word layout.
            // ST-026: proactive xnj→что is keep on Space but still needs undo +
            // layout switch bookkeeping (was undo_.reset() → Backspace did nothing).
            clear_client_preedit_before_commit();
            input_context_->commitString(committed + final_delimiter);
            if (!is_correction && !expand_layout_undo) {
                undo_.reset();
                last_commit_ = LastCommit{buffer_, context.empty() ? "" : context.back(),
                    static_cast<int>(fcitx::utf8::length(buffer_) + fcitx::utf8::length(final_delimiter))};
                buffer_.clear();
            } else {
                const std::string original = std::move(buffer_);
                buffer_.clear();
                const std::string undo_from =
                    expand_layout_undo ? layout_undo_original : original;
                const std::string undo_to =
                    expand_layout_undo ? layout_undo_replacement : final_replacement;
                undo_ = Undo{undo_from, undo_to, final_delimiter,
                             context.empty() ? "" : context.back()};
                pending_auto_space_ = final_delimiter == " ";
                engine_->store().add_history(
                    undo_from, undo_to, input_context_->program(),
                    expand_layout_undo && !is_correction ? "proactive-layout-commit"
                                                         : "flash-disabled");
                const std::string old_im = effective_input_method();
                check_and_switch_layout(
                    expand_layout_undo ? current_token_original : word_part,
                    final_replacement);
                if (undo_ && effective_input_method() != old_im) {
                    undo_->layout_switched = true;
                    undo_->original_im = old_im;
                    undo_->original_layout_idx = (old_im == "smarttype-us") ? "0" : "1";
                } else if (undo_ && had_proactive_layout_switch &&
                           !proactive_from_im.empty() &&
                           proactive_from_im != effective_input_method()) {
                    // Deferred switch may not have flushed yet; still record undo IM.
                    undo_->layout_switched = true;
                    undo_->original_im = proactive_from_im;
                    undo_->original_layout_idx =
                        (proactive_from_im == "smarttype-us") ? "0" : "1";
                }
                last_commit_.reset();
            }
        }
        backspace_count_ = 0;
        tracking_manual_correction_ = last_commit_.has_value();
        remember_context(committed);
    }
    else {
        // !uses_client_preedit(): server-side preedit (XWayland, xim, etc.)
        if (is_correction) {
            const std::string original = std::move(buffer_);
            buffer_.clear();
            const std::size_t L = fcitx::utf8::length(original);
            erase_committed(L, original.size());
            input_context_->commitString(final_replacement + final_delimiter);

            // For non-client-preedit wayland frontends: trigger native panel pulse
            if (native_wayland_fe &&
                engine_->store().setting_enabled("inline_correction_flash", true)) {
                engine_->ui_client().flash(input_context_, final_replacement);
                if (is_logging_enabled(&engine_->store())) {
                    std::cerr << "[SmartType Correction] server-side commit + native pulse | "
                              << "frontend=" << fe << "\n";
                }
            }

            const std::string undo_from =
                expand_layout_undo ? layout_undo_original : original;
            const std::string undo_to =
                expand_layout_undo ? layout_undo_replacement : final_replacement;
            undo_ = Undo{undo_from, undo_to, final_delimiter,
                         context.empty() ? "" : context.back()};
            pending_auto_space_ = final_delimiter == " ";
            engine_->store().add_history(undo_from, undo_to,
                                         input_context_->program(), "automatic-fallback-flash");
            const std::string old_im = effective_input_method();
            check_and_switch_layout(word_part, final_replacement);
            if (undo_ && effective_input_method() != old_im) {
                undo_->layout_switched = true;
                undo_->original_im = old_im;
                undo_->original_layout_idx = (old_im == "smarttype-us") ? "0" : "1";
            }
            last_commit_.reset();
            remember_context(final_replacement);
        } else {
            input_context_->commitString(final_delimiter);
            undo_.reset();
            last_commit_ = LastCommit{buffer_, context.empty() ? "" : context.back(),
                static_cast<int>(fcitx::utf8::length(buffer_) + fcitx::utf8::length(final_delimiter))};
            remember_context(buffer_);
            buffer_.clear();
        }
        backspace_count_ = 0;
        tracking_manual_correction_ = last_commit_.has_value();
    }
    update_preedit();
    flush_deferred_layout_switch("proactive_word_boundary");
}

bool SmartTypeState::undo_last() {
    if (!undo_) return false;
    if (undo_timer_) return false;
    if (undo_->layout_switched) {
        apply_programmatic_layout_switch(undo_->original_im, "undo_last");
    }
    const std::string committed = undo_->replacement + undo_->delimiter;
    auto length = fcitx::utf8::lengthValidated(committed);
    if (length == fcitx::utf8::INVALID_LENGTH) {
        length = fcitx::utf8::length(committed);
    }

    bool surrounding_valid = false;
    if (input_context_->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText)) {
        const auto surrounding = input_context_->surroundingText();
        // cursor() is character-based (see SurroundingText::setText docs).
        if (surrounding.isValid() && surrounding.cursor() >= length) {
            const std::string& text = surrounding.text();
            const unsigned int cursor_chars = surrounding.cursor();
            const auto text_chars = fcitx::utf8::lengthValidated(text);
            if (text_chars != fcitx::utf8::INVALID_LENGTH && cursor_chars <= text_chars) {
                const auto slice_begin = fcitx::utf8::nextNChar(
                    text.begin(), cursor_chars - static_cast<unsigned int>(length));
                const auto slice_end = fcitx::utf8::nextNChar(text.begin(), cursor_chars);
                const std::string before(slice_begin, slice_end);
                if (before == committed) {
                    surrounding_valid = true;
                }
            }
        }
    }

    // ST-026: even when SurroundingText matches, Chromium may ignore
    // deleteSurroundingText — use the same unreliable-client policy as erase_committed.
    if (surrounding_valid && !surrounding_delete_is_unreliable()) {
        input_context_->deleteSurroundingText(-static_cast<int>(length),
                                              static_cast<unsigned int>(length));
        commit_undo();
    } else {
        for (std::size_t index = 0; index < length; ++index) {
            input_context_->forwardKey(fcitx::Key(FcitxKey_BackSpace));
        }
        auto& loop = engine_->instance()->eventLoop();
        undo_timer_ = loop.addTimeEvent(
            CLOCK_MONOTONIC, fcitx::now(CLOCK_MONOTONIC) + 50000, 0,
            [this](fcitx::EventSourceTime*, uint64_t) {
                undo_timer_.reset();
                commit_undo();
                return false;
            });
    }
    return true;
}

void SmartTypeState::commit_delayed(bool update) {
    if (!delayed_commit_) return;
    delayed_commit_->timer.reset();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - delayed_commit_->start_time).count();
    std::string reason = update ? "timer expired" : "subsequent non-word key typed or reset";
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[LOG SmartType] delayed commit finished: reason='" << reason
                  << "' elapsed_ms=" << elapsed << std::endl;
    }

    std::string to_commit = delayed_commit_->corrected_word + delayed_commit_->delimiter;
    input_context_->commitString(to_commit);
    undo_ = Undo(delayed_commit_->original_word, delayed_commit_->corrected_word,
                 delayed_commit_->delimiter, delayed_commit_->previous_word,
                 delayed_commit_->layout_switched, delayed_commit_->original_im,
                 delayed_commit_->original_layout_idx);
    auto_punctuation_length_ = 0;
    delayed_commit_.reset();
    if (update || !pending_layout_input_method_.empty()) {
        update_preedit();
    }
    flush_deferred_layout_switch("proactive_delayed_commit");
}

void SmartTypeState::confirm_correction() {
    if (!undo_) return;
    if (engine_->store().setting_enabled("learning")) {
        engine_->store().record_accept(undo_->original, undo_->replacement, undo_->previous_word);
    }
    undo_.reset();
}

void SmartTypeState::commit_undo() {
    if (!undo_) return;
    input_context_->commitString(undo_->original + undo_->delimiter);
    if (engine_->store().setting_enabled("learning")) {
        engine_->store().record_undo(undo_->original, undo_->replacement, undo_->previous_word);
    }
    engine_->store().mark_last_history_undone(undo_->original, undo_->replacement);
    if (!context_.empty()) context_.pop_back();
    remember_context(undo_->original);
    undo_.reset();
    pending_auto_space_ = false;
    auto_punctuation_length_ = 0;
    smart_typography_prefix_ = 0;
    last_input_was_digit_ = false;
    pending_number_space_ = false;
    awaiting_sentence_start_ = false;
}

bool SmartTypeState::supports_preedit_formatting() const {
    if (!input_context_) return true;
    const std::string_view fe = input_context_->frontendName();
    if (fe.starts_with("wayland")) {
        return false;
    }
    return true;
}

void SmartTypeState::clear_client_preedit_before_commit() {
    if (!uses_client_preedit()) {
        return;
    }
    auto& panel = input_context_->inputPanel();
    panel.setClientPreedit(fcitx::Text());
    input_context_->updatePreedit();
}



void SmartTypeState::commit_literal(const std::string& text) {
    if (uses_client_preedit()) {
        clear_client_preedit_before_commit();
        input_context_->commitString(buffer_ + text);
    } else {
        input_context_->commitString(text);
    }
    buffer_.clear();
    input_context_->inputPanel().reset();
    input_context_->updatePreedit();
    input_context_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    flush_deferred_layout_switch("proactive_literal_commit");
}

void SmartTypeState::reset(bool commit, bool force_discard) {
    if (undo_timer_) {
        undo_timer_.reset();
        commit_undo();
    }
    // During a programmatic IM switch, Fcitx emits reset/deactivate. Keep
    // composing state until switch_guard_timer_ clears the generation flag.
    // Do NOT clear programmatic_switch_in_progress_ here — that allowed a
    // second reset to wipe the buffer mid-transition.
    if (programmatic_switch_in_progress_ && !force_discard) {
        if (is_logging_enabled(&engine_->store())) {
            std::cerr << "[SmartType Layout] reset_suppressed"
                      << " commit=" << (commit ? "true" : "false")
                      << " gen=" << switch_generation_
                      << " program=" << input_context_->program()
                      << " frontend=" << input_context_->frontendName()
                      << " logical=" << active_input_method_
                      << " buffer_len=" << buffer_.size() << std::endl;
        }
        return;
    }
    // ST-041: toolkit focus noise / candidate-panel flashes emit reset while
    // client preedit still owns the word. Discarding the buffer leaves Latin
    // leftovers in the widget ("ghbdет", "plhавствуйте"). Keep composing until
    // Escape, explicit finish_word, or reset(commit=true).
    if (!commit && !force_discard && !buffer_.empty() && uses_client_preedit()) {
        if (is_logging_enabled(&engine_->store())) {
            std::cerr << "[SmartType Layout] reset_suppressed_composing"
                      << " program=" << input_context_->program()
                      << " buffer='" << buffer_ << "'"
                      << " pending=" << pending_layout_input_method_ << std::endl;
        }
        return;
    }
    if (!commit) {
        cancel_deferred_layout_switch("reset_without_commit");
    }
    if (commit) {
        commit_delayed(false);
        confirm_correction();
        if (uses_client_preedit() && !buffer_.empty()) {
            clear_client_preedit_before_commit();
            input_context_->commitString(buffer_);
        }
    } else {
        if (delayed_commit_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - delayed_commit_->start_time).count();
            if (is_logging_enabled(&engine_->store())) {
                std::cerr << "[LOG SmartType] delayed commit cancelled: reason='reset without commit' elapsed_ms=" << elapsed << std::endl;
            }
            delayed_commit_->timer.reset();
            delayed_commit_.reset();
        }
    }
    buffer_.clear();
    context_.clear();
    undo_.reset();

    last_commit_.reset();
    tracking_manual_correction_ = false;
    selected_candidate_ = 0;
    pending_auto_space_ = false;
    auto_punctuation_length_ = 0;
    smart_typography_prefix_ = 0;
    last_input_was_digit_ = false;
    pending_number_space_ = false;
    awaiting_sentence_start_ = false;
    protected_sequence_ = false;
    candidates_dismissed_ = false;
    candidate_anchor_valid_ = false;
    suggestion_timer_.reset();
    clear_layout_switch_episode();
    input_context_->inputPanel().setCustomInputPanelCallback({});
    engine_->ui_client().hide(true);
    update_preedit();
    if (commit) {
        flush_deferred_layout_switch("proactive_reset_commit");
    }
}

void SmartTypeState::key_event(fcitx::KeyEvent& event) {
    last_key_event_time_ = std::chrono::steady_clock::now();
    if (event.isRelease()) {
        return;
    }
    // Defensive re-sync after IC recreation / external IM changes.
    // Skip while a programmatic switch is in flight — Fcitx may still report the
    // previous IM for a moment, and overwriting logical would desync the buffer.
    if (!programmatic_switch_in_progress_) {
        sync_logical_input_method("key_event");
    }

    if (is_disabled_context(input_context_, &engine_->store())) {
        // Pause/exclusion turns off corrections, candidates, learning and
        // editing transactions. X11/GNOME may nevertheless keep one physical
        // US XKB group, so retain only the stateless character mapping required
        // by the selected SmartType RU/EN input method. This applies equally to
        // terminal exclusion, global pause and "pause in current application";
        // otherwise pausing SmartType also makes Russian impossible to type.
        reset(true);
        if (event.key().states().testAny(
                fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                                 fcitx::KeyState::Super})) {
            return;
        }
        const uint32_t unicode = fcitx::Key::keySymToUnicode(event.key().sym());
        if (!unicode ||
            !engine_->store().setting_enabled("x11_normalize_layout", false)) {
            return;
        }
        const uint32_t normalized = normalize_layout_unicode(
            unicode, event.origKey().code(), event.origKey().states(),
            effective_input_method() == "smarttype-us");
        if (normalized != unicode) {
            input_context_->commitString(fcitx::utf8::UCS4ToUTF8(normalized));
            return event.filterAndAccept();
        }
        return;
    }

    if (undo_timer_) {
        undo_timer_.reset();
        commit_undo();
    }
    if (event.key().isModifier()) {
        if (event.key().sym() == FcitxKey_Shift_L || event.key().sym() == FcitxKey_Shift_R) {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            if (last_shift_time_ > 0 && (ms - last_shift_time_) < 400) {
                last_shift_time_ = 0;
                if (input_context_->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText)) {
                    const auto surrounding = input_context_->surroundingText();
                    if (surrounding.isValid()) {
                        std::string selected = surrounding.selectedText();
                        if (!selected.empty()) {
                            std::string translated = smarttype::translate_layout(selected);
                            if (translated != selected) {
                                unsigned int A = surrounding.anchor();
                                unsigned int C = surrounding.cursor();
                                unsigned int size = (A > C) ? (A - C) : (C - A);
                                int offset = (A > C) ? 0 : (static_cast<int>(A) - static_cast<int>(C));
                                
                                input_context_->deleteSurroundingText(offset, size);
                                input_context_->commitString(translated);
                                event.filterAndAccept();
                                return;
                            }
                        }
                    }
                }
            } else {
                last_shift_time_ = ms;
            }
        } else {
            last_shift_time_ = 0;
        }
        return;
    }
    last_shift_time_ = 0;

    if (delayed_commit_) {
        bool is_word = false;
        const uint32_t unicode = fcitx::Key::keySymToUnicode(event.key().sym());
        if (unicode && is_word_character(unicode)) {
            is_word = true;
        }
        if (event.key().check(FcitxKey_BackSpace) && buffer_.empty()) {
            is_word = true;
        }
        if (!is_word) {
            commit_delayed(false);
        }
    }

    // Hotkey: Control+Shift+Space to toggle layout of current buffer
    static const fcitx::Key hotkey("Control+Shift+space");
    if (event.key().check(hotkey)) {
        if (!buffer_.empty()) {
            const bool is_en = (effective_input_method() == "smarttype-us");
            buffer_ = smarttype::translate_layout(buffer_);
            selected_candidate_ = 0;

            const std::string new_im = is_en ? "smarttype" : "smarttype-us";
            if (uses_client_preedit()) {
                defer_layout_switch(new_im, "ctrl_shift_space");
            } else {
                apply_programmatic_layout_switch(new_im, "ctrl_shift_space");
            }

            if (uses_client_preedit()) {
                // ST-041: full update without delayed schedule (no empty-preedit flash).
                update_preedit();
            } else {
                erase_committed(fcitx::utf8::lengthValidated(buffer_), buffer_.size());
                input_context_->commitString(buffer_);
            }
            return event.filterAndAccept();
        }
    }

    // Shift and CapsLock are part of ordinary text input. Treating every
    // modifier as a shortcut made capitalized words invisible to SmartType.
    if (event.key().states().testAny(
            fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                             fcitx::KeyState::Super})) {
        confirm_correction();
        reset(true);
        return;
    }

    if (event.key().check(FcitxKey_BackSpace)) {
        smart_typography_prefix_ = 0;
        if (delayed_commit_ && buffer_.empty()) {
            delayed_commit_->timer.reset();
            if (delayed_commit_->layout_switched) {
                if (!cancel_deferred_layout_switch("delayed_commit_backspace")) {
                    if (uses_client_preedit()) {
                        defer_layout_switch(delayed_commit_->original_im,
                                            "delayed_commit_backspace");
                    } else {
                        apply_programmatic_layout_switch(delayed_commit_->original_im,
                                                         "delayed_commit_backspace");
                    }
                }
            }
            buffer_ = delayed_commit_->original_word;
            delayed_commit_.reset();
            update_preedit();
            return event.filterAndAccept();
        }

        if (layout_switch_undo_active_) {
            // Restore the pre-switch snapshot (not retranslate of live buffer — that
            // "ate" extra letters after the user continued typing).
            const std::string switched_buf = buffer_;
            const std::string original =
                !layout_switch_undo_buffer_.empty()
                    ? layout_switch_undo_buffer_
                    : smarttype::translate_layout(switched_buf);
            const std::string previous =
                context_.empty() ? std::string() : context_.back();
            buffer_ = original;
            if (engine_->store().setting_enabled("learning")) {
                // Demote this pair so the same proactive flip is less eager.
                engine_->store().record_undo(original, switched_buf, previous);
                // Only learn the restored source as a personal word when it is a
                // real dictionary form. Learning Latin layout gibberish ("xnj")
                // permanently blocked layout auto while still showing "что" as a
                // candidate only (ST-026 / user Kate logs).
                if (engine_->corrector().is_dictionary_word(original)) {
                    engine_->store().add_learned_word(original);
                    engine_->corrector().add_personal_word(original);
                }
                engine_->store().add_history(original, switched_buf, input_context_->program(),
                                            "proactive-switch");
                engine_->store().mark_last_history_undone(original, switched_buf);
            }
            if (!cancel_deferred_layout_switch("proactive_switch_undo")) {
                if (uses_client_preedit()) {
                    defer_layout_switch(layout_switch_undo_im_,
                                        "proactive_switch_undo");
                } else {
                    apply_programmatic_layout_switch(layout_switch_undo_im_,
                                                     "proactive_switch_undo");
                }
            }
            layout_switch_undo_active_ = false;
            // Block immediate re-switch of the same attempt; cleared on word boundary.
            layout_switched_for_current_word_ = true;
            layout_switch_post_buffer_.clear();
            layout_switch_undo_buffer_.clear();
            if (uses_client_preedit()) {
                update_preedit();
            } else {
                erase_committed(fcitx::utf8::lengthValidated(switched_buf), switched_buf.size());
                input_context_->commitString(buffer_);
                schedule_preedit();
            }
            return event.filterAndAccept();
        }
        if (auto_punctuation_length_ > 0) {
            erase_committed(static_cast<std::size_t>(auto_punctuation_length_), static_cast<std::size_t>(auto_punctuation_length_));
            auto_punctuation_length_ = 0;
            awaiting_sentence_start_ = false;
            return event.filterAndAccept();
        }
        if (undo_last()) {
            return event.filterAndAccept();
        }
        if (!buffer_.empty()) {
            const auto length = fcitx::utf8::lengthValidated(buffer_);
            std::size_t char_byte_size = 1;
            if (length && length != fcitx::utf8::INVALID_LENGTH) {
                const auto last = fcitx::utf8::getLastChar(buffer_);
                char_byte_size = fcitx::utf8::UCS4ToUTF8(last).size();
                buffer_.resize(buffer_.size() - char_byte_size);
            }
            if (buffer_.empty()) {
                cancel_deferred_layout_switch("buffer_erased");
                clear_layout_switch_episode();
            }
            if (uses_client_preedit()) {
                update_preedit();
                return event.filterAndAccept();
            } else {
                erase_committed(1, char_byte_size);
                schedule_preedit();
                return event.filterAndAccept();
            }
        } else {
            // Буфер пуст, пользователь стирает закоммиченный текст
            if (tracking_manual_correction_ && last_commit_) {
                backspace_count_++;
            } else {
                tracking_manual_correction_ = false;
            }
        }
        return;
    }

    if (event.key().check(FcitxKey_Delete)) {
        // Delete belongs to the client document, not to SmartType's composing
        // buffer. Explicit forwarding avoids a frontend-specific fallback that
        // rendered U+007F as a square in Telegram and swallowed the key in
        // other Wayland applications.
        // Deliberately keep the current composition and sentence state: Delete
        // must not behave like a word boundary or change the next correction.
        smart_typography_prefix_ = 0;
        input_context_->forwardKey(event.key());
        return event.filterAndAccept();
    }

    if (event.key().check(FcitxKey_Escape)) {
        const bool had_smarttype_ui = !buffer_.empty() ||
            input_context_->inputPanel().candidateList() != nullptr;
        reset(had_smarttype_ui);
        if (had_smarttype_ui) return event.filterAndAccept();
        return;
    }

    const bool client_side_panel = input_context_->capabilityFlags().test(fcitx::CapabilityFlag::ClientSideInputPanel);

    if (event.key().check(FcitxKey_Up) || event.key().check(FcitxKey_Down)) {
        if (input_context_->inputPanel().candidateList() != nullptr) {
            // Merely dropping CandidateList keeps client preedit alive. Then
            // SmartType still owns Left/Right while the toolkit cannot move
            // its cursor, which feels like a frozen text field until a mouse
            // click resets the IME. Finish the literal composition instead:
            // Down/Up closes the popup but hands normal editing keys back to
            // the application immediately.
            reset(true);
            return event.filterAndAccept();
        }
    }

    if (event.key().check(FcitxKey_Left) || event.key().check(FcitxKey_Right)) {
        const auto suggestions = get_current_suggestions();
        if (!suggestions.empty()) {
            if (client_side_panel) {
                // Let the client-side UI (e.g. Telegram) handle candidate navigation natively.
                return;
            }
            const int count = static_cast<int>(suggestions.size());
            const bool next = event.key().check(FcitxKey_Right) || event.key().check(FcitxKey_Down);
            int new_index = next ? selected_candidate_ + 1 : selected_candidate_ - 1;
            if (new_index >= 0 && new_index < count) {
                selected_candidate_ = new_index;
                update_preedit();
            }
            return event.filterAndAccept();
        }
        if (!buffer_.empty()) reset(true);
        awaiting_sentence_start_ = false;
        return;
    }

    if (event.key().check(FcitxKey_Tab)) {
        const auto suggestions = get_current_suggestions();
        if (!suggestions.empty()) {
            if (client_side_panel) {
                // Let the client-side panel handle selection/commit on Tab natively.
                return;
            }
            commit_candidate(suggestions[std::min<int>(
                selected_candidate_, static_cast<int>(suggestions.size()) - 1)]);
            return event.filterAndAccept();
        }
        reset(true);
        awaiting_sentence_start_ = false;
        return;
    }

    if (event.key().sym() == FcitxKey_Return || event.key().sym() == FcitxKey_KP_Enter) {
        if (!buffer_.empty()) finish_word("");
        reset(true);
        awaiting_sentence_start_ = true;
        return;
    }

    uint32_t unicode = fcitx::Key::keySymToUnicode(event.key().sym());
    if (!unicode) {
        confirm_correction();
        reset(true);
        return;
    }

    if (protected_sequence_) {
        smart_typography_prefix_ = 0;
        const std::string literal = fcitx::utf8::UCS4ToUTF8(unicode);
        input_context_->commitString(literal);
        if (unicode == ' ' || unicode == '\n' || unicode == '\t') {
            protected_sequence_ = false;
        }
        return event.filterAndAccept();
    }

    // GNOME's IBus bridge and some X11 desktops keep the physical XKB group
    // on US while SmartType owns the logical RU/EN layout. Normalize before
    // classifying the key as a word character: punctuation-row QWERTY keys
    // are Russian letters too (`[` -> `х`, `;` -> `ж`, `,` -> `б`, ...).
    const uint32_t source_unicode = unicode;
    const bool normalize_to_active_layout =
        engine_->store().setting_enabled("x11_normalize_layout", false) ||
        layout_switched_for_current_word_ ||
        !pending_layout_input_method_.empty();
    if (normalize_to_active_layout) {
        unicode = normalize_layout_unicode(
            unicode, event.origKey().code(), event.origKey().states(),
            effective_input_method() == "smarttype-us");
    }

    // Hyphen is normally a word character so compounds keep working. A second
    // consecutive hyphen is the one typography exception: remove the first
    // hyphen from preedit, commit any preceding word, and emit an em dash.
    if (unicode == '-' && engine_->store().setting_enabled("smart_punctuation", true) &&
        !buffer_.empty() && buffer_.back() == '-') {
        buffer_.pop_back();
        if (!buffer_.empty()) {
            finish_word("");
        } else {
            update_preedit();
        }
        input_context_->commitString("—");
        confirm_correction();
        pending_auto_space_ = false;
        auto_punctuation_length_ = 1;
        smart_typography_prefix_ = 0;
        awaiting_sentence_start_ = false;
        return event.filterAndAccept();
    }

    if (is_word_character(unicode)) {
        candidates_dismissed_ = false;
        smart_typography_prefix_ = 0;
        confirm_correction();
        pending_auto_space_ = false;
        auto_punctuation_length_ = 0;
        last_input_was_digit_ = false;
        pending_number_space_ = false;
        if (buffer_.empty()) {
            // Start tracking only text typed in the current uninterrupted run.
            // Pulling an old word from surrounding text after a cursor move can
            // corrupt text in the middle of a document.
            if (last_commit_ && backspace_count_ < last_commit_->committed_length - 1) {
                tracking_manual_correction_ = false;
            }
            if (input_context_->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText)) {
                const auto surrounding = input_context_->surroundingText();
                if (surrounding.isValid()) {
                    const unsigned int cur = surrounding.cursor();
                    if (cur >= 1 && cur <= 2) {
                        std::vector<uint32_t> text_u32 = utf8_to_utf32_local(surrounding.text());
                        if (cur <= text_u32.size()) {
                            bool all_word = true;
                            std::string prefix;
                            for (unsigned int i = 0; i < cur; ++i) {
                                if (!is_word_character(text_u32[i])) {
                                    all_word = false;
                                    break;
                                }
                                prefix += fcitx::utf8::UCS4ToUTF8(text_u32[i]);
                            }
                            if (all_word) {
                                buffer_ = prefix;
                            }
                        }
                    }
                }
            }
        }
        uint32_t output_unicode = unicode;
        if (awaiting_sentence_start_ && is_letter(unicode)) {
            if (engine_->store().setting_enabled("sentence_capitalization")) {
                output_unicode = uppercase_letter(unicode);
            }
            awaiting_sentence_start_ = false;
        } else if (is_letter(unicode)) {
            awaiting_sentence_start_ = false;
        }

        const std::string str = fcitx::utf8::UCS4ToUTF8(output_unicode);
        buffer_ += str;
        selected_candidate_ = 0;

        // A proactive switch can trigger after only the first valid prefix
        // (Gh[b] -> При). The remaining physical source keys must extend the
        // same undo transaction; otherwise Ghbdtn -> Привет stores only Ghb.
        if (layout_switched_for_current_word_ && !layout_switch_undo_buffer_.empty()) {
            const uint32_t undo_unicode = normalize_layout_unicode(
                source_unicode, event.origKey().code(), event.origKey().states(),
                layout_switch_undo_im_ == "smarttype-us");
            layout_switch_undo_buffer_ += fcitx::utf8::UCS4ToUTF8(undo_unicode);
        }

        // ST-041: if a prior preedit rewrite left Latin in SurroundingText, drop it
        // as soon as we are in a layout-switch episode so it cannot accumulate.
        if (layout_switched_for_current_word_ || !pending_layout_input_method_.empty()) {
            erase_leaked_latin_token_before_cursor(layout_switch_undo_buffer_);
        }

        // Continuing after the first proactive prefix temporarily accepts the
        // switch, so Backspace edits an unfinished target normally. Re-arm the
        // transaction when that continuation reaches a complete target word:
        // once Ghb -> При grows into Ghbdtn -> Привет, the visible automatic
        // replacement is complete and the first Backspace should restore the
        // full physical source even before a delimiter is typed.
        if (layout_switched_for_current_word_ &&
            !layout_switch_undo_buffer_.empty()) {
            const bool complete_target =
                engine_->corrector().is_dictionary_word(buffer_);
            const bool source_is_word =
                engine_->corrector().is_dictionary_word(layout_switch_undo_buffer_);
            layout_switch_undo_active_ = complete_target && !source_is_word;
            if (layout_switch_undo_active_) {
                layout_switch_post_buffer_ = buffer_;
            } else {
                layout_switch_post_buffer_.clear();
            }
        }

        // Proactive mid-word layout correction (current token only; prior committed text stays).
        const bool layout_enabled =
            engine_->store().setting_enabled("layout_correction", true) &&
            engine_->store().string_setting("layout_mode", "suggest") != "disabled";
        const bool is_en = (effective_input_method() == "smarttype-us");
        const std::size_t min_len = is_en ? 3 : 4;
        if (layout_enabled && fcitx::utf8::lengthValidated(buffer_) >= min_len &&
            !layout_switched_for_current_word_) {
            const std::string translated = smarttype::translate_layout(buffer_);

            bool mismatch = false;
            if (is_en) {
                // EN→RU: Latin looks invalid as English, Cyrillic is a valid RU prefix.
                mismatch = !engine_->corrector().is_prefix_valid(buffer_, false) &&
                            engine_->corrector().is_prefix_valid(translated, true);
            } else {
                // RU→EN: only for Cyrillic buffers (wrong-layout English). Require a
                // full English dictionary word — not a mere Latin prefix — so
                // legitimate Russian surface forms missing from the .dic stem list
                // (здравствуй is not a prefix of здравствующий) never become
                // "Plhfdcndeq" mid-word. Latin buffers on RU IM are handled on space.
                bool buffer_has_cyrillic = false;
                for (unsigned char c : buffer_) {
                    if (c >= 0x80) {
                        buffer_has_cyrillic = true;
                        break;
                    }
                }
                mismatch = buffer_has_cyrillic &&
                            !engine_->corrector().is_prefix_valid(buffer_, true) &&
                            engine_->corrector().is_dictionary_word(translated);
            }

            // Learned "I undid this layout convert" / user disabled pair.
            if (mismatch &&
                (engine_->store().should_demote_correction(buffer_, translated, "") ||
                 engine_->store().is_rule_disabled(buffer_, translated))) {
                mismatch = false;
            }

            if (mismatch) {
                layout_switched_for_current_word_ = true;
                const std::string old_buffer = buffer_;
                // Snapshot: pre-switch text is old_buffer without the last char that triggered?
                // old_buffer already includes the new letter (translated includes it too).
                // Undo should restore the full pre-switch form including that letter as Latin.
                layout_switch_undo_buffer_ = old_buffer;
                buffer_ = translated;
                layout_switch_post_buffer_ = translated;

                layout_switch_undo_active_ = true;
                layout_switch_undo_im_ = active_input_method_;
                layout_switch_undo_layout_idx_ = is_en ? "0" : "1";

                const std::string new_im = is_en ? "smarttype" : "smarttype-us";
                if (uses_client_preedit()) {
                    defer_layout_switch(new_im, "proactive_mid_word");
                    // Drop any Latin the toolkit already materialised, then show
                    // the full translated preedit in one update (no empty flash).
                    erase_leaked_latin_token_before_cursor(old_buffer);
                    update_preedit();
                    return event.filterAndAccept();
                } else {
                    apply_programmatic_layout_switch(new_im, "proactive_mid_word");
                    std::size_t old_char_len = fcitx::utf8::lengthValidated(old_buffer) - 1;
                    std::size_t old_byte_len = old_buffer.size() - str.size();
                    erase_committed(old_char_len, old_byte_len);
                    input_context_->commitString(buffer_);
                    schedule_preedit();
                    return event.filterAndAccept();
                }
            }
        }

        if (uses_client_preedit()) {
            update_preedit();
            return event.filterAndAccept();
        } else {
            input_context_->commitString(str);
            schedule_preedit();
            return event.filterAndAccept();
        }
    }

    const std::string delimiter = fcitx::utf8::UCS4ToUTF8(unicode);
    const bool smart_punctuation =
        engine_->store().setting_enabled("smart_punctuation", true);
    const bool auto_space_after_punctuation =
        engine_->store().setting_enabled("auto_space_after_punctuation", true);

    const uint32_t previous_typography_prefix = smart_typography_prefix_;
    smart_typography_prefix_ = 0;
    if (smart_punctuation && is_smart_typography_prefix(unicode)) {
        if (previous_typography_prefix == unicode && buffer_.empty()) {
            erase_committed(1, 1);
            input_context_->commitString(smart_typography_replacement(unicode));
            confirm_correction();
            pending_auto_space_ = false;
            auto_punctuation_length_ = 1;
            awaiting_sentence_start_ = false;
            return event.filterAndAccept();
        }
        smart_typography_prefix_ = unicode;
    }
    if (unicode >= '0' && unicode <= '9') {
        confirm_correction();
        finish_word(delimiter);
        last_input_was_digit_ = true;
        pending_number_space_ = false;
        auto_punctuation_length_ = 0;
        awaiting_sentence_start_ = false;
        return event.filterAndAccept();
    }
    if (buffer_.empty()) {
        if (unicode == '@' || unicode == '/' || unicode == '\\' || unicode == '=' ||
            unicode == ':' || (unicode == '.' && last_input_was_digit_)) {
            commit_literal(delimiter);
            protected_sequence_ = true;
            awaiting_sentence_start_ = false;
            auto_punctuation_length_ = 0;
            return event.filterAndAccept();
        }
        if (auto_space_after_punctuation && auto_punctuation_length_ > 0 &&
            is_attaching_punctuation(unicode)) {
            erase_committed(1, 1);
            input_context_->commitString(delimiter + " ");
            ++auto_punctuation_length_;
            awaiting_sentence_start_ = is_sentence_terminal(unicode);
            return event.filterAndAccept();
        }
        // A punctuation key is a new user action, so Backspace must remove
        // that punctuation instead of reverting an older correction. If the
        // correction inserted a space, replace it with the punctuation.
        if (pending_auto_space_ && is_attaching_punctuation(unicode)) {
            erase_committed(1, 1);
            // A correction/candidate commit already inserted a word-boundary
            // space. Punctuation replaces that space transactionally and must
            // restore it afterwards, independently of the general auto-space
            // preference: "исправление " + ',' -> "исправление, ".
            input_context_->commitString(delimiter + " ");
            confirm_correction();
            pending_auto_space_ = false;
            auto_punctuation_length_ = 2;
            tracking_manual_correction_ = false;
            awaiting_sentence_start_ = is_sentence_terminal(unicode);
            return event.filterAndAccept();
        }
        if (auto_space_after_punctuation && unicode == '%' &&
            (pending_number_space_ || has_number_before_space(input_context_))) {
            erase_committed(1, 1);
            input_context_->commitString("% ");
            confirm_correction();
            pending_auto_space_ = false;
            auto_punctuation_length_ = 2;
            last_input_was_digit_ = false;
            pending_number_space_ = false;
            awaiting_sentence_start_ = false;
            return event.filterAndAccept();
        }
        auto_punctuation_length_ = 0;
        const bool follows_number = delimiter == " " && last_input_was_digit_;
        const bool preserve_sentence_start = awaiting_sentence_start_ &&
            (delimiter == " " || delimiter == "\n" || is_opening_delimiter(unicode));
        confirm_correction();
        pending_auto_space_ = false;
        finish_word(delimiter);
        pending_number_space_ = follows_number;
        last_input_was_digit_ = false;
        awaiting_sentence_start_ = preserve_sentence_start || is_sentence_terminal(unicode);
        return event.filterAndAccept();
    }
    if (unicode == '.' && (is_ascii_word(buffer_) || is_single_cyrillic_letter(buffer_))) {
        std::vector<std::string> context(context_.begin(), context_.end());
        auto decision = engine_->corrector().decide(buffer_, context, &engine_->store());
        if (decision.action == smarttype::Action::replace && decision.reason == "layout correction") {
            // Layout correction! Do not protect, let it fall through to finish_word!
        } else {
            commit_literal(delimiter);
            protected_sequence_ = true;
            awaiting_sentence_start_ = false;
            auto_punctuation_length_ = 0;
            return event.filterAndAccept();
        }
    }
    if (unicode == ':' || unicode == '@' || unicode == '/' || unicode == '\\' || unicode == '=') {
        std::vector<std::string> context(context_.begin(), context_.end());
        auto decision = engine_->corrector().decide(buffer_, context, &engine_->store());
        if (decision.action == smarttype::Action::replace && decision.reason == "layout correction") {
            // Layout correction! Do not protect, let it fall through to finish_word!
        } else {
            commit_literal(delimiter);
            protected_sequence_ = true;
            awaiting_sentence_start_ = false;
            auto_punctuation_length_ = 0;
            return event.filterAndAccept();
        }
    }
    finish_word(auto_space_after_punctuation && is_attaching_punctuation(unicode)
                    ? delimiter + " " : delimiter);
    if (is_attaching_punctuation(unicode)) {
        // The punctuation, not the correction, is now the last edit.
        confirm_correction();
        pending_auto_space_ = false;
        auto_punctuation_length_ = auto_space_after_punctuation ? 2 : 1;
        awaiting_sentence_start_ = is_sentence_terminal(unicode);
    } else {
        awaiting_sentence_start_ = is_sentence_terminal(unicode);
    }
    return event.filterAndAccept();
}

SmartTypeUiClient::SmartTypeUiClient(SmartTypeEngine* engine)
    : engine_(engine), instance_(engine->instance()) {}

SmartTypeUiClient::~SmartTypeUiClient() { disconnect(); }

void SmartTypeUiClient::update(fcitx::InputContext* input_context, std::string_view composing,
                               const std::vector<std::string>& candidates_list, int selected_index) {
    if (!use_external_ui(&engine_->store())) return;
    if (candidates_list.empty()) {
        hide();
        return;
    }
    if (!ensure_connected()) return;



    active_context_ = input_context->watch();
    std::ostringstream message;
    const auto& rect = input_context->cursorRect();
    if (rect.left() == 0 && rect.top() == 0 && !std::getenv("SMARTTYPE_INTEGRATION_TEST")) {
        hide();
        return;
    }
    message << "{\"visible\":true,\"flashId\":0,\"selected\":"
            << selected_index << ",\"cursorX\":" << rect.left()
            << ",\"cursorY\":" << rect.top() << ",\"cursorHeight\":" << rect.height()
            << ",\"composingWidth\":"
            << std::min<std::size_t>(180, fcitx::utf8::length(composing) * 10)
            << ",\"program\":\"" << json_escape(input_context->program()) << "\""
            << ",\"frontendName\":\"" << json_escape(std::string(input_context->frontendName())) << "\""
            << ",\"candidates\":[";
    for (std::size_t i = 0; i < candidates_list.size(); ++i) {
        if (i) message << ',';
        message << '"' << json_escape(candidates_list[i]) << '"';
    }
    message << "]}\n";
    send(message.str());
}

void SmartTypeUiClient::flash(fcitx::InputContext* input_context, std::string_view correction) {
    if (!use_external_ui(&engine_->store())) return;
    if (!ensure_connected()) return;

    static int global_flash_id = 0;
    global_flash_id++;

    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[LOG " << get_timestamp() << "] CLIENT sending flashId=" << global_flash_id << " text=" << correction << std::endl;
    }

    active_context_ = input_context->watch();
    const auto& rect = input_context->cursorRect();
    if (rect.left() == 0 && rect.top() == 0 && !std::getenv("SMARTTYPE_INTEGRATION_TEST")) {
        return;
    }
    std::ostringstream message;
    message << "{\"type\":\"flash\",\"id\":" << global_flash_id
            << ",\"x\":" << rect.left()
            << ",\"y\":" << rect.top()
            << ",\"height\":" << rect.height()
            << ",\"word\":\"" << json_escape(correction) << "\"}\n";
    send(message.str());
}

void SmartTypeUiClient::hide(bool force) {
    if (!use_external_ui(&engine_->store())) return;
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[LOG " << get_timestamp() << "] CLIENT hide requested force=" << force << std::endl;
    }
    if (fd_ >= 0) {
        if (force) {
            send("{\"visible\":false,\"flashId\":0}\n");
        } else {
            send("{\"visible\":false}\n");
        }
    }
    active_context_.unwatch();
}

void SmartTypeUiClient::select(int index) {
    auto* context = active_context_.get();
    if (!context || !context->hasFocus()) return;
    auto* state = context->propertyFor(&engine_->state_factory());
    if (state) {
        state->select_and_commit(index);
    }
}

std::string SmartTypeUiClient::socket_path() const {
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime && *runtime) {
        return std::string(runtime) + "/smarttype-ui.sock";
    }
    return "/tmp/smarttype-ui-" + std::to_string(getuid()) + ".sock";
}

void SmartTypeUiClient::spawn_renderer() {
    if (spawn_attempted_) return;
    spawn_attempted_ = true;
    std::string executable = "smarttype-ui";
    if (const char* home = std::getenv("HOME"); home && *home) {
        const std::string local = std::string(home) + "/.local/bin/smarttype-ui";
        if (access(local.c_str(), X_OK) == 0) executable = local;
    }
    char* argv[] = {executable.data(), nullptr};
    pid_t child = 0;
    posix_spawnp(&child, executable.c_str(), nullptr, nullptr, argv, environ);
}

bool SmartTypeUiClient::ensure_connected() {
    if (!use_external_ui(&engine_->store())) return false;
    if (fd_ >= 0) return true;
    const int socket_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socket_fd < 0) return false;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const std::string path = socket_path();
    if (path.size() >= sizeof(address.sun_path)) {
        close(socket_fd);
        return false;
    }
    std::copy(path.begin(), path.end(), address.sun_path);
    address.sun_path[path.size()] = '\0';
    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(socket_fd);
        spawn_renderer();
        return false;
    }
    fd_ = socket_fd;
    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | O_NONBLOCK);
    io_event_ = instance_->eventLoop().addIOEvent(
        fd_, fcitx::IOEventFlags{fcitx::IOEventFlag::In, fcitx::IOEventFlag::Hup,
                                 fcitx::IOEventFlag::Err},
        [this](fcitx::EventSourceIO*, int, fcitx::IOEventFlags flags) {
            if (flags.testAny(fcitx::IOEventFlags{fcitx::IOEventFlag::Hup, fcitx::IOEventFlag::Err})) {
                disconnect();
                return true;
            }
            char data[256];
            for (;;) {
                const auto count = recv(fd_, data, sizeof(data), 0);
                if (count <= 0) break;
                read_buffer_.append(data, static_cast<std::size_t>(count));
            }
            for (std::size_t newline; (newline = read_buffer_.find('\n')) != std::string::npos;) {
                const std::string line = read_buffer_.substr(0, newline);
                read_buffer_.erase(0, newline + 1);
                if (line.starts_with("select ")) select(std::atoi(line.c_str() + 7));
            }
            return true;
        });
    return true;
}

void SmartTypeUiClient::disconnect() {
    io_event_.reset();
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
    read_buffer_.clear();
    active_context_.unwatch();
    spawn_attempted_ = false;
}

void SmartTypeUiClient::send(std::string_view data) {
    if (fd_ < 0) return;
    const auto result = ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL);
    if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) disconnect();
}

bool SmartTypeState::try_rewrite_layout_phrase_prefix(bool to_russian,
                                                     std::string& undo_original_prefix,
                                                     std::string& undo_replacement_prefix) {
    undo_original_prefix.clear();
    undo_replacement_prefix.clear();
    // GNOME's IBus compositor publishes SurroundingText asynchronously and
    // provides no success acknowledgement for DeleteSurroundingText. Mutter
    // rejects stale offsets, after which committing the replacement duplicates
    // or over-deletes the phrase. Prefer safe per-word layout correction here.
    if (is_gnome_ibus_proxy()) {
        return false;
    }
    if (context_.empty()) {
        return false;
    }

    // Candidate words from engine context (newest first), only while retranslatable.
    std::vector<std::string> cand_orig;
    std::vector<std::string> cand_repl;
    constexpr std::size_t kMaxPhraseWords = 5;
    for (auto it = context_.rbegin(); it != context_.rend(); ++it) {
        auto translated = engine_->corrector().try_layout_retranslate(*it, to_russian);
        if (!translated) {
            break;
        }
        cand_orig.push_back(*it);
        cand_repl.push_back(std::move(*translated));
        if (cand_orig.size() >= kMaxPhraseWords) {
            break;
        }
    }
    if (cand_orig.empty()) {
        return false;
    }

    // Only rewrite what is actually present immediately before the cursor in
    // SurroundingText. Never "optimistically" rewrite older context words (that
    // rewrote a prior "ghjtrn " into "проект " while committing "привет").
    if (!input_context_->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText)) {
        return false;
    }
    const auto surrounding = input_context_->surroundingText();
    if (!surrounding.isValid()) {
        return false;
    }
    const std::string& text = surrounding.text();
    const unsigned int cursor_chars = surrounding.cursor();
    const auto text_chars = fcitx::utf8::lengthValidated(text);
    if (text_chars == fcitx::utf8::INVALID_LENGTH || cursor_chars == 0 ||
        cursor_chars > text_chars) {
        return false;
    }

    const auto slice_begin = fcitx::utf8::nextNChar(text.begin(), 0);
    const auto slice_end = fcitx::utf8::nextNChar(text.begin(), cursor_chars);
    const std::string before_cursor(slice_begin, slice_end);

    // Tokenize trailing whitespace-separated words before cursor.
    std::vector<std::string> tokens;
    {
        std::string cur;
        auto flush = [&]() {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        };
        for (size_t i = 0; i < before_cursor.size();) {
            const unsigned char b = static_cast<unsigned char>(before_cursor[i]);
            if (b == ' ' || b == '\t' || b == '\n') {
                flush();
                ++i;
                continue;
            }
            // Copy one UTF-8 character into cur.
            const auto cp_end = fcitx::utf8::nextNChar(before_cursor.begin() +
                                                           static_cast<std::ptrdiff_t>(i),
                                                       1);
            cur.append(before_cursor.begin() + static_cast<std::ptrdiff_t>(i), cp_end);
            i = static_cast<size_t>(cp_end - before_cursor.begin());
        }
        flush();
    }
    if (tokens.empty()) {
        return false;
    }

    // Match longest trailing run: cand_orig[0] is newest (= tokens.back()).
    std::vector<std::string> orig_words;
    std::vector<std::string> repl_words;
    const std::size_t max_n = std::min(tokens.size(), cand_orig.size());
    for (std::size_t n = max_n; n >= 1; --n) {
        bool ok = true;
        for (std::size_t i = 0; i < n; ++i) {
            const std::string& tok = tokens[tokens.size() - 1 - i];
            const std::string& expect = cand_orig[i];
            if (smarttype::lowercase_ru(tok) != smarttype::lowercase_ru(expect)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            for (std::size_t i = 0; i < n; ++i) {
                const std::size_t ti = tokens.size() - n + i;
                const std::size_t ci = n - 1 - i;  // cand index: oldest of run first
                orig_words.push_back(tokens[ti]);
                repl_words.push_back(cand_repl[ci]);
            }
            break;
        }
    }
    if (orig_words.empty()) {
        return false;
    }

    // Rebuild suffix from the actual surrounding tail (preserve spacing: always "word ").
    std::string expected_suffix;
    std::string replacement;
    for (std::size_t i = 0; i < orig_words.size(); ++i) {
        if (i > 0) {
            expected_suffix += ' ';
            replacement += ' ';
            undo_original_prefix += ' ';
            undo_replacement_prefix += ' ';
        }
        expected_suffix += orig_words[i];
        replacement += repl_words[i];
        undo_original_prefix += orig_words[i];
        undo_replacement_prefix += repl_words[i];
    }
    expected_suffix += ' ';
    replacement += ' ';

    // Verify the rebuilt suffix is at the end of before_cursor (char-based).
    const auto suffix_chars = fcitx::utf8::lengthValidated(expected_suffix);
    if (suffix_chars == fcitx::utf8::INVALID_LENGTH || cursor_chars < suffix_chars) {
        // Trailing space may be missing in surrounding — try without final space.
        std::string no_space = expected_suffix;
        if (!no_space.empty() && no_space.back() == ' ') {
            no_space.pop_back();
        }
        const auto ns_chars = fcitx::utf8::lengthValidated(no_space);
        if (ns_chars == fcitx::utf8::INVALID_LENGTH || cursor_chars < ns_chars) {
            return false;
        }
        const auto b0 = fcitx::utf8::nextNChar(
            text.begin(), cursor_chars - static_cast<unsigned int>(ns_chars));
        const auto b1 = fcitx::utf8::nextNChar(text.begin(), cursor_chars);
        const std::string tail(b0, b1);
        if (smarttype::lowercase_ru(tail) != smarttype::lowercase_ru(no_space)) {
            return false;
        }
        if (is_logging_enabled(&engine_->store())) {
            std::cerr << "[SmartType Layout] phrase_rewrite"
                      << " from='" << no_space << "' to='" << replacement << "'"
                      << " words=" << orig_words.size() << " (no trailing space)"
                      << " chrome_like="
                      << (surrounding_delete_is_unreliable() ? "true" : "false") << std::endl;
        }
        // ST-026: use erase_committed (Backspace on Chromium) — raw
        // deleteSurroundingText is often ignored and leaves "F ns А ты что".
        erase_committed(ns_chars, 0);
        // replacement still has trailing space for natural typing flow.
        input_context_->commitString(replacement);
    } else {
        const auto b0 = fcitx::utf8::nextNChar(
            text.begin(), cursor_chars - static_cast<unsigned int>(suffix_chars));
        const auto b1 = fcitx::utf8::nextNChar(text.begin(), cursor_chars);
        const std::string tail(b0, b1);
        if (smarttype::lowercase_ru(tail) != smarttype::lowercase_ru(expected_suffix)) {
            return false;
        }
        if (is_logging_enabled(&engine_->store())) {
            std::cerr << "[SmartType Layout] phrase_rewrite"
                      << " from='" << expected_suffix << "' to='" << replacement << "'"
                      << " words=" << orig_words.size()
                      << " chrome_like="
                      << (surrounding_delete_is_unreliable() ? "true" : "false") << std::endl;
        }
        erase_committed(suffix_chars, 0);
        input_context_->commitString(replacement);
    }

    // Replace rewritten tokens in the engine context window.
    for (std::size_t i = 0; i < orig_words.size() && !context_.empty(); ++i) {
        context_.pop_back();
    }
    for (const auto& w : repl_words) {
        context_.push_back(w);
        while (context_.size() > 3) {
            context_.pop_front();
        }
    }
    return true;
}

void SmartTypeState::check_and_switch_layout(const std::string& original, const std::string& committed) {
    if (is_logging_enabled(&engine_->store())) {
        std::cerr << "[SmartType Layout] check_and_switch_layout original: " << original
                  << " committed: " << committed
                  << " active_im: " << effective_input_method() << std::endl;
    }
    if (original.empty() || committed.empty() || original == committed) return;

    bool has_cyrillic = false;
    for (char c : committed) {
        if (static_cast<unsigned char>(c) >= 0x80) {
            has_cyrillic = true;
            break;
        }
    }

    if (has_cyrillic && effective_input_method() == "smarttype-us") {
        if (uses_client_preedit()) {
            defer_layout_switch("smarttype", "check_and_switch_cyrillic");
        } else {
            apply_programmatic_layout_switch("smarttype", "check_and_switch_cyrillic");
        }
    } else if (!has_cyrillic && effective_input_method() == "smarttype") {
        bool is_layout_trans = false;
        std::string t1 = smarttype::translate_layout(original);
        std::string t1_lower = t1;
        std::transform(t1_lower.begin(), t1_lower.end(), t1_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        std::string comm_lower = committed;
        std::transform(comm_lower.begin(), comm_lower.end(), comm_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (t1_lower == comm_lower) {
            is_layout_trans = true;
        }

        if (is_layout_trans) {
            if (uses_client_preedit()) {
                defer_layout_switch("smarttype-us", "check_and_switch_layout_trans");
            } else {
                apply_programmatic_layout_switch("smarttype-us",
                                                 "check_and_switch_layout_trans");
            }
        }
    }
}

void SmartTypeState::select_and_commit(int index) {
    const auto suggestions = get_current_suggestions();
    if (index >= 0 && index < static_cast<int>(suggestions.size())) {
        commit_candidate(suggestions[index]);
    }
}

class SmartTypeEngineFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new SmartTypeEngine(manager->instance());
    }
};

}  // namespace

FCITX_ADDON_FACTORY(SmartTypeEngineFactory);
