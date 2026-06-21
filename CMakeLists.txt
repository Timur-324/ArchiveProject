cmake_minimum_required(VERSION 3.16)

project(
    ArchiveProject
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    archive

    src/main.cpp

    src/Archive/Archive.cpp
)

target_include_directories(
    archive

    PRIVATE

    src
    src/Archive
    src/Models
)

if (MSVC)

    target_compile_options(
        archive
        PRIVATE
        /W4
    )

else()

    target_compile_options(
        archive
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()