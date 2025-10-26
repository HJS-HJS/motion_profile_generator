cmake_minimum_required(VERSION 3.16)
project(MotionEditor_Qt5_v2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find necessary Qt5 modules
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Svg)

# Enable automatic Qt processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include current directory for generated headers
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Define the executable and list ONLY the .cpp source files
add_executable(MotionEditor
    src/main.cpp
    src/core/mainwindow.cpp
    src/core/motionmodels.cpp
    src/core/grapheditorview.cpp
    src/core/graphnodeitem.cpp
    src/core/commands.cpp
)

# Link the executable against the required Qt5 libraries
target_link_libraries(MotionEditor PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Svg
)
