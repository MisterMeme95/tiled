    cmake_minimum_required(VERSION 3.20)
    project(tiledapp LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
#    set(QT_BASE_DIR "C:/Qt/6.9.1/mingw_64/include")

    find_package(Qt6 REQUIRED COMPONENTS
            Core
            CorePrivate
            Gui
            GuiPrivate
            Widgets
            Qml
            Network
            Svg
            OpenGL
            OpenGLWidgets
            Concurrent
    )
    # No .qrc files listed here!
    set(SOURCE_FILES
            commandlineparser.cpp
            commandlineparser.h
            main.cpp
    )

    #add_executable(tiledapp ${SOURCE_FILES})
    qt_add_executable(tiledapp ${SOURCE_FILES})
    target_include_directories(tiledapp PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ../tiled
            ../libtiled
            ../qtsingleapplication/src
    )
#    include_directories("../tiled")
#    include_directories("../libtiled")
#    if(NOT EMSCRIPTEN)
#    target_include_directories(tiledapp PRIVATE
#            ${CMAKE_CURRENT_SOURCE_DIR}
#            ${QT_BASE_DIR}/QtCore/6.9.1
#            ${QT_BASE_DIR}/QtCore/6.9.1/QtCore
#            ${QT_BASE_DIR}/QtCore/6.9.1/QtCore/private
#            ${QT_BASE_DIR}/QtGui/6.9.1
#            ${QT_BASE_DIR}/QtGui/6.9.1/QtGui
#            ${QT_BASE_DIR}/QtGui/6.9.1/QtGui/private
#            ${QT_BASE_DIR}/QtGui/6.9.1/QtGui/qpa
#    )
#    endif()
    target_link_libraries(tiledapp PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::GuiPrivate
            Qt6::Widgets
            Qt6::Qml
            Qt6::Network
            Qt6::Concurrent

            tilededitor
            tiled
            qtsingleapplication
    )



    if(WIN32 AND NOT EMSCRIPTEN)
        get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

        set(ZLIB_RUNTIME_DIR "${CMAKE_SOURCE_DIR}/src/__include/zlib/install/bin")

        add_custom_command(TARGET tiledapp POST_BUILD
                # Let Qt deploy normal runtime dependencies/plugins
                COMMAND "${QT_BIN_DIR}/windeployqt.exe"
                "$<TARGET_FILE:tiledapp>"

                # Force-copy Qt DLLs that windeployqt may miss through indirect deps
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::Qml>"
                "$<TARGET_FILE:Qt6::Network>"
                "$<TARGET_FILE:Qt6::Svg>"
                "$<TARGET_FILE:Qt6::OpenGL>"
                "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE:Qt6::Concurrent>"
                "$<TARGET_FILE_DIR:tiledapp>"

                # Copy Tiled project DLLs
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:tilededitor>"
                "$<TARGET_FILE:tiled>"
                "$<TARGET_FILE_DIR:tiledapp>"

                # Copy zlib runtime DLLs
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ZLIB_RUNTIME_DIR}/libz.dll"
                "${ZLIB_RUNTIME_DIR}/libzlib.dll"
                "$<TARGET_FILE_DIR:tiledapp>"

                COMMENT "Deploying tiledapp runtime dependencies"
        )
    endif()

    if(EMSCRIPTEN)
        target_link_options(tiledapp PRIVATE "-sUSE_ZLIB=1" "-sUSE_LIBPNG=1" "--bind" "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
    #    set_target_properties(tiledapp PROPERTIES LINK_FLAGS "--bind -lembind")
    #    qt_add_wasm_support(tiledapp)
#        set_target_properties(tiledapp PROPERTIES LINK_FLAGS "--bind -lembind")

    endif()

    if(TILED_SENTRY)
        target_compile_definitions(tiledapp PRIVATE TILED_SENTRY)
        target_include_directories(tiledapp PRIVATE ${CMAKE_SOURCE_DIR}/../../sentry-native/install/include)
    endif()

