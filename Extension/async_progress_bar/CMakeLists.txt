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

# 查找源文件
file(GLOB UI_FILES ${CMAKE_SOURCE_DIR}/ui/*.ui) # 确保路径正确，ui 文件夹下的所有 .ui 文件
file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp) # src 文件夹下的所有 .cpp 文件
file(GLOB HEADER_FILES ${CMAKE_SOURCE_DIR}/inc/*.h) # inc 文件夹下的所有 .h 文件
file(GLOB RESOURCE_FILES ${CMAKE_SOURCE_DIR}/qrc/*.qrc) # inc 文件夹下的所有 .h 文件

list(APPEND CMAKE_AUTOUIC_SEARCH_PATHS "ui")
list(APPEND CMAKE_AUTORCC_SEARCH_PATHS "qrc")

include_directories(${CMAKE_SOURCE_DIR}/inc)

add_executable(${PROJECT_NAME} ${UI_FILES} ${SRC_FILES} ${HEADER_FILES} ${RESOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets)