cmake_minimum_required(VERSION 3.31)

project(jkdns VERSION "1.0.0" LANGUAGES C)

add_compile_options(-Wall -Wextra -std=c11 -D_GNU_SOURCE -g)

set(SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(SOURCE_FILES)

set(IGNORE_FILES "${SRCDIR}/somethig.c;")

file(GLOB_RECURSE SOURCE_FILES "${SRCDIR}/*.c")
file(GLOB_RECURSE HEADER_FILES "${SRCDIR}/*.h")

list(REMOVE_ITEM SOURCE_FILES ${IGNORE_FILES})
list(REMOVE_ITEM HEADER_FILES ${IGNORE_FILES})

add_executable(jkdns ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(jkdns PRIVATE ${SRCDIR})
