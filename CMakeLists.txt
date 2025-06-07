cmake_minimum_required(VERSION 3.10)
project(GameBoyEmulator C)

set(COMPILE_FLAGS "-O2 -g -Wall -Wextra -pedantic")
set(COMPILE_LIBS "-lpthread")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMPILE_FLAGS} ${COMPILE_LIBS}")
file(GLOB SRC "sources/*.c")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

include_directories(${SDL2_INCLUDE_DIRS})
link_directories(${SDL2_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
