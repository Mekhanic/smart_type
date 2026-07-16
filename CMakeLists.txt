cmake_minimum_required(VERSION 3.21)
project(smarttype VERSION 0.2.2 LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE GIT_HASH_RESULT
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT GIT_HASH_RESULT EQUAL 0 OR GIT_COMMIT_HASH STREQUAL "")
    set(GIT_COMMIT_HASH "source-${PROJECT_VERSION}")
endif()

# Get build timestamp
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/smarttype/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/smarttype/version.hpp
    @ONLY
)

add_library(smarttype_core
    src/core/corrector.cpp
    src/core/decision_log.cpp
    src/core/personal_store.cpp
    src/core/text.cpp
)
set_target_properties(smarttype_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(smarttype_core PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/generated)
find_package(SQLite3 REQUIRED)
if(NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 UNKNOWN IMPORTED)
    set_target_properties(SQLite3::SQLite3 PROPERTIES
        IMPORTED_LOCATION "${SQLite3_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIRS}")
endif()
target_link_libraries(smarttype_core PUBLIC SQLite3::SQLite3 ${CMAKE_DL_LIBS})
target_compile_options(smarttype_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(smarttype-eval src/cli/eval.cpp)
target_link_libraries(smarttype-eval PRIVATE smarttype_core)

add_executable(smarttypectl src/cli/control.cpp)
target_link_libraries(smarttypectl PRIVATE smarttype_core)
install(TARGETS smarttypectl smarttype-eval DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/fcitx5-layout-sync.py DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/configure-fcitx-x11.py DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/configure-fcitx-gnome.py DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/configure-fcitx-profile.py DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/doctor.sh scripts/uninstall-user.sh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/smarttype)
install(FILES config/fcitx5-layout-sync.service DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user)
configure_file(config/smarttype-tray.service.in smarttype-tray.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/smarttype-tray.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/themes/smarttype-liquid-glass/"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/themes/smarttype-liquid-glass
        FILES_MATCHING PATTERN "theme.conf" PATTERN "*.png" PATTERN "*.svg")

enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME gnome-fcitx-config
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/configure_fcitx_gnome_tests.py
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/configure-fcitx-gnome.py)
add_test(
    NAME x11-fcitx-config
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/configure_fcitx_x11_tests.py
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/configure-fcitx-x11.py)
add_test(
    NAME x11-layout-sync
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/layout_sync_tests.py
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fcitx5-layout-sync.py)
add_test(
    NAME fcitx-profile-config
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/configure_fcitx_profile_tests.py
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/configure-fcitx-profile.py)
add_test(
    NAME release-bootstrap
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/install_release_tests.sh)
add_test(
    NAME install-dependencies
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/install_dependency_tests.sh)
add_test(
    NAME kimpanel-interaction-patch
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/patch_kimpanel_tests.py
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch-kimpanel.py)

option(SMARTTYPE_BUILD_FCITX "Build the Fcitx5 input method addon" ON)
option(SMARTTYPE_BUILD_UI_DEMO "Build the legacy standalone Qt 6/QML candidate UI prototype" OFF)
option(SMARTTYPE_BUILD_TRAY "Build the SmartType system tray" ON)
if(SMARTTYPE_BUILD_FCITX)
    set(SMARTTYPE_FCITX_SDK_ROOT "" CACHE PATH "Root of an extracted Fcitx5 SDK")
    if(SMARTTYPE_FCITX_SDK_ROOT)
        find_path(SMARTTYPE_FCITX_INCLUDE_DIR Fcitx5/Core/fcitx/instance.h
                  PATHS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/include" NO_DEFAULT_PATH REQUIRED)
        find_library(SMARTTYPE_FCITX_CORE_LIBRARY NAMES Fcitx5Core
                     PATHS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib" "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib64"
                     NO_DEFAULT_PATH REQUIRED)
        find_library(SMARTTYPE_FCITX_CONFIG_LIBRARY NAMES Fcitx5Config
                     PATHS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib" "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib64"
                     NO_DEFAULT_PATH REQUIRED)
        find_library(SMARTTYPE_FCITX_UTILS_LIBRARY NAMES Fcitx5Utils
                     PATHS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib" "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib64"
                     NO_DEFAULT_PATH REQUIRED)
        add_library(Fcitx5::Core SHARED IMPORTED)
        set_target_properties(Fcitx5::Core PROPERTIES
            IMPORTED_LOCATION "${SMARTTYPE_FCITX_CORE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SMARTTYPE_FCITX_INCLUDE_DIR}/Fcitx5/Core;${SMARTTYPE_FCITX_INCLUDE_DIR}/Fcitx5/Utils;${SMARTTYPE_FCITX_INCLUDE_DIR}/Fcitx5/Config"
            INTERFACE_LINK_LIBRARIES "${SMARTTYPE_FCITX_CONFIG_LIBRARY};${SMARTTYPE_FCITX_UTILS_LIBRARY}")
        set(Fcitx5Core_FOUND TRUE)
    else()
        find_package(Fcitx5Core QUIET)
    endif()
    if(Fcitx5Core_FOUND OR TARGET Fcitx5::Core)
        add_subdirectory(src/fcitx)
    else()
        message(FATAL_ERROR "Fcitx5 development files not found. Install the Fcitx5 development package or set SMARTTYPE_FCITX_SDK_ROOT.")
    endif()
endif()

if(SMARTTYPE_BUILD_UI_DEMO)
    add_subdirectory(src/ui)
endif()

if(SMARTTYPE_BUILD_TRAY)
    add_subdirectory(src/tray)
endif()

# Developer tools (smarttype-theme-lab, etc.) — EXCLUDE_FROM_ALL, never built by default.
add_subdirectory(tools)

option(SMARTTYPE_BUILD_INTERNAL_UI "Build the SmartType internal Fcitx5 UI addon" ON)
option(SMARTTYPE_ENABLE_X11 "Build the native X11 candidate-panel renderer" ON)
if(SMARTTYPE_BUILD_INTERNAL_UI AND TARGET Fcitx5::Core)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Cairo REQUIRED IMPORTED_TARGET cairo)
    pkg_check_modules(Pango REQUIRED IMPORTED_TARGET pango pangocairo)
    pkg_check_modules(GdkPixbuf REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)
    pkg_check_modules(GioUnix REQUIRED IMPORTED_TARGET gio-unix-2.0)
    pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client wayland-cursor)

    set(WAYLAND_FOUND ON)
    set(ENABLE_DBUS OFF)

    if(SMARTTYPE_ENABLE_X11)
        if(SMARTTYPE_FCITX_SDK_ROOT)
            list(PREPEND CMAKE_PREFIX_PATH
                "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib64/cmake"
                "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib/cmake")
        endif()
        find_package(Fcitx5ModuleXCB REQUIRED)
        pkg_check_modules(CairoXCB REQUIRED IMPORTED_TARGET cairo-xcb)
        pkg_check_modules(XcbAux REQUIRED IMPORTED_TARGET xcb-aux)
        pkg_check_modules(XcbIcccm REQUIRED IMPORTED_TARGET xcb-icccm)
        pkg_check_modules(XcbXinerama REQUIRED IMPORTED_TARGET xcb-xinerama)
        pkg_check_modules(XcbRandr REQUIRED IMPORTED_TARGET xcb-randr)
        pkg_check_modules(XcbEwmh REQUIRED IMPORTED_TARGET xcb-ewmh)
        pkg_check_modules(XcbKeysyms REQUIRED IMPORTED_TARGET xcb-keysyms)
        set(ENABLE_X11 ON)
    else()
        set(ENABLE_X11 OFF)
    endif()
    
    add_subdirectory(src/ui/smarttypeui)
endif()

add_executable(smarttype-tests tests/core_tests.cpp)
target_link_libraries(smarttype-tests PRIVATE smarttype_core)
add_test(NAME core COMMAND smarttype-tests)
add_test(NAME eval-baseline
         COMMAND smarttype-eval ${PROJECT_SOURCE_DIR}/tests/data/baseline.tsv)

# UI addon tests: animation math, interrupt, and addon smoke
add_executable(smarttype-ui-addon-tests tests/ui_addon_tests.cpp)
target_compile_features(smarttype-ui-addon-tests PRIVATE cxx_std_20)
target_link_libraries(smarttype-ui-addon-tests PRIVATE ${CMAKE_DL_LIBS})
target_compile_options(smarttype-ui-addon-tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME animation-math      COMMAND smarttype-ui-addon-tests math)
add_test(NAME animation-interrupt COMMAND smarttype-ui-addon-tests interrupt)
if(TARGET smarttypeui)
    add_dependencies(smarttype-ui-addon-tests smarttypeui)
    add_test(NAME addon-smoke COMMAND smarttype-ui-addon-tests smoke
             "$<TARGET_PROPERTY:smarttypeui,SMARTTYPEUI_ADDON_CONFIG_FILE>"
             "$<TARGET_FILE:smarttypeui>" "${SMARTTYPE_ENABLE_X11}")
endif()

add_executable(smarttype-send-alt-shift EXCLUDE_FROM_ALL tests/send_alt_shift.cpp)
