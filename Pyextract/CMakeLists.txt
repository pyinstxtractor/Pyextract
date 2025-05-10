cmake_minimum_required(VERSION 3.16)

project(PyInstallerViewer VERSION 1.0 LANGUAGES CXX)  # Ensure versioning is defined

# Resolve absolute paths dynamically
get_filename_component(QTWINEXTRAS_PATH "${CMAKE_SOURCE_DIR}/qtwinextras-dev" ABSOLUTE)
set(BUILD_RELEASE_PATH "${CMAKE_BINARY_DIR}")  # Correctly set build path

message(STATUS "Resolved qtwinextras-dev Path: ${QTWINEXTRAS_PATH}")
message(STATUS "Resolved Build Release Path: ${BUILD_RELEASE_PATH}")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets WinExtras)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets WinExtras)

# Use dynamically resolved path for WinExtras includes (No `add_subdirectory()`)
get_filename_component(ACTUAL_INCLUDE_DIR "${QTWINEXTRAS_PATH}/src/winextras" ABSOLUTE)
message(STATUS "Resolved Include Directory: ${ACTUAL_INCLUDE_DIR}")

include_directories(${ACTUAL_INCLUDE_DIR})

set(PROJECT_SOURCES
    extractionworker.cpp
    main.cpp
    mainwindow.cpp
    pyinstarchive.cpp
    pyinstarchive.h
    mainwindow.h
    mainwindow.ui
    extractionworker.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(PyInstallerViewer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(PyInstallerViewer SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(PyInstallerViewer
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# Instead of `add_subdirectory()`, link against the precompiled library
target_link_libraries(PyInstallerViewer PRIVATE Qt${QT_VERSION_MAJOR}::Widgets "${BUILD_RELEASE_PATH}/lib/Qt6WinExtras.lib")

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.PyInstallerViewer)
endif()

set_target_properties(PyInstallerViewer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS PyInstallerViewer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(PyInstallerViewer)
endif()
