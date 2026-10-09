cmake_minimum_required(VERSION 3.21)
project(gomoku_rt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

# 邏輯層 + 資料層：純 C++，不得連結任何 Qt（design §2.1）
add_library(gomoku_core STATIC
    src/core/board.cpp
    src/core/rule_checker.cpp
    src/core/energy_manager.cpp
    src/core/skill_system.cpp
    src/core/ai_engine.cpp
)
target_include_directories(gomoku_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 網路：QtCore + QtNetwork，不得依賴 QtWidgets
add_library(gomoku_net STATIC
    src/net/net.cpp
)
set_target_properties(gomoku_net PROPERTIES AUTOMOC ON)
target_link_libraries(gomoku_net PUBLIC gomoku_core Qt6::Core Qt6::Network)

# 控制層：QtCore，不得依賴 QtWidgets
add_library(gomoku_app STATIC
    src/app/game_controller.h
    src/app/game_controller.cpp
    src/app/game_clock.h
    src/app/game_clock.cpp
    src/app/local_session.h
    src/app/local_session.cpp
    src/app/game_session.h
)
set_target_properties(gomoku_app PROPERTIES AUTOMOC ON)
target_link_libraries(gomoku_app PUBLIC gomoku_core gomoku_net Qt6::Core)

# 介面層：QtWidgets
add_library(gomoku_ui STATIC
    src/ui/board_geometry.cpp
    src/ui/board_view.h
    src/ui/board_view.cpp
    src/ui/hud_model.cpp
    src/ui/board_input.cpp
    src/ui/menu_model.cpp
    src/ui/hud_view.h
    src/ui/hud_view.cpp
    src/ui/skill_select_view.h
    src/ui/skill_select_view.cpp
    src/ui/main_window.h
    src/ui/main_window.cpp
)
set_target_properties(gomoku_ui PROPERTIES AUTOMOC ON)
target_link_libraries(gomoku_ui PUBLIC gomoku_app Qt6::Widgets)

# 主程式
add_executable(gomoku
    src/main.cpp
)
set_target_properties(gomoku PROPERTIES AUTOMOC ON)
target_link_libraries(gomoku PRIVATE gomoku_ui)

# 測試
include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)

    add_executable(core_tests
        tests/core/types_test.cpp
        tests/core/board_test.cpp
        tests/core/rule_checker_test.cpp
        tests/core/energy_manager_test.cpp
        tests/core/skill_system_test.cpp
        tests/core/ai_pattern_test.cpp
        tests/core/ai_score_test.cpp
        tests/core/ai_decision_test.cpp
    )
    target_link_libraries(core_tests PRIVATE gomoku_core GTest::gtest_main)
    gtest_discover_tests(core_tests)

    add_executable(app_tests
        tests/app/main.cpp
        tests/app/game_controller_test.cpp
        tests/app/game_controller_skill_test.cpp
        tests/app/countdown_restart_test.cpp
        tests/app/local_session_test.cpp
        tests/app/ai_controller_test.cpp
    )
    target_link_libraries(app_tests PRIVATE gomoku_app GTest::gtest)
    gtest_discover_tests(app_tests)

    add_executable(ui_tests
        tests/ui/board_geometry_test.cpp
        tests/ui/hud_model_test.cpp
        tests/ui/board_input_test.cpp
        tests/ui/menu_model_test.cpp
    )
    target_link_libraries(ui_tests PRIVATE gomoku_ui GTest::gtest_main)
    gtest_discover_tests(ui_tests)
endif()
