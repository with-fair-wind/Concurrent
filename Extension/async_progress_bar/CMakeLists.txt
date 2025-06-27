cmake_minimum_required(VERSION 3.20) # CMake install : https://cmake.org/download/
project(async_progress_bar LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_PREFIX_PATH "D:/soft/Qt/Qt5.14.2/5.14.2/mingw73_64") # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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

add_executable(${PROJECT_NAME} ${UI_FILES} ${SRC_FILES} ${HEADER_FILES} ${RESOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets)