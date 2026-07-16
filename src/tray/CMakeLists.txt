find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network DBus)

set(CMAKE_AUTORCC ON)
qt_add_executable(smarttype-tray main.cpp icons.qrc settingsdialog.cpp settingsdialog.hpp
                  issuereportdialog.cpp issuereportdialog.hpp)
target_link_libraries(smarttype-tray PRIVATE smarttype_core Qt6::Widgets Qt6::Network Qt6::DBus)
target_include_directories(smarttype-tray PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_options(smarttype-tray PRIVATE -Wall -Wextra -Wpedantic -Werror)
install(TARGETS smarttype-tray DESTINATION ${CMAKE_INSTALL_BINDIR})
# Generate the desktop file with home path replaced
file(READ ${PROJECT_SOURCE_DIR}/config/smarttype-tray.desktop DESKTOP_CONTENT)
string(REPLACE "@HOME@" "$ENV{HOME}" DESKTOP_CONTENT "${DESKTOP_CONTENT}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/smarttype-tray.desktop "${DESKTOP_CONTENT}")

# Desktop entry for the application menu only. User autostart is installed by
# scripts/install-user.sh into ${XDG_CONFIG_HOME}/autostart (not via CMake).
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/smarttype-tray.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${PROJECT_SOURCE_DIR}/icons/icon_idle.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
        RENAME smarttype-idle.png)

add_test(NAME settings-smoke COMMAND smarttype-tray --check-settings)
set_tests_properties(settings-smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_test(NAME tray-startup-no-popup COMMAND smarttype-tray --check-tray-startup)
set_tests_properties(tray-startup-no-popup PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME report-dialog-smoke COMMAND smarttype-tray --check-report)
set_tests_properties(report-dialog-smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME tray-icons-smoke COMMAND smarttype-tray --check-icons)
set_tests_properties(tray-icons-smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
