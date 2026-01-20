cmake_minimum_required(VERSION 3.16)

project(hft_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -O2)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(hft_main
    src/hft_main.cpp
)

add_executable(simex_server
    src/simex_server.cpp
)

target_link_libraries(hft_main pthread)
target_link_libraries(simex_server pthread)
