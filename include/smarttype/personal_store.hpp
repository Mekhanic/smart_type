#pragma once

#include "smarttype/corrector.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;

namespace smarttype {

struct CorrectionHistoryEntry {
    long long id;
    std::string original;
    std::string replacement;
    std::string app;
    std::string source;
    bool undone;
    long long created_at;
};

struct DiagnosticEntry {
    std::string original;
    std::string candidate;
    std::string action;
    std::string reason;
    double confidence;
    std::string app;
    long long created_at;
};

class PersonalStore {
public:
    explicit PersonalStore(std::filesystem::path path = default_path());
    ~PersonalStore();
    PersonalStore(const PersonalStore&) = delete;
    PersonalStore& operator=(const PersonalStore&) = delete;

    static std::filesystem::path default_path();
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

    void add_word(const std::string& word);
    void add_learned_word(const std::string& word);
    void remove_word(const std::string& word);
    // Records an unknown word and promotes it to the personal dictionary only
    // after it has been seen repeatedly. Returns true on promotion.
    [[nodiscard]] bool observe_unknown_word(const std::string& word, int threshold = 3);
    [[nodiscard]] int unknown_word_observation_count(const std::string& word) const;
    void add_rule(const std::string& typo, const std::string& correction,
                  double confidence = 0.99, const std::string& previous_word = "");
    void record_undo(const std::string& typo, const std::string& correction,
                     const std::string& previous_word = "");
    void record_accept(const std::string& typo, const std::string& correction,
                       const std::string& previous_word = "");
    [[nodiscard]] bool should_demote_correction(
        const std::string& typo, const std::string& correction,
        const std::string& previous_word = "") const;
    void add_transition(const std::string& word, const std::string& next_word);
    [[nodiscard]] std::vector<std::string> words() const;
    [[nodiscard]] std::vector<std::string> all_words() const;
    [[nodiscard]] std::vector<Rule> rules() const;
    void add_learned_rule(const std::string& typo, const std::string& correction,
                          double confidence = 0.99, const std::string& previous_word = "");
    [[nodiscard]] std::vector<std::string> next_words(const std::string& word, int limit = 5) const;
    [[nodiscard]] int get_word_use_count(const std::string& word) const;
    [[nodiscard]] int get_transition_use_count(const std::string& word, const std::string& next_word) const;
    [[nodiscard]] int undo_count(const std::string& typo, const std::string& correction) const;
    
    void blacklist_add(const std::string& app);
    void blacklist_remove(const std::string& app);
    [[nodiscard]] std::vector<std::string> blacklist_get() const;
    [[nodiscard]] bool is_app_blacklisted(const std::string& program) const;
    void set_mode(const std::string& mode);
    [[nodiscard]] std::string mode() const;
    void set_setting(const std::string& key, bool enabled);
    [[nodiscard]] bool setting_enabled(const std::string& key, bool default_value = true) const;
    void set_string_setting(const std::string& key, const std::string& value);
    [[nodiscard]] std::string string_setting(const std::string& key,
                                             const std::string& default_value = "") const;
    void add_history(const std::string& original, const std::string& replacement,
                     const std::string& app, const std::string& source);
    void mark_last_history_undone(const std::string& original, const std::string& replacement);
    [[nodiscard]] std::vector<CorrectionHistoryEntry> history(int limit = 10) const;
    void disable_rule(const std::string& typo, const std::string& correction);
    [[nodiscard]] bool is_rule_disabled(const std::string& typo,
                                        const std::string& correction) const;
    [[nodiscard]] std::vector<Rule> disabled_rules() const;
    void enable_rule(const std::string& typo, const std::string& correction);
    void clear_history();
    void reset_learning();
    void add_diagnostic(const std::string& original, const std::string& candidate,
                        const std::string& action, const std::string& reason,
                        double confidence, const std::string& app);
    [[nodiscard]] std::vector<DiagnosticEntry> diagnostics(int limit = 100) const;
    void clear_diagnostics();

    struct Stats {
        int total_corrections{0};
        int undone_corrections{0};
    };
    [[nodiscard]] Stats get_stats() const;

private:
    void exec(const char* sql) const;
    std::filesystem::path path_;
    sqlite3* db_{nullptr};
};

}  // namespace smarttype
