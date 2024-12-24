cmake_minimum_required(VERSION 3.20) # CMake install : https://cmake.org/download/
project(async_progress_bar LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# set(CMAKE_PREFIX_PATH "F:/soft/Qt6.7.2/6.7.2/mingw_64") # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt库
find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
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

if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)
elseif(Qt5_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets)
endif()