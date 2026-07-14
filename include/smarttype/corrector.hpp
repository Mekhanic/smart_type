#pragma once

#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace smarttype {

class SpellChecker;

enum class Action { keep, suggest, replace };

struct Decision {
    Action action{Action::keep};
    std::string original;
    std::string candidate;
    double confidence{0.0};
    std::string reason;
};

struct Rule {
    std::string typo;
    std::string correction;
    double confidence;
    std::optional<std::string> previous_word;
};

class PersonalStore;

class Corrector {
public:
    Corrector();
    explicit Corrector(std::vector<Rule> rules);
    ~Corrector();
    Corrector(const Corrector&) = delete;
    Corrector& operator=(const Corrector&) = delete;

    [[nodiscard]] Decision decide(
        std::string_view word,
        const std::vector<std::string>& context = {},
        const PersonalStore* store = nullptr) const;
    [[nodiscard]] std::vector<std::string> get_candidates(
        std::string_view word,
        const std::vector<std::string>& context = {},
        const PersonalStore* store = nullptr) const;
    [[nodiscard]] static bool is_protected(std::string_view token);
    [[nodiscard]] static bool is_strongly_protected(std::string_view token);
    [[nodiscard]] std::optional<std::string> normalize_accidental_case(
        std::string_view word) const;
    void add_rule(Rule rule);
    void add_personal_word(std::string word);
    [[nodiscard]] bool is_prefix_valid(std::string_view prefix, bool ru) const;
    [[nodiscard]] bool is_dictionary_word(std::string_view word) const;

    // Clear wrong-layout token → translated form (for phrase-level rewrite).
    // to_russian=true: Latin gibberish typed on EN meant to be Russian.
    // Returns nullopt if the word looks intentional in the source language.
    [[nodiscard]] std::optional<std::string> try_layout_retranslate(
        std::string_view word, bool to_russian) const;

private:
    std::vector<Rule> rules_;
    std::vector<std::string> personal_words_;
    std::unique_ptr<SpellChecker> spell_checker_;
};

std::string action_name(Action action);

}  // namespace smarttype
