cmake_minimum_required(VERSION 3.20)

project(openscreen-pipewire-capture LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0 gio-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-base-1.0)

add_executable(openscreen-pipewire-capture
  src/main.c
)

target_include_directories(openscreen-pipewire-capture PRIVATE
  ${GLIB_INCLUDE_DIRS}
  ${GSTREAMER_INCLUDE_DIRS}
)

target_link_libraries(openscreen-pipewire-capture PRIVATE
  ${GLIB_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
)

target_compile_options(openscreen-pipewire-capture PRIVATE -Wall -Wextra)
