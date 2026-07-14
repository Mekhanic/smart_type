if (ENABLE_X11 OR WAYLAND_FOUND)

# add_subdirectory(themes)

set(CLASSICUI_SRCS)
set(CLASSICUI_LIBS)
if (WAYLAND_FOUND)
    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)

    set(WAYLAND_PROTOCOLS
        "input-method-unstable-v1.xml" "wayland-input-method-unstable-v1"
        "input-method-unstable-v2.xml" "wayland-input-method-unstable-v2"
        "virtual-keyboard-unstable-v1.xml" "wayland-virtual-keyboard-unstable-v1"
        "blur.xml" "wayland-blur"
        "cursor-shape-v1.xml" "wayland-cursor-shape-v1"
        "fractional-scale-v1.xml" "wayland-fractional-scale-v1"
        "viewporter.xml" "wayland-viewporter"
        "wlr-data-control-unstable-v1.xml" "wayland-wlr-data-control-unstable-v1"
        "tablet-unstable-v2.xml" "wayland-tablet-unstable-v2"
        "plasma-window-management.xml" "wayland-plasma-window-management"
        "wlr-foreign-toplevel-management-unstable-v1.xml" "wayland-wlr-foreign-toplevel-management-unstable-v1"
        "ext-data-control-v1.xml" "wayland-ext-data-control-unstable-v1"
    )

    list(LENGTH WAYLAND_PROTOCOLS proto_len)
    math(EXPR max_idx "${proto_len} - 1")
    foreach(idx RANGE 0 ${max_idx} 2)
        list(GET WAYLAND_PROTOCOLS ${idx} xml_file)
        math(EXPR val_idx "${idx} + 1")
        list(GET WAYLAND_PROTOCOLS ${val_idx} base_name)
        
        set(xml_path "${CMAKE_CURRENT_SOURCE_DIR}/${xml_file}")
        set(header_out "${CMAKE_CURRENT_BINARY_DIR}/${base_name}-client-protocol.h")
        set(code_out "${CMAKE_CURRENT_BINARY_DIR}/${base_name}-protocol.c")
        
        add_custom_command(
            OUTPUT "${header_out}"
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header "${xml_path}" "${header_out}"
            DEPENDS "${xml_path}"
        )
        add_custom_command(
            OUTPUT "${code_out}"
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code "${xml_path}" "${code_out}"
            DEPENDS "${xml_path}"
        )
        
        set(CLASSICUI_SRCS ${CLASSICUI_SRCS} "${header_out}" "${code_out}")
    endforeach()

    file(GLOB WAYLAND_WRAPPER_SRCS
         wl_*.cpp zwp_*.cpp zwlr_*.cpp wp_*.cpp ext_*.cpp org_kde_*.cpp)
    set(CLASSICUI_SRCS ${CLASSICUI_SRCS}
        waylandui.cpp waylandwindow.cpp waylandshmwindow.cpp
        waylandpointer.cpp buffer.cpp waylandinputwindow.cpp
        waylandcursor.cpp waylandcursortheme.cpp
        display.cpp outputinfomation.cpp
        ${WAYLAND_WRAPPER_SRCS})
    set(CLASSICUI_LIBS ${CLASSICUI_LIBS}
        PkgConfig::WaylandClient)
else()
endif()

if (ENABLE_X11)
    set(CLASSICUI_SRCS ${CLASSICUI_SRCS}
        xcbui.cpp xcbwindow.cpp xcbtraywindow.cpp xcbinputwindow.cpp xcbmenu.cpp)
    set(CLASSICUI_LIBS ${CLASSICUI_LIBS} PkgConfig::CairoXCB Fcitx5::Module::XCB
        PkgConfig::XcbAux PkgConfig::XcbIcccm PkgConfig::XcbXinerama
        PkgConfig::XcbRandr PkgConfig::XcbEwmh PkgConfig::XcbKeysyms)
endif()

if (ENABLE_DBUS)
    set(CLASSICUI_SRCS ${CLASSICUI_SRCS} portalsettingmonitor.cpp)
    set(CLASSICUI_LIBS ${CLASSICUI_LIBS} Fcitx5::Module::DBus)
endif()

configure_file(config.h.in config.h)

file(GLOB_RECURSE YOGA_SRCS
     yoga/*.cpp)

add_library(smarttypeui MODULE
    classicui.cpp window.cpp theme.cpp inputwindow.cpp plasmathemewatchdog.cpp colorhelper.cpp ${CLASSICUI_SRCS} ${YOGA_SRCS}
)
set_target_properties(smarttypeui PROPERTIES PREFIX "" OUTPUT_NAME "smarttypeui")

find_path(SMARTTYPE_NOTIFICATIONITEM_INCLUDE_DIR notificationitem_public.h
    HINTS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/include"
    PATH_SUFFIXES Fcitx5/Module/fcitx-module/notificationitem REQUIRED)
find_path(SMARTTYPE_WAYLAND_INCLUDE_DIR wayland_public.h
    HINTS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/include"
    PATH_SUFFIXES Fcitx5/Module/fcitx-module/wayland REQUIRED)
find_path(SMARTTYPE_XCB_INCLUDE_DIR xcb_public.h
    HINTS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/include"
    PATH_SUFFIXES Fcitx5/Module/fcitx-module/xcb)
find_path(SMARTTYPE_DBUS_INCLUDE_DIR dbus_public.h
    HINTS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/include"
    PATH_SUFFIXES Fcitx5/Module/fcitx-module/dbus)

target_include_directories(smarttypeui PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/yoga
    ${CMAKE_CURRENT_SOURCE_DIR}/fcitx-utils
    ${PROJECT_SOURCE_DIR}/include
    ${SMARTTYPE_NOTIFICATIONITEM_INCLUDE_DIR}
    ${SMARTTYPE_XCB_INCLUDE_DIR}
    ${SMARTTYPE_DBUS_INCLUDE_DIR}
    ${SMARTTYPE_WAYLAND_INCLUDE_DIR}
)

if (ENABLE_X11)
target_compile_definitions(smarttypeui PRIVATE -DFCITX_XCB_EWMH)
endif()

target_link_libraries(smarttypeui
    Fcitx5::Core
    PkgConfig::Cairo PkgConfig::Pango
    PkgConfig::GdkPixbuf PkgConfig::GioUnix
    ${CLASSICUI_LIBS}
   )

install(TARGETS smarttypeui DESTINATION ${CMAKE_INSTALL_LIBDIR}/fcitx5)
set(FCITX_ADDON_TYPE "SharedLibrary")
configure_file(smarttypeui.conf.in.in smarttypeui.conf @ONLY)
set_property(TARGET smarttypeui PROPERTY SMARTTYPEUI_ADDON_CONFIG_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/smarttypeui.conf")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/smarttypeui.conf" DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/addon)

endif()
