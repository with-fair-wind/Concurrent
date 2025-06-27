cmake_minimum_required(VERSION 3.20) # CMake install : https://cmake.org/download/
project(async_progress_bar LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# set(CMAKE_PREFIX_PATH "F:/soft/Qt6.7.2/6.7.2/mingw_64") # Qt Kit Dir
set(CMAKE_PREFIX_PATH "D:/soft/Qt/Qt5.14.2/5.14.2/mingw73_64") # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(PROJECT_BINARY_DIR STREQUAL PROJECT_SOURCE_DIR)
    message(WARNING "The binary directory of CMake cannot be the same as source directory!")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(WIN32)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES)
endif()

if(NOT MSVC)
    find_program(CCACHE_PROGRAM ccache)

    if(CCACHE_PROGRAM)
        message(STATUS "Found CCache: ${CCACHE_PROGRAM}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PROGRAM})
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PROGRAM})
    endif()
endif()

message("PROJECT_NAME: ${PROJECT_NAME}")
message("PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
message("PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")
message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")

# 查找Qt库
find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
endif()

find_package(Qt5 COMPONENTS Widgets REQUIRED) # Qt COMPONENTS

if(Qt5_FOUND)
    message(STATUS "Qt5::Widgets found at " ${Qt5_DIR})
endif()

if(Qt6_FOUND)
    message(STATUS "Qt6::Widgets found at " ${Qt6_DIR})
endif()

# 查找源文件
file(GLOB UI_FILES ${PROJECT_SOURCE_DIR}/ui/*.ui)
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/inc/*.h)
file(GLOB RESOURCE_FILES ${PROJECT_SOURCE_DIR}/qrc/*.qrc)

list(APPEND CMAKE_AUTOUIC_SEARCH_PATHS "ui")
list(APPEND CMAKE_AUTORCC_SEARCH_PATHS "qrc")

include_directories(${PROJECT_SOURCE_DIR}/inc)
include_directories(${PROJECT_SOURCE_DIR}/inc)

add_executable(${PROJECT_NAME} ${UI_FILES} ${SRC_FILES} ${HEADER_FILES} ${RESOURCE_FILES})

if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)
elseif(Qt5_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets)
endif()