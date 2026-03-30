# Assuming that each submodule might have its own CMakeLists.txt or need specific handling

# Include directories for all sub-projects
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libtiled)

# Subdirectories that might contain their own CMakeLists.txt
#add_subdirectory(karchive)
add_subdirectory(libtiled)
#add_subdirectory(libtiledquick)
#add_subdirectory(plugins)
add_subdirectory(qtsingleapplication)
#add_subdirectory(terraingenerator)
add_subdirectory(tiled)
add_subdirectory(tiledapp)
#add_subdirectory(tiledquick)
#add_subdirectory(tiledquickplugin)
#add_subdirectory(tmxrasterizer)
#add_subdirectory(tmxviewer)

# If some directories do not have their own CMakeLists.txt and need to compile sources directly:
# Example adding a simple executable if needed
# add_executable(tiledapp ${CMAKE_CURRENT_SOURCE_DIR}/tiledapp/main.cpp)

# Example of linking libraries if they are built in the subdirectories
# target_link_libraries(tiledapp libtiled)

# Handling platform-specific configurations
if(WIN32 AND WINDOWS_LAYOUT)
    # Specific configuration or files to include/exclude
endif()

# DBus option handling
#if(DBUS)
#    find_package(DBus REQUIRED)
#    # Example: target_link_libraries(target_name ${DBus_LIBRARIES})
#endif()

# Handle other options like static linking with Zstandard
if(STATIC_ZSTD)
    find_package(Zstd REQUIRED)
    # Example: target_link_libraries(target_name Zstd::Zstd)
endif()
