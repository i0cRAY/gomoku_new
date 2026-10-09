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
    src/core/core.cpp
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
    src/app/app.cpp
)
set_target_properties(gomoku_app PROPERTIES AUTOMOC ON)
target_link_libraries(gomoku_app PUBLIC gomoku_core gomoku_net Qt6::Core)

# 主程式與介面層
add_executable(gomoku
    src/main.cpp
)
set_target_properties(gomoku PROPERTIES AUTOMOC ON)
target_link_libraries(gomoku PRIVATE gomoku_app Qt6::Widgets)

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
    )
    target_link_libraries(core_tests PRIVATE gomoku_core GTest::gtest_main)
    gtest_discover_tests(core_tests)
endif()
