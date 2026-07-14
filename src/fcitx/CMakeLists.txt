add_library(smarttype MODULE smarttype_engine.cpp)
target_link_libraries(smarttype PRIVATE smarttype_core Fcitx5::Core)
target_include_directories(smarttype PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_options(smarttype PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(smarttype PROPERTIES PREFIX "" OUTPUT_NAME "smarttype")

configure_file(smarttype-addon.conf.in smarttype-addon.conf @ONLY)
set(SMARTTYPE_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/testdata/fcitx5")
file(MAKE_DIRECTORY "${SMARTTYPE_TEST_DATA_DIR}/addon" "${SMARTTYPE_TEST_DATA_DIR}/inputmethod")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/smarttype-addon.conf"
               "${SMARTTYPE_TEST_DATA_DIR}/addon/smarttype.conf" COPYONLY)
configure_file(smarttype.conf "${SMARTTYPE_TEST_DATA_DIR}/inputmethod/smarttype.conf" COPYONLY)
configure_file(smarttype-us.conf "${SMARTTYPE_TEST_DATA_DIR}/inputmethod/smarttype-us.conf" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/tests/fcitx/keyboard-us.conf"
               "${SMARTTYPE_TEST_DATA_DIR}/inputmethod/keyboard-us.conf" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/tests/fcitx/testfrontend.conf"
               "${SMARTTYPE_TEST_DATA_DIR}/addon/testfrontend.conf" COPYONLY)
install(TARGETS smarttype DESTINATION ${CMAKE_INSTALL_LIBDIR}/fcitx5)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/smarttype-addon.conf"
        RENAME smarttype.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/addon)
install(FILES smarttype.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod)
install(FILES smarttype-us.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod)

add_executable(smarttype-fcitx-test "${PROJECT_SOURCE_DIR}/tests/fcitx_integration.cpp")
target_link_libraries(smarttype-fcitx-test PRIVATE Fcitx5::Core smarttype_core)
find_path(SMARTTYPE_TESTFRONTEND_INCLUDE_DIR testfrontend_public.h
          HINTS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/include"
          PATH_SUFFIXES Fcitx5/Module/fcitx-module/testfrontend
          REQUIRED)
target_include_directories(smarttype-fcitx-test PRIVATE
    "${SMARTTYPE_TESTFRONTEND_INCLUDE_DIR}")
find_file(SMARTTYPE_TESTFRONTEND_LIBRARY libtestfrontend.so
          HINTS "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib" "${SMARTTYPE_FCITX_SDK_ROOT}/usr/lib64"
          PATHS /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu
          PATH_SUFFIXES fcitx5
          REQUIRED)
get_filename_component(SMARTTYPE_TESTFRONTEND_ADDON_DIR
                       "${SMARTTYPE_TESTFRONTEND_LIBRARY}" DIRECTORY)
add_dependencies(smarttype-fcitx-test smarttype)
add_test(NAME fcitx-integration COMMAND smarttype-fcitx-test)
set_tests_properties(fcitx-integration PROPERTIES ENVIRONMENT
    "SMARTTYPE_INTEGRATION_TEST=1;FCITX_ADDON_DIRS=${CMAKE_CURRENT_BINARY_DIR}:$<TARGET_FILE_DIR:smarttype>:${SMARTTYPE_TESTFRONTEND_ADDON_DIR};FCITX_DATA_DIRS=${SMARTTYPE_TEST_DATA_DIR};XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;XDG_STATE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-state")
