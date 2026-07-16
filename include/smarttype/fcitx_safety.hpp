#pragma once

#include <string_view>

namespace smarttype {

inline bool unsafe_raw_xim_context(std::string_view frontend, bool has_client_preedit) {
    return frontend == "xim" && !has_client_preedit;
}

}  // namespace smarttype
