# tools/CMakeLists.txt
# Developer tools — none are built by default (all use EXCLUDE_FROM_ALL).

option(SMARTTYPE_BUILD_THEME_LAB
    "Build the developer-only Theme Lab UI editor (requires Qt6Widgets)"
    OFF)

if(SMARTTYPE_BUILD_THEME_LAB)
    add_subdirectory(theme-lab)
else()
    # Always include so the target exists for explicit --target invocations
    # when cmake was configured without the option.
    # (add_subdirectory will return early if Qt6 is missing anyway.)
    add_subdirectory(theme-lab)
endif()
