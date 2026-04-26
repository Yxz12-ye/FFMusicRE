cmake_minimum_required(VERSION 3.16)
project(FFMusicRE
    VERSION 0.1.0
    DESCRIPTION "A lightweight cross-platform C++ music player"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTO_RCC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

add_definitions(-DTAGLIB_STATIC)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(ENABLE_TESTING "Enable testing" OFF)
option(CMAKE_USE_VCPKG "Use vcpkg package manager" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/use_vcpkg.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_warnings.cmake)

file(GLOB_RECURSE PROJECT_SOURCE
    src/*.cpp
    src/*.h
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc)

find_package(Slint REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Slint::Slint sfml-audio tag)
slint_target_sources(${PROJECT_NAME} ui/app-window.slint)


