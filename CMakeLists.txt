cmake_minimum_required(VERSION 3.25.1)
project(myncdu)

set(C_STANDARD 11)

file (GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")

add_executable(myncdu ${SOURCES})
include_directories("header")
