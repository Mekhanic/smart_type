#include "smarttype/personal_store.hpp"
#include "smarttype/version.hpp"

#include <iostream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cctype>

namespace {
void usage() {
    std::cerr << "usage:\n"
              << "  smarttypectl status\n"
              << "  smarttypectl add-word <word>\n"
              << "  smarttypectl add-rule <typo> <correction> [previous-word]\n"
              << "  smarttypectl list\n"
              << "  smarttypectl blacklist add <app>\n"
              << "  smarttypectl blacklist remove <app>\n"
              << "  smarttypectl blacklist list\n"
              << "  smarttypectl set-setting <key> <value>\n"
              << "  smarttypectl get-setting <key>\n"
              << "  smarttypectl --version\n";
}

pid_t find_fcitx5_pid() {
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (!std::all_of(name.begin(), name.end(), ::isdigit)) continue;
        try {
            std::ifstream comm_file(entry.path() / "comm");
            std::string comm;
            if (comm_file >> comm && comm == "fcitx5") {
                return std::stoi(name);
            }
        } catch (...) {}
    }
    return 0;
}

std::string find_loaded_library(pid_t pid, std::string_view lib_name) {
    try {
        std::ifstream maps_file("/proc/" + std::to_string(pid) + "/maps");
        std::string line;
        while (std::getline(maps_file, line)) {
            if (line.find(lib_name) != std::string::npos) {
                auto pos = line.find('/');
                if (pos != std::string::npos) {
                    std::string path = line.substr(pos);
                    while (!path.empty() && std::isspace(static_cast<unsigned char>(path.back()))) {
                        path.pop_back();
                    }
                    return path;
                }
            }
        }
    } catch (...) {}
    return "";
}
}

int main(int argc, char** argv) try {
    if (argc < 2) { usage(); return 2; }
    const std::string command = argv[1];
    if (command == "--version" || command == "-v" || command == "version") {
        std::cout << "smarttypectl version: " << smarttype::GIT_COMMIT_HASH
                  << " (built: " << smarttype::BUILD_TIMESTAMP << ")\n";
        return 0;
    }
    smarttype::PersonalStore store;
    if (command == "add-word" && argc == 3) {
        store.add_word(argv[2]);
        std::cout << "Добавлено в личный словарь: " << argv[2] << '\n';
    } else if (command == "add-rule" && (argc == 4 || argc == 5)) {
        store.add_rule(argv[2], argv[3], 0.99, argc == 5 ? argv[4] : "");
        std::cout << "Добавлено правило: " << argv[2] << " -> " << argv[3] << '\n';
    } else if (command == "list" && argc == 2) {
        std::cout << "Личные слова:\n";
        for (const auto& word : store.words()) std::cout << "  " << word << '\n';
        std::cout << "Правила:\n";
        for (const auto& rule : store.rules()) {
            std::cout << "  " << rule.typo << " -> " << rule.correction << " (" << rule.confidence << ')';
            if (rule.previous_word) std::cout << " после «" << *rule.previous_word << "»";
            std::cout << '\n';
        }
    } else if (command == "blacklist" && argc >= 3) {
        const std::string sub = argv[2];
        if (sub == "add" && argc == 4) {
            store.blacklist_add(argv[3]);
            std::cout << "Приложение '" << argv[3] << "' добавлено в черный список\n";
        } else if (sub == "remove" && argc == 4) {
            store.blacklist_remove(argv[3]);
            std::cout << "Приложение '" << argv[3] << "' удалено из черного списка\n";
        } else if (sub == "list" && argc == 3) {
            std::cout << "Черный список приложений:\n";
            for (const auto& app : store.blacklist_get()) {
                std::cout << "  " << app << '\n';
            }
        } else { usage(); return 2; }
    } else if (command == "set-setting" && argc == 4) {
        const std::string key = argv[2];
        const std::string val = argv[3];
        if (val == "true") {
            store.set_setting(key, true);
        } else if (val == "false") {
            store.set_setting(key, false);
        } else {
            store.set_string_setting(key, val);
        }
        std::cout << "Настройка '" << key << "' изменена на '" << val << "'\n";
    } else if (command == "get-setting" && argc == 3) {
        const std::string key = argv[2];
        const std::string val = store.string_setting(key, "");
        if (val.empty()) {
            const bool enabled = store.setting_enabled(key, false);
            std::cout << key << ": " << (enabled ? "true" : "false (или не задано)") << '\n';
        } else {
            std::cout << key << ": " << val << '\n';
        }
    } else if (command == "status" && argc == 2) {
        std::cout << "smarttypectl version: " << smarttype::GIT_COMMIT_HASH
                  << " (built: " << smarttype::BUILD_TIMESTAMP << ")\n";

        pid_t fcitx_pid = find_fcitx5_pid();
        std::string engine_path;
        if (fcitx_pid > 0) {
            engine_path = find_loaded_library(fcitx_pid, "smarttype.so");
        }
        if (engine_path.empty()) {
            engine_path = "Not loaded (fcitx5 is not running or addon not loaded)";
        }
        std::cout << "loaded engine path: " << engine_path << '\n';

        std::string active_ui = "unknown";
        if (fcitx_pid > 0) {
            if (!find_loaded_library(fcitx_pid, "smarttypeui.so").empty()) {
                active_ui = "smarttypeui (native server UI)";
            } else if (!find_loaded_library(fcitx_pid, "classicui.so").empty()) {
                active_ui = "classicui";
            } else if (!find_loaded_library(fcitx_pid, "kimpanel.so").empty()) {
                active_ui = "kimpanel";
            } else {
                active_ui = "none / other";
            }
        } else {
            active_ui = "Fcitx5 is not running";
        }
        std::cout << "active UI: " << active_ui << '\n';

        bool ext_ui = false;
        if (const char* env = std::getenv("SMARTTYPE_EXTERNAL_UI")) {
            ext_ui = (std::string_view(env) == "1" || std::string_view(env) == "true");
        } else {
            ext_ui = store.setting_enabled("external_ui", false);
        }
        std::cout << "external_ui state: " << (ext_ui ? "enabled" : "disabled") << '\n';

        std::cout << "database path: " << store.path() << '\n';

        std::vector<std::string> features;
        if (store.setting_enabled("enabled", true)) features.push_back("enabled");
        if (store.setting_enabled("autocorrect", true)) features.push_back("autocorrect");
        if (store.setting_enabled("suggestions", true)) features.push_back("suggestions");
        if (store.setting_enabled("learning", true)) features.push_back("learning");
        if (store.setting_enabled("smart_punctuation", true)) features.push_back("smart_punctuation");
        if (store.setting_enabled("layout_correction", true)) features.push_back("layout_correction");
        if (store.setting_enabled("accidental_case", true)) features.push_back("accidental_case");
        if (store.setting_enabled("inline_correction_flash", true)) features.push_back("inline_correction_flash");
        if (store.setting_enabled("sentence_capitalization", true)) features.push_back("sentence_capitalization");
        if (store.setting_enabled("disable_in_terminals", false)) features.push_back("disable_in_terminals");
        if (store.setting_enabled("disable_in_code", false)) features.push_back("disable_in_code");
        if (store.setting_enabled("diagnostics", false)) features.push_back("diagnostics");

        std::cout << "enabled features:";
        if (features.empty()) {
            std::cout << " none\n";
        } else {
            for (const auto& feat : features) {
                std::cout << " " << feat;
            }
            std::cout << '\n';
        }
        return 0;
    } else { usage(); return 2; }
    return 0;
} catch (const std::exception& error) {
    std::cerr << "smarttypectl: " << error.what() << '\n';
    return 1;
}
