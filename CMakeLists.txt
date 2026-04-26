cmake_minimum_required(VERSION 3.21)
project(FFMusicRE
    VERSION 0.1.0
    DESCRIPTION "A lightweight cross-platform C++ music player"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(ENABLE_TESTING "Enable testing" OFF)
option(CMAKE_USE_VCPKG "Use vcpkg package manager" ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/use_vcpkg.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_warnings.cmake)

file(GLOB_RECURSE PROJECT_SOURCE CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(${PROJECT_NAME} PRIVATE TAGLIB_STATIC)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/slint/api/cpp/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/slint/api/cpp" slint_cpp_build)
endif()

if(NOT TARGET Slint::Slint)
    find_package(Slint REQUIRED)
endif()

find_package(SFML COMPONENTS Audio REQUIRED)
find_package(taglib CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Slint::Slint SFML::Audio TagLib::tag)
if(ENABLE_WARNINGS)
    target_link_libraries(${PROJECT_NAME} PRIVATE project_warnings)
endif()
slint_target_sources(${PROJECT_NAME} ui/app-window.slint)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Ole32 Shell32 Uuid)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
    )
endif()

