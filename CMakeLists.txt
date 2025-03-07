cmake_minimum_required(VERSION 3.10)
project(FFMusic CXX)

add_definitions(-DTAGLIB_STATIC)

# set(SOURCES
#     main.cpp
#     src/GBKHelper.cpp
#     src/FileMgr.cpp)

file(GLOB_RECURSE PROJECT_SOURCE
    inc/*.h
    src/*.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE})

target_include_directories(${PROJECT_NAME} PRIVATE ./inc)


find_package(Slint REQUIRED)

add_subdirectory(./libs/taglib)
add_subdirectory(./libs/SFML)

target_link_libraries(${PROJECT_NAME} PRIVATE Slint::Slint sfml-audio tag)
target_include_directories(${PROJECT_NAME} PRIVATE ./libs/SFML/include ./libs/taglib/taglib ./libs/taglib/taglib/toolkit ./src)
slint_target_sources(${PROJECT_NAME} ui/app-window.slint)


