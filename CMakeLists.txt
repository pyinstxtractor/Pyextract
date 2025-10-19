cmake_minimum_required(VERSION 3.15)
project(PyInstaller_CPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(SOURCES
    src/PyInstaller.cpp
    src/ThreadPool.cpp
)

set(HEADERS
    include/PyInstArchive.h
    include/ThreadPool.h
    include/zconf.h
    include/zlib.h
)

add_executable(PyExtract ${SOURCES} ${HEADERS})

if(WIN32)
    target_link_libraries(PyExtract PRIVATE ${PROJECT_SOURCE_DIR}/lib/libz-static.lib)
elseif(UNIX)
    target_link_libraries(PyExtract PRIVATE ${PROJECT_SOURCE_DIR}/lib/libz.a)
endif()