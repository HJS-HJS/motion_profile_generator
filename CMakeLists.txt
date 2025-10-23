cmake_minimum_required(VERSION 3.16)
project(MotionEditor_Qt5_v2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 실행 파일 생성 (commands.cpp, commands.h 추가)
add_executable(MotionEditor
    src/main.cpp
    src/core/mainwindow.h
    src/core/mainwindow.cpp
    src/core/motionmodels.h
    src/core/motionmodels.cpp
    src/core/grapheditorview.h
    src/core/grapheditorview.cpp
    src/core/graphnodeitem.h
    src/core/graphnodeitem.cpp
    src/core/commands.h
    src/core/commands.cpp
)

target_link_libraries(MotionEditor PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)