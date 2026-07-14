#include "smarttype/corrector.hpp"
#include "smarttype/decision_log.hpp"
#include "smarttype/personal_store.hpp"
#include "smarttype/text.hpp"
#include <algorithm>

#include <cstdlib>
#include <fstream>
#include <iostream>

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    smarttype::Corrector corrector;
    auto result = corrector.decide("севодня", {"я"});
    expect(result.action == smarttype::Action::replace && result.candidate == "сегодня", "obvious typo");

    result = corrector.decide("незнаю", {"я"});
    expect(result.action == smarttype::Action::replace && result.candidate == "не знаю", "context rule applies");
    result = corrector.decide("незнаю", {"слово"});
    expect(result.action == smarttype::Action::keep, "context rule does not apply blindly");
    result = corrector.decide("вопщем");
    expect(result.action == smarttype::Action::suggest, "medium confidence only suggests");
    result = corrector.decide("Happ");
    expect(result.action == smarttype::Action::keep, "technical English word protected");
    result = corrector.decide("Xray-сервер");
    expect(result.action == smarttype::Action::keep, "mixed token protected");
    result = corrector.decide("https://example.com/севодня");
    expect(result.action == smarttype::Action::keep, "URL protected");
    for (const char* protected_token : {"VPN", "YouTube", "iPhone", "SmartType", "Windows11"}) {
        result = corrector.decide(protected_token);
        expect(result.action == smarttype::Action::keep, "brand, acronym, or digit token protected");
    }
    result = corrector.decide("РКН");
    expect(result.action == smarttype::Action::keep, "Cyrillic acronym protected");
    result = corrector.decide("и");
    expect(result.action == smarttype::Action::keep && result.candidate == "и",
           "single-character words are never autocorrected");
    expect(corrector.get_candidates("и").empty(),
           "single-character words do not get misleading candidates");
    result = corrector.decide("f");
    expect(result.action == smarttype::Action::replace && result.candidate == "а",
           "single-character layout correction EN -> RU");
    result = corrector.decide("F");
    expect(result.action == smarttype::Action::replace && result.candidate == "А",
           "single-character layout correction EN -> RU (capital)");

    expect(corrector.is_prefix_valid("прив", true), "Cyrillic prefix valid");
    expect(!corrector.is_prefix_valid("ghb", true), "Cyrillic prefix invalid for gibberish");
    expect(corrector.is_prefix_valid("hel", false), "English prefix valid");
    expect(!corrector.is_prefix_valid("мту", false), "English prefix invalid for gibberish");
    
    // Layout correction and english words
    result = corrector.decide("ghjtrn");
    expect(result.action == smarttype::Action::replace && result.candidate == "проект", "layout correction");
    result = corrector.decide("руддщ");
    expect(result.action == smarttype::Action::replace && result.candidate == "hello",
           "reverse layout correction");
    result = corrector.decide("project");
    expect(result.action == smarttype::Action::keep, "correct English word protected");
    result = corrector.decide("docker");
    expect(result.action == smarttype::Action::keep, "technical word protected");

    // ST-021: phrase-level layout helpers ("F ns xnj" → "А ты что")
    {
        auto f = corrector.try_layout_retranslate("F", true);
        expect(f && *f == "А", "try_layout_retranslate F→А");
        auto ns = corrector.try_layout_retranslate("ns", true);
        expect(ns && *ns == "ты", "try_layout_retranslate ns→ты");
        auto xnj = corrector.try_layout_retranslate("xnj", true);
        expect(xnj && *xnj == "что", "try_layout_retranslate xnj→что");
        auto the = corrector.try_layout_retranslate("the", true);
        expect(!the, "try_layout_retranslate leaves real English");
        auto hello_rev = corrector.try_layout_retranslate("руддщ", false);
        expect(hello_rev && *hello_rev == "hello", "try_layout_retranslate reverse руддщ→hello");
        auto sh = corrector.try_layout_retranslate("ш", false);
        expect(sh && *sh == "i", "try_layout_retranslate reverse ш→i");
        auto ershtl = corrector.try_layout_retranslate("ерштл", false);
        expect(ershtl && *ershtl == "think", "try_layout_retranslate reverse ерштл→think");
        auto vshch = corrector.try_layout_retranslate("вщ", false);
        expect(vshch && *vshch == "do", "try_layout_retranslate reverse вщ→do");
        auto nshchg = corrector.try_layout_retranslate("нщг", false);
        expect(nshchg && *nshchg == "you", "try_layout_retranslate reverse нщг→you");
        result = corrector.decide("вщ");
        expect(result.action == smarttype::Action::replace && result.candidate == "do",
               "decide layout reverse вщ→do");
    }

    // ST-023 / ST-024 protections
    {
        result = corrector.decide("happ.info");
        expect(result.action == smarttype::Action::keep, "ST-024 domain happ.info kept");
        result = corrector.decide("автоплатежей");
        expect(result.action != smarttype::Action::replace ||
                   result.candidate.find(' ') == std::string::npos,
               "ST-023 no auto-split compound автоплатежей");
    }

    corrector.add_rule({"севодня", "не менять", 0.80, std::nullopt});
    result = corrector.decide("севодня");
    expect(result.action == smarttype::Action::suggest && result.candidate == "не менять",
           "personal feedback overrides a built-in rule");
    smarttype::Corrector grammar_corrector;
    result = grammar_corrector.decide("течении", {"в"});
    expect(result.action == smarttype::Action::suggest && result.candidate == "течение",
           "contextual grammar is suggestion-only");
    result = grammar_corrector.decide("ихний");
    expect(result.action == smarttype::Action::suggest && result.candidate == "их",
           "semantic correction is suggestion-only");

    smarttype::PersonalStore store(":memory:");
    store.add_word("Remnawave");
    store.add_rule("ашипка", "ошибка", 0.99, "моя");
    expect(store.words().size() == 1 && store.words().front() == "remnawave", "personal word persists");
    store.remove_word("Remnawave");
    expect(store.words().empty(), "personal word can be removed");
    store.add_word("Remnawave");
    expect(store.rules().size() == 1 && store.rules().front().previous_word == "моя", "personal rule persists");
    store.record_undo("ашипка", "ошибка", "моя");
    expect(store.undo_count("ашипка", "ошибка") == 1, "undo feedback persists");
    store.record_undo("ашипка", "ошибка", "моя");
    store.record_undo("ашипка", "ошибка", "моя");
    expect(store.should_demote_correction("ашипка", "ошибка", "моя"),
           "repeated context-specific undos demote auto correction");
    store.record_undo("контекст", "контакт", "один");
    store.record_undo("контекст", "контакт", "два");
    store.record_undo("контекст", "контакт", "три");
    expect(store.should_demote_correction("контекст", "контакт", "четыре"),
           "rejections across contexts demote the same correction pair");
    store.add_rule("и", "а", 0.99);
    store.add_learned_rule("и", "включите", 0.99);
    const auto safe_rules = store.rules();
    expect(std::none_of(safe_rules.begin(), safe_rules.end(), [](const auto& rule) {
               return rule.typo == "и";
           }), "single-character correction rules cannot be learned");
    for (int index = 0; index < 5; ++index) {
        store.record_accept("частая", "часто", "");
    }
    store.record_undo("частая", "часто", "");
    store.record_undo("частая", "часто", "");
    store.record_undo("частая", "часто", "");
    expect(!store.should_demote_correction("частая", "часто", ""),
           "accepted correction survives occasional undos");
    expect(store.mode() == "normal", "normal mode is the default");
    store.set_mode("cautious");
    expect(store.mode() == "cautious", "correction mode persists");
    expect(store.setting_enabled("sentence_capitalization"),
           "sentence capitalization defaults to enabled");
    store.set_setting("sentence_capitalization", false);
    expect(!store.setting_enabled("sentence_capitalization"),
           "boolean setting persists");
    store.set_string_setting("current_app", "telegram-desktop");
    expect(store.string_setting("current_app") == "telegram-desktop", "string setting persists");
    store.add_history("севодня", "сегодня", "test-app", "automatic");
    auto correction_history = store.history();
    expect(correction_history.size() == 1 && correction_history[0].original == "севодня" &&
               !correction_history[0].undone,
           "correction history persists");
    store.mark_last_history_undone("севодня", "сегодня");
    expect(store.history()[0].undone, "history records undo");
    store.disable_rule("севодня", "сегодня");
    expect(store.is_rule_disabled("севодня", "сегодня"), "disabled correction applies immediately");
    expect(!store.disabled_rules().empty(), "disabled corrections can be listed");
    store.enable_rule("севодня", "сегодня");
    expect(!store.is_rule_disabled("севодня", "сегодня"), "disabled correction can be restored");
    store.add_diagnostic("севодня", "сегодня", "replace", "test reason", 0.99, "test-app");
    expect(store.diagnostics().size() == 1 && store.diagnostics()[0].reason == "test reason",
           "diagnostic reason persists");
    store.clear_diagnostics();
    expect(store.diagnostics().empty(), "diagnostics can be cleared");

    // Word transitions (bigrams)
    store.add_transition("привет", "мир");
    store.add_transition("привет", "мир");
    store.add_transition("привет", "всем");
    auto next = store.next_words("привет");
    expect(next.size() == 2 && next[0] == "мир" && next[1] == "всем", "bigram transitions persistent");
    expect(store.get_transition_use_count("привет", "мир") == 2, "transition use count correct");
    expect(store.get_transition_use_count("привет", "всем") == 1, "transition use count correct");
    expect(store.get_word_use_count("remnawave") == 1, "word use count correct");

    // Unknown input must not poison the personal dictionary after one typo.
    expect(!store.observe_unknown_word("Квазислово"), "unknown word not learned on first sight");
    expect(!store.observe_unknown_word("квазислово"), "unknown word not learned on second sight");
    expect(store.get_word_use_count("квазислово") == 0, "unconfirmed word absent from dictionary");
    expect(store.observe_unknown_word("квазислово"), "unknown word promoted on third sight");
    expect(store.get_word_use_count("квазислово") == 1, "promoted word stored once");
    expect(!store.observe_unknown_word("квазислово"), "promotion happens only once");
    expect(store.get_word_use_count("квазислово") == 1, "later observations do not inflate use count");

    // Corrector ranking with store
    // Если мы наберем слово "здраствуйте" с ошибкой, Hunspell вернет варианты, один из которых "здравствуйте" (dist=1)
    // Без store и с ним он должен отдать правильного кандидата
    result = corrector.decide("здраствуйте", {}, &store);
    expect(result.action == smarttype::Action::replace && result.candidate == "здравствуйте", "ranked suggestion matches with store");

    const auto log_path = std::filesystem::temp_directory_path() / "smarttype-test-decisions.jsonl";
    std::filesystem::remove(log_path);
    smarttype::DecisionLog log(log_path);
    smarttype::Corrector logger_corrector;
    const auto logged_decision = logger_corrector.decide("севодня");
    log.write("севодня", logged_decision);
    std::ifstream log_file(log_path);
    const std::string log_contents((std::istreambuf_iterator<char>(log_file)), {});
    expect(log_contents.find("севодня") == std::string::npos, "decision log contains no raw token");
    expect(log_contents.find("\"action\":\"replace\"") != std::string::npos, "decision log has metadata");
    expect((std::filesystem::status(log_path).permissions() & std::filesystem::perms::group_read) ==
               std::filesystem::perms::none,
           "decision log is not group-readable");
    std::filesystem::remove(log_path);

    // Case preservation tests
    expect(smarttype::preserve_case("Яндекс", "яндекс") == "Яндекс", "preserve case for Я");
    expect(smarttype::preserve_case("Ёлка", "ёлка") == "Ёлка", "preserve case for Ё");
    expect(smarttype::preserve_case("Яблоко", "яблоко") == "Яблоко", "preserve case for Яблоко");
    expect(smarttype::preserve_case("Привет", "привет") == "Привет", "preserve case for Привет");
    expect(smarttype::lowercase_ru("ПРИВЕТ") == "привет", "lowercase_ru Cyrillic uppercase");
    expect(smarttype::lowercase_ru("GHBDTN") == "ghbdtn", "lowercase_ru ASCII uppercase");
    expect(smarttype::preserve_case("СЕВОДНЯ", "сегодня") == "СЕГОДНЯ", "preserve Cyrillic caps lock");
    expect(smarttype::preserve_case("HELLO", "world") == "WORLD", "preserve ASCII caps lock");
    expect(smarttype::preserve_case("ЁЛКА", "ёлочка") == "ЁЛОЧКА", "preserve uppercase Ё");
    // Test теливзор correction to телевизор (distance 2 on long word)
    result = corrector.decide("теливзор");
    expect(result.action == smarttype::Action::replace && result.candidate == "телевизор", "auto replace теливзор with телевизор");
    result = corrector.decide("теливизора");
    expect(result.action == smarttype::Action::replace && result.candidate == "телевизора",
           "dictionary corrects an inflected word form");
    result = corrector.decide("теливизорами");
    expect(result.action == smarttype::Action::replace && result.candidate == "телевизорами",
           "dictionary corrects plural instrumental form");
    result = corrector.decide("computr");
    expect(result.action == smarttype::Action::suggest && (result.candidate == "compute" || result.candidate == "computer"), "suggest english spelling correction");
    // Emoji suggestions tests
    auto candidates = corrector.get_candidates("привет");
    expect(std::find(candidates.begin(), candidates.end(), "👋") != candidates.end(), "emoji suggested for привет");
    candidates = corrector.get_candidates("Умпылда");
    expect(!candidates.empty() && candidates.size() <= 3 && candidates.back() == "Умпылда",
           "the exact unknown spelling is the final candidate");

    result = corrector.decide("ПРИвет");
    expect(result.action == smarttype::Action::replace && result.candidate == "Привет",
           "accidental mixed Cyrillic case normalized");
    result = corrector.decide("ЗДраВствуйте");
    expect(result.action == smarttype::Action::replace && result.candidate == "Здравствуйте",
           "multiple accidental capitals normalized");
    result = corrector.decide("ПРИВЕТ");
    expect(result.action == smarttype::Action::keep, "intentional all caps preserved");

    // Blacklist database tests
    store.blacklist_add("MyUniqueEditor");
    auto blacklist = store.blacklist_get();
    expect(std::find(blacklist.begin(), blacklist.end(), "MyUniqueEditor") != blacklist.end(), "app added to blacklist");
    store.blacklist_remove("MyUniqueEditor");
    blacklist = store.blacklist_get();
    expect(std::find(blacklist.begin(), blacklist.end(), "MyUniqueEditor") == blacklist.end(), "app removed from blacklist");
    store.blacklist_add("code");
    expect(store.is_app_blacklisted("code"), "exact executable is blacklisted");
    expect(store.is_app_blacklisted("com.visualstudio.code"), "desktop id component is blacklisted");
    expect(!store.is_app_blacklisted("codec-player"), "blacklist does not match arbitrary substring");

    // Settings tests
    store.set_string_setting("layout_mode", "auto");
    expect(store.string_setting("layout_mode", "suggest") == "auto", "string setting gets and sets correctly");
    store.set_setting("layout_correction", false);
    expect(store.setting_enabled("layout_correction", true) == false, "boolean setting gets and sets correctly");

    store.add_learned_word("Автообучение");
    store.add_learned_rule("учебнаяа", "учебная", 0.99, "моя");
    auto all_words = store.all_words();
    expect(std::find(all_words.begin(), all_words.end(), "автообучение") != all_words.end(),
           "learned word is available to corrector");
    store.reset_learning();
    auto explicit_words = store.words();
    expect(std::find(explicit_words.begin(), explicit_words.end(), "remnawave") != explicit_words.end(),
           "reset learning preserves explicit dictionary");
    all_words = store.all_words();
    expect(std::find(all_words.begin(), all_words.end(), "автообучение") == all_words.end(),
           "reset learning removes learned words");

    // Layout translation tests
    expect(smarttype::translate_layout("ghbdtn") == "привет", "translate qwerty to jcuken");
    expect(smarttype::translate_layout("руддщ") == "hello", "translate jcuken to qwerty");
    expect(smarttype::translate_layout("Ghbdtn") == "Привет", "translate layout preserves capitalization");
    expect(smarttype::translate_layout("ghbdtn/") == "привет.", "translate slash to dot");

    // Affix-only Russian surface forms must stay valid (not false RU→EN layout flip).
    expect(corrector.is_dictionary_word("здравствуй"), "здравствуй is a dictionary word");
    expect(corrector.is_dictionary_word("Здравствуй"), "Здравствуй is a dictionary word");
    expect(corrector.is_prefix_valid("здравствуй", true),
           "здравствуй is a valid RU prefix/surface form");
    expect(corrector.is_prefix_valid("Здравствуй", true),
           "Здравствуй is a valid RU prefix/surface form");
    {
        const auto decision = corrector.decide("Здравствуй", {}, nullptr);
        expect(decision.action == smarttype::Action::keep ||
                   (decision.action != smarttype::Action::replace ||
                    decision.reason.find("layout") == std::string::npos),
               "Здравствуй must not be layout-corrected to Latin");
        expect(decision.candidate.find('P') == std::string::npos &&
                   decision.candidate.find('p') == std::string::npos,
               "Здравствуй candidate stays Cyrillic");
    }
    {
        // Real reverse case still works: Cyrillic gibberish → English word.
        const auto decision = corrector.decide("руддщ", {}, nullptr);
        expect(decision.action == smarttype::Action::replace && decision.candidate == "hello",
               "руддщ still layout-corrects to hello");
    }
    {
        // Phrase anchor: last word must be a hard layout replace so Space can
        // also rewrite preceding short tokens ("F ns " → "А ты ").
        const auto decision = corrector.decide("xnj", {}, nullptr);
        expect(decision.action == smarttype::Action::replace && decision.candidate == "что" &&
                   decision.reason == "layout correction",
               "xnj layout-corrects to что (dict-backed)");
        expect(corrector.is_dictionary_word("что"), "что is dictionary word");
    }
    {
        // Personal Latin gibberish must NOT freeze layout auto (undo of proactive
        // used to add "xnj" as a personal word → keep forever).
        corrector.add_personal_word("xnj");
        const auto decision = corrector.decide("xnj", {}, nullptr);
        expect(decision.action == smarttype::Action::replace && decision.candidate == "что" &&
                   decision.reason == "layout correction",
               "personal xnj still layout-corrects to что");
    }

    std::cout << "all core tests passed\n";
}
