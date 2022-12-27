cmake_minimum_required(VERSION 3.14...3.25)

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT (CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Release default")
endif()

project(PL3D-KC LANGUAGES C)

if(UNIX)
  # apt install libx11-dev libxext-dev
  # brew install libx11 xquartz
  find_package(X11 REQUIRED)
endif()

add_library(fw OBJECT fw/pkb.c fw/sys.c
$<$<BOOL:${WIN32}>:fw/wvid.c>
$<$<BOOL:${UNIX}>:fw/xvid.c>
)
target_include_directories(fw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fw)
target_link_libraries(fw PRIVATE
$<$<BOOL:${UNIX}>:X11::Xext>
$<$<BOOL:${WIN32}>:winmm>
)

add_library(pl OBJECT clip.c gfx.c imode.c importer.c math.c pl.c)
target_include_directories(pl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(pl PRIVATE $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_WARNINGS>)

add_executable(main main.c)
target_link_libraries(main PRIVATE pl fw)


# --- auto-ignore build directory
if(NOT EXISTS ${PROJECT_BINARY_DIR}/.gitignore)
  file(WRITE ${PROJECT_BINARY_DIR}/.gitignore "*")
endif()
