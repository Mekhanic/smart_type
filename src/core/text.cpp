#include "smarttype/text.hpp"

#include <cctype>
#include <cstdint>
#include <iostream>
#include <vector>
#include <algorithm>

namespace smarttype {

namespace {
std::vector<uint32_t> utf8_to_utf32(std::string_view input);
std::string utf32_to_utf8(const std::vector<uint32_t>& input);
}

std::string lowercase_ru(std::string_view input) {
    std::string result(input);
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto byte = static_cast<unsigned char>(result[i]);
        if (byte >= 'A' && byte <= 'Z') {
            result[i] = static_cast<char>(std::tolower(byte));
        } else if (byte == 0xD0 && i + 1 < result.size()) {
            auto next = static_cast<unsigned char>(result[i + 1]);
            if (next >= 0x90 && next <= 0x9F) result[i + 1] = static_cast<char>(next + 0x20);
            else if (next >= 0xA0 && next <= 0xAF) {
                result[i] = static_cast<char>(0xD1);
                result[i + 1] = static_cast<char>(next - 0x20);
            } else if (next == 0x81) {
                result[i] = static_cast<char>(0xD1);
                result[i + 1] = static_cast<char>(0x91);
            }
            ++i;
        }
    }
    return result;
}

namespace {
bool is_uppercase_cp(uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return true;
    if ((cp >= 0x0410 && cp <= 0x042F) || cp == 0x0401) return true;
    return false;
}

bool is_lowercase_cp(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return true;
    if ((cp >= 0x0430 && cp <= 0x044F) || cp == 0x0451) return true;
    return false;
}

uint32_t to_uppercase_cp(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    if (cp >= 0x0430 && cp <= 0x044F) return cp - 0x20;
    if (cp == 0x0451) return 0x0401;
    return cp;
}

uint32_t to_lowercase_cp(uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 32;
    if (cp >= 0x0410 && cp <= 0x042F) return cp + 0x20;
    if (cp == 0x0401) return 0x0451;
    return cp;
}
} // namespace

std::string preserve_case(std::string_view source, std::string replacement) {
    if (source.empty() || replacement.empty()) return replacement;

    auto src_u32 = utf8_to_utf32(source);
    auto repl_u32 = utf8_to_utf32(replacement);

    // If character count matches 1-to-1:
    if (src_u32.size() == repl_u32.size()) {
        for (std::size_t i = 0; i < src_u32.size(); ++i) {
            if (is_uppercase_cp(src_u32[i])) {
                repl_u32[i] = to_uppercase_cp(repl_u32[i]);
            } else if (is_lowercase_cp(src_u32[i])) {
                repl_u32[i] = to_lowercase_cp(repl_u32[i]);
            }
        }
        return utf32_to_utf8(repl_u32);
    }

    // Fallback if lengths do not match:
    bool all_upper = true;
    bool has_letter = false;
    for (uint32_t cp : src_u32) {
        if (is_lowercase_cp(cp)) {
            has_letter = true;
            all_upper = false;
        } else if (is_uppercase_cp(cp)) {
            has_letter = true;
        }
    }

    if (has_letter && all_upper) {
        for (auto& cp : repl_u32) {
            cp = to_uppercase_cp(cp);
        }
        return utf32_to_utf8(repl_u32);
    }

    // Capitalize the first letter if the source first letter is uppercase
    if (!src_u32.empty() && is_uppercase_cp(src_u32[0])) {
        if (!repl_u32.empty()) {
            repl_u32[0] = to_uppercase_cp(repl_u32[0]);
        }
        return utf32_to_utf8(repl_u32);
    }

    return replacement;
}

namespace {

std::vector<uint32_t> utf8_to_utf32(std::string_view input) {
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

std::string utf32_to_utf8(const std::vector<uint32_t>& input) {
    std::string result;
    for (uint32_t cp : input) {
        if (cp < 0x80) {
            result += static_cast<char>(cp);
        } else if (cp < 0x800) {
            result += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            result += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x110000) {
            result += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
            result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return result;
}

uint32_t qwerty_to_jcuken(uint32_t cp) {
    switch (cp) {
        // Lowercase
        case 'q': return 0x0439;
        case 'w': return 0x0446;
        case 'e': return 0x0443;
        case 'r': return 0x043a;
        case 't': return 0x0435;
        case 'y': return 0x043d;
        case 'u': return 0x0433;
        case 'i': return 0x0448;
        case 'o': return 0x0449;
        case 'p': return 0x0437;
        case '[': return 0x0445;
        case ']': return 0x044a;
        case 'a': return 0x0444;
        case 's': return 0x044b;
        case 'd': return 0x0432;
        case 'f': return 0x0430;
        case 'g': return 0x043f;
        case 'h': return 0x0440;
        case 'j': return 0x043e;
        case 'k': return 0x043b;
        case 'l': return 0x0434;
        case ';': return 0x0436;
        case '\'': return 0x044d;
        case 'z': return 0x044f;
        case 'x': return 0x0447;
        case 'c': return 0x0441;
        case 'v': return 0x043c;
        case 'b': return 0x0438;
        case 'n': return 0x0442;
        case 'm': return 0x044c;
        case ',': return 0x0431;
        case '.': return 0x044e;
        case '/': return '.';
        case '`': return 0x0451;

        // Uppercase
        case 'Q': return 0x0419;
        case 'W': return 0x0426;
        case 'E': return 0x0423;
        case 'R': return 0x041a;
        case 'T': return 0x0415;
        case 'Y': return 0x041d;
        case 'U': return 0x0413;
        case 'I': return 0x0428;
        case 'O': return 0x0429;
        case 'P': return 0x0417;
        case '{': return 0x0425;
        case '}': return 0x042a;
        case 'A': return 0x0424;
        case 'S': return 0x042b;
        case 'D': return 0x0412;
        case 'F': return 0x0410;
        case 'G': return 0x041f;
        case 'H': return 0x0420;
        case 'J': return 0x041e;
        case 'K': return 0x041b;
        case 'L': return 0x0414;
        case ':': return 0x0416;
        case '"': return 0x042d;
        case 'Z': return 0x042f;
        case 'X': return 0x0427;
        case 'C': return 0x0421;
        case 'V': return 0x041c;
        case 'B': return 0x0418;
        case 'N': return 0x0422;
        case 'M': return 0x042c;
        case '<': return 0x0411;
        case '>': return 0x042e;
        case '?': return ',';
        case '~': return 0x0401;

        // Punctuation Shift Row (QWERTY to JCUKEN)
        case '@': return '"';
        case '#': return 0x2116; // №
        case '$': return ';';
        case '^': return ':';
        case '&': return '?';

        default: return cp;
    }
}

uint32_t jcuken_to_qwerty(uint32_t cp) {
    switch (cp) {
        // Lowercase
        case 0x0439: return 'q';
        case 0x0446: return 'w';
        case 0x0443: return 'e';
        case 0x043a: return 'r';
        case 0x0435: return 't';
        case 0x043d: return 'y';
        case 0x0433: return 'u';
        case 0x0448: return 'i';
        case 0x0449: return 'o';
        case 0x0437: return 'p';
        case 0x0445: return '[';
        case 0x044a: return ']';
        case 0x0444: return 'a';
        case 0x044b: return 's';
        case 0x0432: return 'd';
        case 0x0430: return 'f';
        case 0x043f: return 'g';
        case 0x0440: return 'h';
        case 0x043e: return 'j';
        case 0x043b: return 'k';
        case 0x0434: return 'l';
        case 0x0436: return ';';
        case 0x044d: return '\'';
        case 0x044f: return 'z';
        case 0x0447: return 'x';
        case 0x0441: return 'c';
        case 0x043c: return 'v';
        case 0x0438: return 'b';
        case 0x0442: return 'n';
        case 0x044c: return 'm';
        case 0x0431: return ',';
        case 0x044e: return '.';
        case '.':    return '/'; // RU dot is key '/'
        case 0x0451: return '`';

        // Uppercase
        case 0x0419: return 'Q';
        case 0x0426: return 'W';
        case 0x0423: return 'E';
        case 0x041a: return 'R';
        case 0x0415: return 'T';
        case 0x041d: return 'Y';
        case 0x0413: return 'U';
        case 0x0428: return 'I';
        case 0x0429: return 'O';
        case 0x0417: return 'P';
        case 0x0425: return '{';
        case 0x042a: return '}';
        case 0x0424: return 'A';
        case 0x042b: return 'S';
        case 0x0412: return 'D';
        case 0x0410: return 'F';
        case 0x041f: return 'G';
        case 0x0420: return 'H';
        case 0x041e: return 'J';
        case 0x041b: return 'K';
        case 0x0414: return 'L';
        case 0x0416: return ':';
        case 0x042d: return '"';
        case 0x042f: return 'Z';
        case 0x0427: return 'X';
        case 0x0421: return 'C';
        case 0x041c: return 'V';
        case 0x0418: return 'B';
        case 0x0422: return 'N';
        case 0x042c: return 'M';
        case 0x0411: return '<';
        case 0x042e: return '>';
        case ',':    return '?'; // RU comma (Shift+/) is key '?'
        case 0x0401: return '~';

        // Shift Row Punctuation
        case '"': return '@';
        case 0x2116: return '#'; // №
        case ';': return '$';
        case ':': return '^';

        default: return cp;
    }
}

} // namespace

std::string translate_layout(std::string_view input) {
    auto u32 = utf8_to_utf32(input);
    bool has_cyrillic = false;
    for (uint32_t cp : u32) {
        if (cp >= 0x0400 && cp <= 0x04FF) {
            has_cyrillic = true;
            break;
        }
    }
    
    std::vector<uint32_t> translated;
    translated.reserve(u32.size());
    for (uint32_t cp : u32) {
        if (has_cyrillic) {
            translated.push_back(jcuken_to_qwerty(cp));
        } else {
            translated.push_back(qwerty_to_jcuken(cp));
        }
    }
    return utf32_to_utf8(translated);
}

}  // namespace smarttype
