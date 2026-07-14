#include "smarttype/corrector.hpp"
#include "smarttype/text.hpp"
#include "smarttype/personal_store.hpp"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <iconv.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace smarttype {
class SpellChecker {
public:
    SpellChecker() {
        library_ = dlopen("libhunspell-1.7.so.0", RTLD_LAZY | RTLD_LOCAL);
        if (!library_) return;
        create_ = reinterpret_cast<Create>(dlsym(library_, "Hunspell_create"));
        destroy_ = reinterpret_cast<Destroy>(dlsym(library_, "Hunspell_destroy"));
        spell_ = reinterpret_cast<Spell>(dlsym(library_, "Hunspell_spell"));
        suggest_ = reinterpret_cast<Suggest>(dlsym(library_, "Hunspell_suggest"));
        free_list_ = reinterpret_cast<FreeList>(dlsym(library_, "Hunspell_free_list"));
        if (!create_ || !destroy_ || !spell_ || !suggest_ || !free_list_) return;

        std::filesystem::path directory = "/usr/share/hunspell";
        if (const char* custom = std::getenv("SMARTTYPE_HUNSPELL_DIR"); custom && *custom) directory = custom;

        const auto aff_ru = directory / "ru_RU.aff";
        const auto dic_ru = directory / "ru_RU.dic";
        if (std::filesystem::exists(aff_ru) && std::filesystem::exists(dic_ru)) {
            encoding_ru_ = detect_encoding(aff_ru);
            handle_ru_ = create_(aff_ru.c_str(), dic_ru.c_str());
        }

        const auto aff_en = directory / "en_US.aff";
        const auto dic_en = directory / "en_US.dic";
        if (std::filesystem::exists(aff_en) && std::filesystem::exists(dic_en)) {
            handle_en_ = create_(aff_en.c_str(), dic_en.c_str());
        }
        load_prefix_lists(directory);
    }

    ~SpellChecker() {
        if (handle_ru_ && destroy_) destroy_(handle_ru_);
        if (handle_en_ && destroy_) destroy_(handle_en_);
        if (library_) dlclose(library_);
    }

    bool available() const { return handle_ru_ != nullptr; }
    bool available_en() const { return handle_en_ != nullptr; }

    bool contains(const std::string& word) const {
        const auto encoded = convert_to_dic(word);
        return handle_ru_ && encoded && spell_(handle_ru_, encoded->c_str()) != 0;
    }

    bool contains_en(const std::string& word) const {
        return handle_en_ && spell_(handle_en_, word.c_str()) != 0;
    }

    std::vector<std::string> suggest(const std::string& word) const {
        if (!handle_ru_) return {};
        const auto encoded = convert_to_dic(word);
        if (!encoded) return {};
        char** suggestions = nullptr;
        const int count = suggest_(handle_ru_, &suggestions, encoded->c_str());
        std::vector<std::string> result;
        if (count > 0 && suggestions) {
            for (int i = 0; i < count; ++i) {
                if (suggestions[i]) {
                    auto converted = convert_from_dic(suggestions[i]);
                    if (converted) result.push_back(std::move(*converted));
                }
            }
        }
        if (suggestions) free_list_(handle_ru_, &suggestions, count);
        return result;
    }

    std::vector<std::string> suggest_en(const std::string& word) const {
        if (!handle_en_) return {};
        char** suggestions = nullptr;
        const int count = suggest_(handle_en_, &suggestions, word.c_str());
        std::vector<std::string> result;
        if (count > 0 && suggestions) {
            for (int i = 0; i < count; ++i) {
                if (suggestions[i]) {
                    result.push_back(suggestions[i]);
                }
            }
        }
        if (suggestions) free_list_(handle_en_, &suggestions, count);
        return result;
    }

    bool is_prefix_valid(std::string_view prefix, bool ru) const {
        const auto& list = ru ? ru_prefix_list_ : en_prefix_list_;
        std::string norm(prefix);
        if (ru) {
            norm = lowercase_ru(prefix);
        } else {
            std::transform(norm.begin(), norm.end(), norm.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }
        auto it = std::lower_bound(list.begin(), list.end(), norm);
        if (it != list.end() && it->compare(0, norm.size(), norm) == 0) {
            return true;
        }
        // .dic stem lists miss affix-only surface forms (e.g. "здравствуй" is not a
        // prefix of "здравствующий"/"здравствовать", but hunspell accepts it).
        if (ru) {
            return available() && contains(norm);
        }
        return available_en() && contains_en(norm);
    }

    void load_prefix_lists(const std::filesystem::path& directory) {
        // Load Russian
        const auto dic_ru = directory / "ru_RU.dic";
        if (std::filesystem::exists(dic_ru)) {
            std::ifstream file(dic_ru);
            if (file.is_open()) {
                std::string line;
                // Skip the first line (count)
                std::getline(file, line);
                while (std::getline(file, line)) {
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                    if (line.empty()) continue;
                    auto slash = line.find('/');
                    std::string word = (slash == std::string::npos) ? line : line.substr(0, slash);
                    if (word.empty()) continue;
                    
                    auto converted = convert_from_dic(word);
                    if (converted) {
                        ru_prefix_list_.push_back(lowercase_ru(*converted));
                    }
                }
            }
        }
        std::sort(ru_prefix_list_.begin(), ru_prefix_list_.end());
        ru_prefix_list_.erase(std::unique(ru_prefix_list_.begin(), ru_prefix_list_.end()), ru_prefix_list_.end());

        // Load English
        const auto dic_en = directory / "en_US.dic";
        if (std::filesystem::exists(dic_en)) {
            std::ifstream file(dic_en);
            if (file.is_open()) {
                std::string line;
                // Skip count
                std::getline(file, line);
                while (std::getline(file, line)) {
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                    if (line.empty()) continue;
                    auto slash = line.find('/');
                    std::string word = (slash == std::string::npos) ? line : line.substr(0, slash);
                    if (word.empty()) continue;
                    
                    std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return std::tolower(c); });
                    en_prefix_list_.push_back(word);
                }
            }
        }
        std::sort(en_prefix_list_.begin(), en_prefix_list_.end());
        en_prefix_list_.erase(std::unique(en_prefix_list_.begin(), en_prefix_list_.end()), en_prefix_list_.end());
    }

private:
    std::string detect_encoding(const std::filesystem::path& aff_path) {
        std::ifstream file(aff_path);
        if (!file.is_open()) return "UTF-8";
        std::string line;
        while (std::getline(file, line)) {
            if (line.compare(0, 4, "SET ") == 0) {
                std::string enc = line.substr(4);
                while (!enc.empty() && (enc.back() == '\r' || std::isspace(static_cast<unsigned char>(enc.back())))) {
                    enc.pop_back();
                }
                return enc;
            }
        }
        return "UTF-8";
    }

    std::optional<std::string> convert_to_dic(const std::string& word) const {
        if (encoding_ru_ == "UTF-8") return word;
        return convert(word, encoding_ru_.c_str(), "UTF-8");
    }

    std::optional<std::string> convert_from_dic(const std::string& word) const {
        if (encoding_ru_ == "UTF-8") return word;
        return convert(word, "UTF-8", encoding_ru_.c_str());
    }

    static std::optional<std::string> convert(const std::string& input, const char* to, const char* from) {
        iconv_t converter = iconv_open(to, from);
        if (converter == reinterpret_cast<iconv_t>(-1)) return std::nullopt;
        std::string output(input.size() * 4 + 16, '\0');
        char* source = const_cast<char*>(input.data());
        std::size_t source_size = input.size();
        char* destination = output.data();
        std::size_t destination_size = output.size();
        const std::size_t status = iconv(converter, &source, &source_size, &destination, &destination_size);
        iconv_close(converter);
        if (status == static_cast<std::size_t>(-1)) return std::nullopt;
        output.resize(output.size() - destination_size);
        return output;
    }

    using Create = void* (*)(const char*, const char*);
    using Destroy = void (*)(void*);
    using Spell = int (*)(void*, const char*);
    using Suggest = int (*)(void*, char***, const char*);
    using FreeList = void (*)(void*, char***, int);
    void* library_{nullptr};
    void* handle_ru_{nullptr};
    void* handle_en_{nullptr};
    Create create_{nullptr};
    Destroy destroy_{nullptr};
    Spell spell_{nullptr};
    Suggest suggest_{nullptr};
    FreeList free_list_{nullptr};
    std::string encoding_ru_{"UTF-8"};
    std::vector<std::string> ru_prefix_list_;
    std::vector<std::string> en_prefix_list_;
};

namespace {

std::vector<Rule> initial_rules() {
    return {
        {"севодня", "сегодня", 0.99, std::nullopt},
        {"рабоать", "работать", 0.99, std::nullopt},
        {"вопще", "вообще", 0.98, std::nullopt},
        {"дамой", "домой", 0.98, std::nullopt},
        {"прийдет", "придёт", 0.96, std::nullopt},
        {"всетаки", "всё-таки", 0.96, std::nullopt},
        {"потомучто", "потому что", 0.96, std::nullopt},
        {"незнаю", "не знаю", 0.97, "я"},
        {"вопщем", "в общем", 0.90, std::nullopt},
        {"течении", "течение", 0.80, "в"},
        {"ихний", "их", 0.75, std::nullopt},
    };
}

bool has_ascii_letter(std::string_view value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    });
}

std::string qwerty_to_cyrillic(std::string_view english_text) {
    static const std::unordered_map<char, std::string> map = {
        {'q', "й"}, {'w', "ц"}, {'e', "у"}, {'r', "к"}, {'t', "е"}, {'y', "н"}, {'u', "г"}, {'i', "ш"}, {'o', "щ"}, {'p', "з"}, {'[', "х"}, {']', "ъ"},
        {'a', "ф"}, {'s', "ы"}, {'d', "в"}, {'f', "а"}, {'g', "п"}, {'h', "р"}, {'j', "о"}, {'k', "л"}, {'l', "д"}, {';', "ж"}, {'\'', "э"},
        {'z', "я"}, {'x', "ч"}, {'c', "с"}, {'v', "м"}, {'b', "и"}, {'n', "т"}, {'m', "ь"}, {',', "б"}, {'.', "ю"}, {'/', "."},
        {'Q', "Й"}, {'W', "Ц"}, {'E', "У"}, {'R', "К"}, {'T', "Е"}, {'Y', "Н"}, {'U', "Г"}, {'I', "Ш"}, {'O', "Щ"}, {'P', "З"}, {'{', "Х"}, {'}', "Ъ"},
        {'A', "Ф"}, {'S', "Ы"}, {'D', "В"}, {'F', "А"}, {'G', "П"}, {'H', "Р"}, {'J', "О"}, {'K', "Л"}, {'L', "Д"}, {':', "Ж"}, {'"', "Э"},
        {'Z', "Я"}, {'X', "Ч"}, {'C', "С"}, {'V', "М"}, {'B', "И"}, {'N', "Т"}, {'M', "Ь"}, {'<', "Б"}, {'>', "Ю"}, {'?', ","},
        {'~', "Ё"}, {'`', "ё"}, {'@', "\""}, {'#', "№"}, {'$', ";"}, {'%', "%"}, {'^', ":"}, {'&', "?"}, {'*', "*"}
    };

    std::string result;
    for (char c : english_text) {
        auto it = map.find(c);
        if (it != map.end()) {
            result += it->second;
        } else {
            result += c;
        }
    }
    return result;
}

bool is_qwerty_only(std::string_view token) {
    if (token.empty()) return false;
    for (char c : token) {
        if (static_cast<unsigned char>(c) >= 0x80) return false;
    }
    return true;
}

std::u32string utf8_to_utf32(std::string_view utf8) {
    std::u32string result;
    result.reserve(utf8.size());
    for (std::size_t i = 0; i < utf8.size(); ) {
        uint32_t cp = 0;
        unsigned char c = utf8[i];
        std::size_t len = 0;
        if (c < 0x80) { cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { cp = c; len = 1; }
        
        if (i + len > utf8.size()) break;
        for (std::size_t j = 1; j < len; ++j) {
            cp = (cp << 6) | (utf8[i + j] & 0x3F);
        }
        result.push_back(cp);
        i += len;
    }
    return result;
}

std::optional<std::string> cyrillic_to_qwerty(std::string_view text) {
    static const std::unordered_map<char32_t, char> map = {
        {U'й','q'},{U'ц','w'},{U'у','e'},{U'к','r'},{U'е','t'},{U'н','y'},
        {U'г','u'},{U'ш','i'},{U'щ','o'},{U'з','p'},{U'х','['},{U'ъ',']'},
        {U'ф','a'},{U'ы','s'},{U'в','d'},{U'а','f'},{U'п','g'},{U'р','h'},
        {U'о','j'},{U'л','k'},{U'д','l'},{U'ж',';'},{U'э','\''},
        {U'я','z'},{U'ч','x'},{U'с','c'},{U'м','v'},{U'и','b'},{U'т','n'},
        {U'ь','m'},{U'б',','},{U'ю','.'},{U'ё','`'}
    };
    std::string result;
    for (const auto cp : utf8_to_utf32(text)) {
        const bool upper = (cp >= U'А' && cp <= U'Я') || cp == U'Ё';
        char32_t lower = cp;
        if (cp >= U'А' && cp <= U'Я') lower += 0x20;
        else if (cp == U'Ё') lower = U'ё';
        const auto iterator = map.find(lower);
        if (iterator == map.end()) return std::nullopt;
        char value = iterator->second;
        if (upper && value >= 'a' && value <= 'z') value = static_cast<char>(value - 32);
        result.push_back(value);
    }
    return result.empty() ? std::nullopt : std::optional<std::string>(result);
}

double keyboard_proximity_bonus(const std::u32string& typed, const std::u32string& candidate) {
    if (typed.size() != candidate.size()) return 0.0;
    std::size_t mismatch = typed.size();
    int count = 0;
    for (std::size_t index = 0; index < typed.size(); ++index) {
        if (typed[index] != candidate[index]) {
            mismatch = index;
            ++count;
        }
    }
    if (count != 1) return 0.0;
    constexpr std::u32string_view rows[] = {
        U"qwertyuiop", U"asdfghjkl", U"zxcvbnm",
        U"йцукенгшщзхъ", U"фывапролджэ", U"ячсмитьбю"
    };
    auto locate = [&](char32_t value) -> std::pair<int, int> {
        for (int row = 0; row < 6; ++row) {
            const auto column = rows[row].find(value);
            if (column != std::u32string_view::npos) return {row % 3, static_cast<int>(column)};
        }
        return {-10, -10};
    };
    const auto left = locate(typed[mismatch]);
    const auto right = locate(candidate[mismatch]);
    if (left.first < 0 || right.first < 0) return 0.0;
    return std::abs(left.first - right.first) <= 1 &&
                   std::abs(left.second - right.second) <= 1 ? 10.0 : -3.0;
}

std::size_t damerau_levenshtein_distance(const std::u32string& s1, const std::u32string& s2) {
    std::size_t l1 = s1.size();
    std::size_t l2 = s2.size();
    if (l1 == 0) return l2;
    if (l2 == 0) return l1;

    std::vector<std::vector<std::size_t>> d(l1 + 2, std::vector<std::size_t>(l2 + 2, 0));
    std::size_t max_dist = l1 + l2;
    d[0][0] = max_dist;
    for (std::size_t i = 0; i <= l1; ++i) {
        d[i + 1][0] = max_dist;
        d[i + 1][1] = i;
    }
    for (std::size_t j = 0; j <= l2; ++j) {
        d[0][j + 1] = max_dist;
        d[1][j + 1] = j;
    }

    std::unordered_map<uint32_t, std::size_t> da;
    for (std::size_t i = 1; i <= l1; ++i) {
        std::size_t db = 0;
        for (std::size_t j = 1; j <= l2; ++j) {
            std::size_t k = da[s2[j - 1]];
            std::size_t l = db;
            std::size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            if (cost == 0) db = j;

            d[i + 1][j + 1] = std::min({
                d[i][j + 1] + 1,
                d[i + 1][j] + 1,
                d[i][j] + cost,
                d[k][l] + (i - k - 1) + 1 + (j - l - 1)
            });
        }
        da[s1[i - 1]] = i;
    }
    return d[l1 + 1][l2 + 1];
}

}  // namespace

Corrector::Corrector() : rules_(initial_rules()), spell_checker_(std::make_unique<SpellChecker>()) {}
Corrector::Corrector(std::vector<Rule> rules)
    : rules_(std::move(rules)), spell_checker_(std::make_unique<SpellChecker>()) {}
Corrector::~Corrector() = default;

bool Corrector::is_strongly_protected(std::string_view token) {
    if (token.empty()) return true;
    if (token.find("://") != std::string_view::npos || token.find('@') != std::string_view::npos ||
        token.find('/') != std::string_view::npos || token.find('\\') != std::string_view::npos ||
        token.find('=') != std::string_view::npos || token.front() == '#' || token.front() == '$') {
        return true;
    }
    // ST-024: domain / dotted identifiers (happ.info, file.tar.gz) — never autocorrect.
    {
        const auto dot = token.find('.');
        if (dot != std::string_view::npos && dot > 0 && dot + 1 < token.size()) {
            const bool left_alnum = std::isalnum(static_cast<unsigned char>(token[dot - 1]));
            const bool right_alnum = std::isalnum(static_cast<unsigned char>(token[dot + 1]));
            if (left_alnum && right_alnum) {
                return true;
            }
        }
    }
    const bool has_digit = std::any_of(token.begin(), token.end(), [](unsigned char c) {
        return c >= '0' && c <= '9';
    });
    if (has_digit && has_ascii_letter(token)) return true;
    
    static const std::unordered_set<std::string_view> tech_terms = {
        "git", "cmake", "ninja", "gcc", "g++", "clang", "python", "rust", "go",
        "docker", "kubernetes", "k8s", "html", "css", "js", "json", "yaml", "xml",
        "sql", "sqlite", "api", "url", "ip", "http", "https", "ssh", "bash", "sh",
        "zsh", "fish", "linux", "fedora", "ubuntu", "debian", "arch", "kde", "gnome",
        "wayland", "x11", "fcitx", "ibus", "qt", "gtk", "flatpak", "snap", "systemd",
        "systemctl", "journalctl", "dbus", "github", "gitlab", "cpp", "happ",
        "sudo", "dnf", "vless", "vpn", "rkn"
    };

    std::string lower_token;
    lower_token.reserve(token.size());
    for (char c : token) lower_token.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (tech_terms.count(lower_token) > 0) {
        return true;
    }

    const bool has_non_ascii = std::any_of(token.begin(), token.end(), [](unsigned char c) { return c >= 0x80; });
    return has_ascii_letter(token) && has_non_ascii;
}

bool Corrector::is_protected(std::string_view token) {
    if (is_strongly_protected(token)) return true;
    
    bool has_upper = false;
    bool has_lower = false;
    bool interior_ascii_upper = false;
    for (std::size_t index = 0; index < token.size(); ++index) {
        const auto byte = static_cast<unsigned char>(token[index]);
        if (byte >= 'A' && byte <= 'Z') {
            has_upper = true;
            interior_ascii_upper = interior_ascii_upper || index > 0;
        } else if (byte >= 'a' && byte <= 'z') {
            has_lower = true;
        } else if (byte == 0xD0 && index + 1 < token.size()) {
            const auto next = static_cast<unsigned char>(token[index + 1]);
            if ((next >= 0x90 && next <= 0xAF) || next == 0x81) has_upper = true;
            else if (next >= 0xB0 && next <= 0xBF) has_lower = true;
            ++index;
        } else if (byte == 0xD1 && index + 1 < token.size()) {
            const auto next = static_cast<unsigned char>(token[index + 1]);
            if ((next >= 0x80 && next <= 0x8F) || next == 0x91) has_lower = true;
            ++index;
        }
    }
    if (has_upper && !has_lower && utf8_to_utf32(token).size() > 1) return true;
    if (interior_ascii_upper && has_lower) return true;
    return false;
}

void Corrector::add_rule(Rule rule) { rules_.insert(rules_.begin(), std::move(rule)); }

void Corrector::add_personal_word(std::string word) {
    personal_words_.push_back(lowercase_ru(word));
}

bool Corrector::is_prefix_valid(std::string_view prefix, bool ru) const {
    if (!spell_checker_) return false;
    std::string norm(prefix);
    if (ru) {
        norm = lowercase_ru(prefix);
    } else {
        std::transform(norm.begin(), norm.end(), norm.begin(), [](unsigned char c) { return std::tolower(c); });
    }
    for (const auto& w : personal_words_) {
        if (w.compare(0, norm.size(), norm) == 0) {
            return true;
        }
    }
    return spell_checker_->is_prefix_valid(prefix, ru);
}

bool Corrector::is_dictionary_word(std::string_view word) const {
    if (!spell_checker_) return false;
    const std::string normalized = lowercase_ru(word);
    const bool is_english = is_qwerty_only(word) && has_ascii_letter(word);
    if (is_english) {
        return spell_checker_->available_en() && spell_checker_->contains_en(normalized);
    } else {
        return spell_checker_->available() && spell_checker_->contains(normalized);
    }
}

namespace {

bool is_short_russian_particle(std::string_view normalized) {
    static const std::unordered_set<std::string_view> kParticles = {
        "а", "и", "в", "о", "с", "у", "я", "к", "ж", "б", "н", "п", "д", "т", "м",
        "ты", "не", "но", "да", "же", "бы", "ли", "ну", "ой", "эх", "вы",
        "мы", "он", "то", "ни", "уж", "по", "за", "из", "от", "до", "об", "ко",
    };
    return kParticles.contains(normalized);
}

bool is_short_english_particle(std::string_view normalized) {
    static const std::unordered_set<std::string_view> kParticles = {
        "a", "i", "o", "u", "to", "of", "in", "on", "at", "is", "it", "or", "an",
        "be", "as", "we", "he", "me", "my", "do", "no", "so", "if", "by", "up",
    };
    return kParticles.contains(normalized);
}

}  // namespace

std::optional<std::string> Corrector::try_layout_retranslate(std::string_view word,
                                                            bool to_russian) const {
    if (word.empty() || is_strongly_protected(word)) {
        return std::nullopt;
    }

    if (to_russian) {
        if (!is_qwerty_only(word) || !has_ascii_letter(word)) {
            return std::nullopt;
        }
        const std::string normalized = lowercase_ru(word);
        const auto src_len = utf8_to_utf32(normalized).size();
        // Intentional English — leave alone. Single letters like "f" may exist in
        // hunspell; only treat as English if they are known particles or longer words.
        if (is_short_english_particle(normalized)) {
            return std::nullopt;
        }
        if (src_len >= 2 && spell_checker_ && spell_checker_->available_en() &&
            spell_checker_->contains_en(normalized)) {
            return std::nullopt;
        }

        const auto cyrillic = qwerty_to_cyrillic(word);
        const auto cyr_norm = lowercase_ru(cyrillic);
        if (std::find(personal_words_.begin(), personal_words_.end(), cyr_norm) !=
            personal_words_.end()) {
            return preserve_case(word, cyrillic);
        }
        if (spell_checker_ && spell_checker_->available() &&
            spell_checker_->contains(cyr_norm)) {
            return preserve_case(word, cyrillic);
        }
        if (is_short_russian_particle(cyr_norm)) {
            return preserve_case(word, cyrillic);
        }
        return std::nullopt;
    }

    // Reverse: Cyrillic gibberish → English.
    const auto english = cyrillic_to_qwerty(word);
    if (!english) {
        return std::nullopt;
    }
    const std::string normalized = lowercase_ru(word);
    const auto src_len = utf8_to_utf32(normalized).size();
    // Real Russian words stay. Single letters like "ш" (→"i") are not dictionary
    // "words" in the particle sense — allow remap to English particles/letters.
    if (src_len >= 2 && is_short_russian_particle(normalized)) {
        return std::nullopt;
    }
    if (src_len >= 2 && spell_checker_ && spell_checker_->available() &&
        spell_checker_->contains(normalized)) {
        return std::nullopt;
    }
    // len==1: only block if this cyrillic letter is a common intentional particle
    // (а/и/в/…) that is NOT a clear EN layout typo. "ш"/"щ"/"б" map to i/o/, and
    // should retranslate when the rest of the phrase is EN.
    if (src_len == 1 && is_short_russian_particle(normalized)) {
        const auto eng_norm_probe = lowercase_ru(*english);
        if (!is_short_english_particle(eng_norm_probe) &&
            !(spell_checker_ && spell_checker_->available_en() &&
              spell_checker_->contains_en(eng_norm_probe))) {
            return std::nullopt;
        }
    }
    const auto eng_norm = lowercase_ru(*english);
    if (spell_checker_ && spell_checker_->available_en() &&
        spell_checker_->contains_en(eng_norm)) {
        return *english;
    }
    if (is_short_english_particle(eng_norm)) {
        return *english;
    }
    // Single Latin letter targets (i, o, …) after reverse map.
    if (src_len == 1 && eng_norm.size() == 1 &&
        eng_norm[0] >= 'a' && eng_norm[0] <= 'z') {
        return *english;
    }
    return std::nullopt;
}

std::optional<std::string> Corrector::normalize_accidental_case(std::string_view word) const {
    if (word.empty()) return std::nullopt;
    const auto normalized = lowercase_ru(word);
    if (std::find(personal_words_.begin(), personal_words_.end(), normalized) != personal_words_.end()) {
        return std::nullopt;
    }

    const auto letters = utf8_to_utf32(word);
    int upper = 0;
    int lower = 0;
    bool first_upper = false;
    for (std::size_t index = 0; index < letters.size(); ++index) {
        const auto cp = letters[index];
        const bool is_upper = (cp >= 0x0410 && cp <= 0x042F) || cp == 0x0401;
        const bool is_lower = (cp >= 0x0430 && cp <= 0x044F) || cp == 0x0451;
        if (!is_upper && !is_lower) return std::nullopt;
        upper += is_upper ? 1 : 0;
        lower += is_lower ? 1 : 0;
        if (index == 0) first_upper = is_upper;
    }
    if (upper == 0 || lower == 0) return std::nullopt;
    if (first_upper && upper == 1) return std::nullopt; // already Title Case
    if (!spell_checker_ || !spell_checker_->available() || !spell_checker_->contains(normalized)) {
        return std::nullopt;
    }
    return preserve_case("Аб", normalized);
}

Decision Corrector::decide(std::string_view word, const std::vector<std::string>& context, const PersonalStore* store) const {
    Decision result{Action::keep, std::string(word), std::string(word), 1.0, "no matching rule"};
    if (const auto normalized_case = normalize_accidental_case(word)) {
        return {Action::replace, std::string(word), *normalized_case, 0.99,
                "accidental mixed case"};
    }
    if (is_strongly_protected(word)) {
        result.reason = "protected token";
        return result;
    }

    const auto normalized = lowercase_ru(word);
    if (utf8_to_utf32(normalized).size() <= 1) {
        if (is_qwerty_only(word) && has_ascii_letter(word)) {
            const auto cyrillic = qwerty_to_cyrillic(word);
            const auto cyrillic_normalized = lowercase_ru(cyrillic);
            if (cyrillic_normalized == "а" || cyrillic_normalized == "и" ||
                cyrillic_normalized == "в" || cyrillic_normalized == "о" ||
                cyrillic_normalized == "с" || cyrillic_normalized == "у" ||
                cyrillic_normalized == "я" || cyrillic_normalized == "к") {
                result.action = Action::replace;
                result.candidate = preserve_case(word, cyrillic);
                result.confidence = 0.99;
                result.reason = "layout correction";
                return result;
            }
        }
        result.reason = "single-character token";
        return result;
    }
    // Clear wrong-layout retranslation runs BEFORE personal dictionary.
    // Undoing a mid-word proactive switch used to add the Latin source (e.g. "xnj")
    // as a personal word; that permanently blocked layout auto and phrase rewrite
    // while still showing "что" only as a candidate (user: F ns xnj never auto-fixes).
    if (is_qwerty_only(word) && has_ascii_letter(word)) {
        bool is_valid_english = false;
        if (spell_checker_ && spell_checker_->available_en()) {
            is_valid_english = spell_checker_->contains_en(normalized);
        }

        if (!is_valid_english) {
            const auto cyrillic = qwerty_to_cyrillic(word);
            const auto cyrillic_normalized = lowercase_ru(cyrillic);

            bool is_valid_russian = false;
            if (std::find(personal_words_.begin(), personal_words_.end(), cyrillic_normalized) !=
                personal_words_.end()) {
                is_valid_russian = true;
            } else if (spell_checker_ && spell_checker_->available()) {
                is_valid_russian = spell_checker_->contains(cyrillic_normalized);
            }

            if (is_valid_russian) {
                result.action = Action::replace;
                result.candidate = preserve_case(word, cyrillic);
                result.confidence = 0.99;
                result.reason = "layout correction";
                return result;
            }
        }
    }

    if (std::find(personal_words_.begin(), personal_words_.end(), normalized) != personal_words_.end()) {
        result.reason = "personal dictionary";
        return result;
    }
    
    // 1. Проверяем правила автозамены
    for (const auto& rule : rules_) {
        if (rule.typo != normalized) continue;
        if (rule.previous_word) {
            if (context.empty() || lowercase_ru(context.back()) != *rule.previous_word) {
                result.confidence = rule.confidence;
                result.reason = "context condition not met";
                return result;
            }
        }
        result.candidate = preserve_case(word, rule.correction);
        result.confidence = rule.confidence;
        result.action = rule.confidence >= 0.95 ? Action::replace : Action::suggest;
        result.reason = rule.previous_word ? "personal rule with context" : "personal rule";
        return result;
    }

    // 2. Remaining layout path (RU→EN reverse); EN→RU already handled above.

    // Reverse layout correction, e.g. "руддщ" typed instead of "hello".
    if (const auto english = cyrillic_to_qwerty(word)) {
        const bool valid_russian = spell_checker_ && spell_checker_->available() &&
                                   spell_checker_->contains(normalized);
        const bool valid_english = spell_checker_ && spell_checker_->available_en() &&
                                   spell_checker_->contains_en(lowercase_ru(*english));
        if (!valid_russian && valid_english) {
            result.action = Action::replace;
            result.candidate = *english;
            result.confidence = 0.99;
            result.reason = "layout correction";
            return result;
        }
    }

    // Apply general protection (e.g. acronyms, capitalized words) to avoid spelling corrections
    if (is_protected(word)) {
        result.reason = "protected token";
        return result;
    }

    // 3. Обычная проверка слова по словарям с ранжированием по Дамерау-Левенштейну и биграммам
    if (spell_checker_) {
        const bool is_english = is_qwerty_only(word) && has_ascii_letter(word);
        
        if (is_english) {
            if (spell_checker_->available_en() && spell_checker_->contains_en(normalized)) {
                result.reason = "english dictionary word";
                return result;
            }
        } else {
            if (spell_checker_->available() && spell_checker_->contains(normalized)) {
                result.reason = "dictionary word";
                return result;
            }
        }
        
        std::vector<std::string> suggestions;
        if (is_english) {
            if (spell_checker_->available_en()) {
                suggestions = spell_checker_->suggest_en(normalized);
            }
        } else {
            if (spell_checker_->available()) {
                suggestions = spell_checker_->suggest(normalized);
            }
        }
        
        if (!suggestions.empty()) {
            std::u32string word_u32 = utf8_to_utf32(normalized);
            std::string best_candidate;
            double best_score = -9999.0;
            std::size_t best_dist = 999;
            
            for (const auto& candidate : suggestions) {
                std::string cand_norm = lowercase_ru(candidate);
                std::u32string cand_u32 = utf8_to_utf32(cand_norm);
                std::size_t dist = damerau_levenshtein_distance(word_u32, cand_u32);
                
                int trans_count = 0;
                int word_count = 0;
                if (store) {
                    word_count = store->get_word_use_count(cand_norm);
                    if (!context.empty()) {
                        trans_count = store->get_transition_use_count(context.back(), cand_norm);
                    }
                }
                
                // Формула веса кандидата
                double score = 100.0 - static_cast<double>(dist) * 15.0;
                score += keyboard_proximity_bonus(word_u32, cand_u32);
                if (trans_count > 0) {
                    score += 25.0 + std::log1p(trans_count) * 5.0;
                }
                if (word_count > 0) {
                    score += 10.0 + std::log1p(word_count) * 2.0;
                }
                
                if (score > best_score) {
                    best_score = score;
                    best_candidate = candidate;
                    best_dist = dist;
                }
            }
            
            if (!best_candidate.empty()) {
                result.candidate = preserve_case(word, best_candidate);
                if (is_english) {
                    result.confidence = 0.80;
                    result.action = Action::suggest;
                } else {
                    if (best_dist == 1) {
                        result.confidence = 0.96;
                        result.action = Action::replace;
                    } else if (best_dist == 2) {
                        if (word_u32.size() >= 6 || best_score > 90.0) {
                            result.confidence = 0.95;
                            result.action = Action::replace;
                        } else {
                            result.confidence = 0.75;
                            result.action = Action::suggest;
                        }
                    } else {
                        result.confidence = 0.60;
                        result.action = Action::suggest;
                    }
                }
                result.reason = is_english ? "english hunspell suggestion (ranked)" : "hunspell suggestion (ranked)";
                // ST-023: never auto-split a solid word into multi-word ("автоплатежей"
                // → "авто платежей"). Keep as suggestion only.
                if (result.action == Action::replace &&
                    result.candidate.find(' ') != std::string::npos &&
                    word.find(' ') == std::string_view::npos) {
                    result.action = Action::suggest;
                    result.confidence = std::min(result.confidence, 0.75);
                    result.reason += " (no auto-split compound)";
                }
            }
        }
    }
    return result;
}

std::vector<std::string> Corrector::get_candidates(std::string_view word, const std::vector<std::string>& context, const PersonalStore* store) const {
    if (const auto normalized_case = normalize_accidental_case(word)) {
        return {*normalized_case};
    }
    if (is_strongly_protected(word)) return {};
    
    const bool is_english = is_qwerty_only(word) && has_ascii_letter(word);
    const auto normalized = lowercase_ru(word);

    // Check for layout correction candidate first
    std::optional<std::string> layout_corrected;
    if (is_qwerty_only(word)) {
        bool is_valid_english = false;
        if (spell_checker_ && spell_checker_->available_en()) {
            is_valid_english = spell_checker_->contains_en(normalized);
        }
        if (!is_valid_english) {
            const auto cyrillic = qwerty_to_cyrillic(word);
            const auto cyrillic_normalized = lowercase_ru(cyrillic);
            bool is_valid_russian = false;
            if (std::find(personal_words_.begin(), personal_words_.end(), cyrillic_normalized) != personal_words_.end()) {
                is_valid_russian = true;
            } else if (spell_checker_ && spell_checker_->available()) {
                is_valid_russian = spell_checker_->contains(cyrillic_normalized);
            }
            if (is_valid_russian) {
                layout_corrected = preserve_case(word, cyrillic);
            }
        }
    } else if (const auto english = cyrillic_to_qwerty(word)) {
        const bool valid_russian = spell_checker_ && spell_checker_->available() &&
                                   spell_checker_->contains(normalized);
        const bool valid_english = spell_checker_ && spell_checker_->available_en() &&
                                   spell_checker_->contains_en(lowercase_ru(*english));
        if (!valid_russian && valid_english) {
            layout_corrected = *english;
        }
    }

    if (layout_corrected) {
        return {*layout_corrected};
    }

    if (is_protected(word)) return {};
    if (utf8_to_utf32(normalized).size() <= 1) {
        if (is_qwerty_only(word) && has_ascii_letter(word)) {
            const auto cyrillic = qwerty_to_cyrillic(word);
            const auto cyrillic_normalized = lowercase_ru(cyrillic);
            if (cyrillic_normalized == "а" || cyrillic_normalized == "и" ||
                cyrillic_normalized == "в" || cyrillic_normalized == "о" ||
                cyrillic_normalized == "с" || cyrillic_normalized == "у" ||
                cyrillic_normalized == "я" || cyrillic_normalized == "к") {
                return {preserve_case(word, cyrillic)};
            }
        }
        return {};
    }
    std::vector<std::pair<std::string, double>> scored_candidates;
    std::unordered_set<std::string> seen;

    auto add_candidate = [&](const std::string& candidate, double score) {
        std::string preserved = preserve_case(word, candidate);
        if (seen.insert(preserved).second) {
            scored_candidates.emplace_back(preserved, score);
        }
    };

    // 1. Проверяем правила автозамены
    for (const auto& rule : rules_) {
        if (rule.typo == normalized) {
            if (!rule.previous_word || (!context.empty() && lowercase_ru(context.back()) == *rule.previous_word)) {
                double score = 200.0 + rule.confidence * 50.0;
                add_candidate(rule.correction, score);
            }
        }
    }

    // 2. Проверяем раскладку
    if (is_qwerty_only(word)) {
        bool is_valid_english = false;
        if (spell_checker_ && spell_checker_->available_en()) {
            is_valid_english = spell_checker_->contains_en(normalized);
        }
        if (!is_valid_english) {
            const auto cyrillic = qwerty_to_cyrillic(word);
            const auto cyrillic_normalized = lowercase_ru(cyrillic);
            bool is_valid_russian = false;
            if (std::find(personal_words_.begin(), personal_words_.end(), cyrillic_normalized) != personal_words_.end()) {
                is_valid_russian = true;
            } else if (spell_checker_ && spell_checker_->available()) {
                is_valid_russian = spell_checker_->contains(cyrillic_normalized);
            }
            if (is_valid_russian) {
                add_candidate(cyrillic, 300.0);
            }
        }
    }
    if (const auto english = cyrillic_to_qwerty(word)) {
        const bool valid_russian = spell_checker_ && spell_checker_->available() &&
                                   spell_checker_->contains(normalized);
        const bool valid_english = spell_checker_ && spell_checker_->available_en() &&
                                   spell_checker_->contains_en(lowercase_ru(*english));
        if (!valid_russian && valid_english) add_candidate(*english, 300.0);
    }

    // 3. Собираем варианты Hunspell
    if (spell_checker_) {
        std::vector<std::string> suggestions;
        if (is_english) {
            if (spell_checker_->available_en() && !spell_checker_->contains_en(normalized)) {
                suggestions = spell_checker_->suggest_en(normalized);
            }
        } else {
            if (spell_checker_->available() && !spell_checker_->contains(normalized)) {
                suggestions = spell_checker_->suggest(normalized);
            }
        }
        
        std::u32string word_u32 = utf8_to_utf32(normalized);
        for (const auto& candidate : suggestions) {
            std::string cand_norm = lowercase_ru(candidate);
            std::u32string cand_u32 = utf8_to_utf32(cand_norm);
            std::size_t dist = damerau_levenshtein_distance(word_u32, cand_u32);
            
            int trans_count = 0;
            int word_count = 0;
            if (store) {
                word_count = store->get_word_use_count(cand_norm);
                if (!context.empty()) {
                    trans_count = store->get_transition_use_count(context.back(), cand_norm);
                }
            }
            
            double score = 100.0 - static_cast<double>(dist) * 15.0;
            score += keyboard_proximity_bonus(word_u32, cand_u32);
            if (trans_count > 0) {
                score += 25.0 + std::log1p(trans_count) * 5.0;
            }
            if (word_count > 0) {
                score += 10.0 + std::log1p(word_count) * 2.0;
            }
            add_candidate(candidate, score);
        }
    }

    std::sort(scored_candidates.begin(), scored_candidates.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    static const std::unordered_map<std::string_view, std::string_view> emoji_map = {
        {"привет", "👋"}, {"пока", "👋"}, {"кофе", "☕"}, {"пицца", "🍕"},
        {"круто", "😎"}, {"хаха", "😂"}, {"смешно", "😂"}, {"любовь", "❤️"},
        {"сердце", "❤️"}, {"огонь", "🔥"}, {"да", "✅"}, {"нет", "❌"},
        {"сон", "😴"}, {"спать", "😴"}, {"ок", "👍"}, {"хорошо", "👍"},
        {"думаю", "🤔"}, {"плачу", "😢"}, {"грустно", "😢"}, {"пиво", "🍺"},
        {"вино", "🍷"}, {"чай", "🍵"}, {"торт", "🎂"}, {"подарок", "🎁"},
        {"музыка", "🎵"}, {"кот", "🐱"}, {"кошка", "🐱"}, {"собака", "🐶"},
        {"пес", "🐶"}, {"деньги", "💵"}
    };

    std::string emoji_match;
    auto it = emoji_map.find(normalized);
    if (it != emoji_map.end()) {
        emoji_match = it->second;
    } else if (!scored_candidates.empty()) {
        std::string best_cand_lower = lowercase_ru(scored_candidates.front().first);
        auto it2 = emoji_map.find(best_cand_lower);
        if (it2 != emoji_map.end()) {
            emoji_match = it2->second;
        }
    }

    std::vector<std::string> result;
    for (std::size_t i = 0; i < scored_candidates.size() && result.size() < 2; ++i) {
        result.push_back(scored_candidates[i].first);
    }
    if (!emoji_match.empty()) {
        result.push_back(emoji_match);
    }
    for (std::size_t i = result.size(); i < std::min<std::size_t>(scored_candidates.size(), 3); ++i) {
        result.push_back(scored_candidates[i].first);
    }
    // Gboard-style escape hatch: when corrections exist, the exact spelling
    // the user entered is always the final candidate. Selecting it explicitly
    // teaches SmartType that this is an intentional word.
    if (!scored_candidates.empty() &&
        std::find(result.begin(), result.end(), std::string(word)) == result.end()) {
        if (result.size() >= 3) result.resize(2);
        result.emplace_back(word);
    }
    return result;
}

std::string action_name(Action action) {
    switch (action) {
    case Action::keep: return "keep";
    case Action::suggest: return "suggest";
    case Action::replace: return "replace";
    }
    return "keep";
}

}  // namespace smarttype
