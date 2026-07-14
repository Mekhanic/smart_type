#pragma once

#include "smarttype/corrector.hpp"

#include <filesystem>
#include <string_view>

namespace smarttype {

class DecisionLog {
public:
    explicit DecisionLog(std::filesystem::path path = default_path());
    static std::filesystem::path default_path();
    void write(std::string_view token, const Decision& decision) const;

private:
    std::filesystem::path path_;
};

}  // namespace smarttype

