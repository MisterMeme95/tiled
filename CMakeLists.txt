cmake_minimum_required(VERSION 3.10)  # Set the minimum required version of CMake
project(Tiled VERSION "1.11.2")  # Define the project name and version
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
# Set the macro
add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x050000)

# Get compile definitions for the current directory and print them
get_directory_property(CompileDefinitions COMPILE_DEFINITIONS)
message(STATUS "Compile definitions: ${CompileDefinitions}")
#find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Set version from environment variable or use default
set(TILED_VERSION $ENV{TILED_VERSION})
if(NOT TILED_VERSION)
    set(TILED_VERSION "1.11.2")
endif()

# Options based on environment variables
option(SNAPSHOT "Build the snapshot version" $ENV{TILED_SNAPSHOT})
option(RELEASE "Build the release version" $ENV{TILED_RELEASE})
option(USE_RPATHS "Use RPATHs when linking" ON)
option(STATIC_ZSTD "Link statically against Zstandard" OFF)
option(SENTRY "Enable Sentry integration" OFF)
option(DBUS "Use D-Bus" ON)
option(INSTALL_HEADERS "Install headers" OFF)
option(WINDOWS_INSTALLER "Create a Windows installer" OFF)

# Set paths and platform-specific options
set(LIB_DIR "lib")
set(OPENSSL_PATH $ENV{OPENSSL_PATH})
set(PYTHON_PKG_CONFIG_NAME "python3-embed")

if(WIN32)
    set(WINDOWS_LAYOUT ON)
else()
    set(WINDOWS_LAYOUT OFF)
endif()

# Include directories and subdirectories
#include_directories(${CMAKE_SOURCE_DIR}/build/Desktop_Qt_6_8_0_MinGW_64_bit-Debug/Debug_Desktop__d69bacc25dcd8cf2/libtilededitor.877b2b3a/qt.headers)
include_directories(${CMAKE_SOURCE_DIR}/src)
#add_subdirectory(dist)
#add_subdirectory(docs)
add_subdirectory(src)
#add_subdirectory(tests)
#add_subdirectory(translations)

# Configure test settings
enable_testing()
add_test(NAME TiledTests COMMAND tests)
