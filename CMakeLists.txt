cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(dump-analyzer LANGUAGES CXX)

add_executable(dump-analyzer-cli src/main.cpp)
target_compile_features(dump-analyzer-cli PUBLIC cxx_std_20)
set_target_properties(
    dump-analyzer-cli
    PROPERTIES CXX_EXTENSIONS OFF
               CXX_STANDARD_REQUIRED ON
               COMPILE_WARNING_AS_ERROR ON)
if(MSVC)
    target_compile_options(dump-analyzer-cli PRIVATE /W4)
else()
    target_compile_options(dump-analyzer-cli PRIVATE -Wall -Wextra -pedantic)
endif()
