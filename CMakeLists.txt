cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(dump-analyzer LANGUAGES CXX)

add_executable(
    ${PROJECT_NAME}
    src/main.cpp src/app/args.cpp src/app/app.cpp src/data/data.cpp
    src/parse/parse.cpp src/utils/fs_utils.cpp)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES CXX_EXTENSIONS OFF
               CXX_STANDARD_REQUIRED ON
               COMPILE_WARNING_AS_ERROR ON)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wshadow
                                                   -pedantic)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src)

add_subdirectory(third_party/argh)
target_link_libraries(${PROJECT_NAME} PRIVATE argh)
