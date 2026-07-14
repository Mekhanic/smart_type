#pragma once

#include <string>
#include <string_view>

namespace smarttype {

std::string lowercase_ru(std::string_view input);
std::string preserve_case(std::string_view source, std::string replacement);
std::string translate_layout(std::string_view input);

}  // namespace smarttype

