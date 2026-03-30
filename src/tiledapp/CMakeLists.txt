cmake_minimum_required(VERSION 3.20)
project(tiledapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_BASE_DIR "C:/Qt/6.9.1/mingw_64/include")

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
)

# Source files
set(SOURCE_FILES
        commandlineparser.cpp
        commandlineparser.h
        main.cpp
)
qt_add_resources(TILEDAPP_RESOURCES "tiledapp.qrc")

add_executable(tiledapp ${SOURCE_FILES} ${TILEDAPP_RESOURCES})
include_directories("../tiled")
include_directories("../libtiled")

target_include_directories(tiledapp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${QT_BASE_DIR}/QtCore/6.9.1
        ${QT_BASE_DIR}/QtCore/6.9.1/QtCore
        ${QT_BASE_DIR}/QtCore/6.9.1/QtCore/private
        ${QT_BASE_DIR}/QtGui/6.9.1
        ${QT_BASE_DIR}/QtGui/6.9.1/QtGui
        ${QT_BASE_DIR}/QtGui/6.9.1/QtGui/private
        ${QT_BASE_DIR}/QtGui/6.9.1/QtGui/qpa
)
# Dependencies
target_link_libraries(tiledapp PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        tilededitor
)

## Only on Windows, configure and include the .rc file
#if(WIN32)
#    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tiled.rc.in
#            ${CMAKE_CURRENT_BINARY_DIR}/tiled.rc @ONLY)
#
#    target_sources(tiledapp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tiled.rc)
#endif()

# Optional Sentry configuration
if(TILED_SENTRY)
    target_compile_definitions(tiledapp PRIVATE TILED_SENTRY)
    target_include_directories(tiledapp PRIVATE ${CMAKE_SOURCE_DIR}/../../sentry-native/install/include)
endif()

