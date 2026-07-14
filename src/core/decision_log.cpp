#include "smarttype/decision_log.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace smarttype {

std::filesystem::path DecisionLog::default_path() {
    if (const char* state = std::getenv("XDG_STATE_HOME"); state && *state) {
        return std::filesystem::path(state) / "smarttype/decisions.jsonl";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".local/state/smarttype/decisions.jsonl";
    }
    return {};
}

DecisionLog::DecisionLog(std::filesystem::path path) : path_(std::move(path)) {
    if (!path_.empty()) {
        std::filesystem::create_directories(path_.parent_path());
        if (path_.parent_path().filename() == "smarttype") {
            std::filesystem::permissions(
                path_.parent_path(), std::filesystem::perms::owner_all,
                std::filesystem::perm_options::replace);
        }
        if (std::filesystem::exists(path_)) {
            std::filesystem::permissions(
                path_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                std::filesystem::perm_options::replace);
        }
    }
}

void DecisionLog::write(std::string_view token, const Decision& decision) const {
    if (path_.empty()) return;
    std::ostringstream token_id;
    token_id << std::hex << std::hash<std::string_view>{}(token);
    const auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ofstream output(path_, std::ios::app);
    output << "{\"time\":" << timestamp << ",\"action\":\"" << action_name(decision.action)
           << "\",\"confidence\":" << std::fixed << std::setprecision(3) << decision.confidence
           << ",\"reason\":\"" << decision.reason << "\",\"token_id\":\"" << token_id.str()
           << "\"}\n";
    output.close();
    std::filesystem::permissions(
        path_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
}

}  // namespace smarttype
