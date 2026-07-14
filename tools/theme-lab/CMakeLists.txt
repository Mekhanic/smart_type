cmake_minimum_required(VERSION 3.21)

#
# tools/theme-lab/CMakeLists.txt
#
# Builds the developer-only `smarttype-theme-lab` executable.
# Enabled via: cmake -DSMARTTYPE_BUILD_THEME_LAB=ON
#
# Dependencies: Qt6Widgets, Cairo, Pango (+ GLib via Pango).
# The tool is NOT installed; run it directly from the build directory.
#

find_package(Qt6 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    message(STATUS "Qt6Widgets not found — smarttype-theme-lab disabled")
    return()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(Cairo    REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(Pango    REQUIRED IMPORTED_TARGET pango pangocairo)
pkg_check_modules(GdkPixbuf REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)

set(THEMELAB_SRCS
    main.cpp
    labtheme.h
    labtheme.cpp
    labrenderer.h
    labrenderer.cpp
    themepreviewwidget.h
    themepreviewwidget.cpp
    themelabwindow.h
    themelabwindow.cpp
)

qt6_wrap_cpp(THEMELAB_MOC
    themepreviewwidget.h
    themelabwindow.h
)

add_executable(smarttype-theme-lab EXCLUDE_FROM_ALL
    ${THEMELAB_SRCS}
    ${THEMELAB_MOC}
)

target_compile_features(smarttype-theme-lab PRIVATE cxx_std_20)

target_link_libraries(smarttype-theme-lab PRIVATE
    Qt6::Widgets
    PkgConfig::Cairo
    PkgConfig::Pango
    PkgConfig::GdkPixbuf
)

target_include_directories(smarttype-theme-lab PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Informational message
message(STATUS "smarttype-theme-lab: will build when target is explicitly requested")
message(STATUS "  Run: cmake --build <builddir> --target smarttype-theme-lab")
