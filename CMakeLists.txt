cmake_minimum_required(VERSION 3.31)

project(jkdns VERSION "1.0.0" LANGUAGES C)

set(SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(SOURCE_FILES)

set(IGNORE_FILES "${SRCDIR}/ignore_this.c;")

file(GLOB_RECURSE SOURCE_FILES "${SRCDIR}/*.c")
file(GLOB_RECURSE HEADER_FILES "${SRCDIR}/*.h")

list(REMOVE_ITEM SOURCE_FILES ${IGNORE_FILES})
list(REMOVE_ITEM HEADER_FILES ${IGNORE_FILES})

file(GLOB_RECURSE OS_SOURCE "${SRCDIR}/os/*.c")
file(GLOB_RECURSE OS_HEADERS "${SRCDIR}/os/*.h")

list(REMOVE_ITEM SOURCE_FILES ${OS_SOURCE})
list(REMOVE_ITEM HEADER_FILES ${OS_HEADERS})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_options(/W4 /std:c11 /Zi)

    file(GLOB_RECURSE WINDOWS_SOURCE "${SRCDIR}/os/windows/*.c")
    file(GLOB_RECURSE WINDOWS_HEADERS "${SRCDIR}/os/windows/*.h")
    list(APPEND SOURCE_FILES ${WINDOWS_SOURCE})
    list(APPEND HEADER_FILES ${WINDOWS_HEADERS})
    message("${SOURCE_FILES}")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_options(-Wall -Wextra -std=c11 -D_GNU_SOURCE -g)

    file(GLOB_RECURSE LINUX_SOURCE "${SRCDIR}/os/linux/*.c")
    file(GLOB_RECURSE LINUX_HEADERS "${SRCDIR}/os/linux/*.h")
    list(APPEND SOURCE_FILES ${LINUX_SOURCE})
    list(APPEND HEADER_FILES ${LINUX_HEADERS})
    message("${SOURCE_FILES}")
endif()

add_executable(jkdns ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(jkdns PRIVATE ${SRCDIR})
