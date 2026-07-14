find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Network Qml Quick QuickControls2)

find_file(KF6_WINDOWSYSTEM_LIBRARY NAMES libKF6WindowSystem.so.6
          PATHS /usr/lib64 /usr/lib NO_DEFAULT_PATH REQUIRED)
set(SMARTTYPE_KWINDOWSYSTEM_INCLUDE "${PROJECT_SOURCE_DIR}/.deps/kf6-sysroot/usr/include/KF6/KWindowSystem")

qt_add_executable(smarttype-ui-demo
    main.cpp
    blurcontroller.cpp
    blurcontroller.hpp
)
set_target_properties(smarttype-ui-demo PROPERTIES AUTOMOC ON)

qt_add_resources(smarttype-ui-demo qml_resources
    PREFIX "/qml"
    FILES
        qml/Main.qml
        qml/CandidatePanel.qml
        qml/CandidateCapsule.qml
)

target_include_directories(smarttype-ui-demo PRIVATE "${SMARTTYPE_KWINDOWSYSTEM_INCLUDE}")
target_link_libraries(smarttype-ui-demo PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    "${KF6_WINDOWSYSTEM_LIBRARY}"
)
target_compile_options(smarttype-ui-demo PRIVATE -Wall -Wextra -Wpedantic -Werror)

qt_add_executable(smarttype-ui
    renderer_main.cpp
    rendererbridge.cpp
    rendererbridge.hpp
    blurcontroller.cpp
    blurcontroller.hpp
)
set_target_properties(smarttype-ui PROPERTIES AUTOMOC ON)
qt_add_resources(smarttype-ui renderer_qml_resources
    PREFIX "/qml"
    FILES
        qml/Production.qml
        qml/CandidatePanel.qml
        qml/CandidateCapsule.qml
)
target_include_directories(smarttype-ui PRIVATE "${SMARTTYPE_KWINDOWSYSTEM_INCLUDE}" "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(smarttype-ui PRIVATE
    Qt6::Core Qt6::DBus Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
    "${KF6_WINDOWSYSTEM_LIBRARY}"
)
target_compile_options(smarttype-ui PRIVATE -Wall -Wextra -Wpedantic -Werror)
install(TARGETS smarttype-ui DESTINATION bin)
