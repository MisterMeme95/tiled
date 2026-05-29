cmake_minimum_required(VERSION 3.16)  # Updated for better Qt6 support
project(tiled VERSION "1.11.2" LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define options for static Zstd (if needed)
option(STATIC_ZSTD "Use static Zstd" OFF)

# Find required Qt packages
find_package(Qt6 COMPONENTS Gui REQUIRED)

# Include Zstd if needed
find_package(PkgConfig)
pkg_check_modules(ZSTD libzstd)

# Add definitions based on project settings and dependencies
add_compile_definitions(
        TILED_LIBRARY
        TILED_LIB_DIR="${PROJECT_SOURCE_DIR}/lib"
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        QT_NO_URL_CAST_FROM_STRING
        QT_DISABLE_DEPRECATED_BEFORE=0x060000  # Adjust as per Qt 6's deprecated APIs
        QT_NO_DEPRECATED_WARNINGS
        _USE_MATH_DEFINES
)

if(ZSTD_FOUND OR STATIC_ZSTD)
    add_compile_definitions(TILED_ZSTD_SUPPORT)
endif()

# Source files
set(SOURCE_FILES
        compression.cpp
        compression.h
        containerhelpers.h
        fileformat.cpp
        fileformat.h
        filesystemwatcher.cpp
        filesystemwatcher.h
        gidmapper.cpp
        gidmapper.h
        grid.h
        grouplayer.cpp
        grouplayer.h
        hex.cpp
        hex.h
        hexagonalrenderer.cpp
        hexagonalrenderer.h
        obliquerenderer.cpp
        obliquerenderer.h
        imagecache.cpp
        imagecache.h
        imagelayer.cpp
        imagelayer.h
        imagereference.cpp
        imagereference.h
        isometricrenderer.cpp
        isometricrenderer.h
        layer.cpp
        layer.h
        logginginterface.cpp
        logginginterface.h
        map.cpp
        map.h
        mapformat.cpp
        mapformat.h
        mapobject.cpp
        mapobject.h
        mapreader.cpp
        mapreader.h
        maprenderer.cpp
        maprenderer.h
        maptovariantconverter.cpp
        maptovariantconverter.h
        mapwriter.cpp
        mapwriter.h
        minimaprenderer.cpp
        minimaprenderer.h
        object.cpp
        object.h
        objectgroup.cpp
        objectgroup.h
        objecttemplate.cpp
        objecttemplate.h
        objecttemplateformat.cpp
        objecttemplateformat.h
        objecttypes.cpp
        objecttypes.h
        orthogonalrenderer.cpp
        orthogonalrenderer.h
        plugin.cpp
        plugin.h
        pluginmanager.cpp
        pluginmanager.h
        properties.cpp
        properties.h
        propertytype.cpp
        propertytype.h
        savefile.cpp
        savefile.h
        staggeredrenderer.cpp
        staggeredrenderer.h
        templatemanager.cpp
        templatemanager.h
        tile.cpp
        tileanimationdriver.cpp
        tileanimationdriver.h
        tiled.cpp
        tiled_global.h
        tiled.h
        tile.h
        tilelayer.cpp
        tilelayer.h
        tileset.cpp
        tileset.h
        tilesetformat.cpp
        tilesetformat.h
        tilesetmanager.cpp
        tilesetmanager.h
        tmxmapformat.cpp
        tmxmapformat.h
        varianttomapconverter.cpp
        varianttomapconverter.h
        wangset.cpp
        wangset.h
        world.cpp
        world.h
)
find_package(ZLIB REQUIRED)

## Create the dynamic library
#add_library(tiled STATIC ${SOURCE_FILES})
# Create the library conditionally based on the target platform
if(EMSCRIPTEN)
    # WebAssembly needs everything statically linked into the final .wasm binary
    add_library(tiled STATIC ${SOURCE_FILES})
else()
    # Windows/Desktop expects a .dll because of the Tiled export macros
    add_library(tiled SHARED ${SOURCE_FILES})
endif()
# Link libraries
target_link_libraries(tiled
        Qt6::Gui
        ZLIB::ZLIB
)

# Zstd library handling
if(ZSTD_FOUND AND NOT STATIC_ZSTD)
    target_link_libraries(tiled ${ZSTD_LIBRARIES})
    target_include_directories(tiled PRIVATE ${ZSTD_INCLUDE_DIRS})
endif()

if(STATIC_ZSTD AND ZSTD_FOUND)
    target_link_libraries(tiled ${ZSTD_STATIC_LIBRARIES})
elseif(STATIC_ZSTD)
    # Assuming static Zstd library is placed at a specific location
    target_link_libraries(tiled ${PROJECT_SOURCE_DIR}/../../zstd/lib/libzstd.a)
endif()

# Installation rules
if(NOT APPLE)
    install(TARGETS tiled LIBRARY DESTINATION ${PROJECT_SOURCE_DIR}/lib)
endif()

# Export settings
# Note: Use modern CMake practices for managing targets and dependencies
