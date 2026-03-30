cmake_minimum_required(VERSION 3.12)
project(qtsingleapplication)

## Specify the C++ standard
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)

# Find the required Qt packages
find_package(Qt6 COMPONENTS Widgets Network REQUIRED)  # Adjust to Qt5 if using Qt5
# Add a static library target
add_library(qtsingleapplication STATIC
        src/qtlocalpeer.cpp
        src/qtlocalpeer.h
        src/qtsingleapplication.cpp
        src/qtsingleapplication.h
)

# Include directories
target_include_directories(qtsingleapplication PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link the Qt libraries
target_link_libraries(qtsingleapplication Qt6::Widgets Qt6::Network)
#install(TARGETS qtsingleapplication
#        ARCHIVE DESTINATION lib
#        LIBRARY DESTINATION lib
#        RUNTIME DESTINATION bin
#)
#
#install(FILES
#        src/qtlocalpeer.h
#        src/qtsingleapplication.h
#        DESTINATION include/qtsingleapplication
#)
