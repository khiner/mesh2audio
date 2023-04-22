cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -stdlib=libc++")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")

project(mesh2audio LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJC)
endif()

add_subdirectory(lib)

file(
    GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp
    lib/implot/implot.cpp lib/implot/implot_items.cpp lib/implot/implot_demo.cpp
    lib/fmt/src/format.cc
    lib/imgui/misc/cpp/imgui_stdlib.cpp
    lib/ImGuizmo/ImGuizmo.cpp
)

# Recursively copy the res directory to the build directory.
file(COPY res DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(${PROJECT_NAME} ${SOURCES})

set(INCLUDE_EXECUTABLE OFF CACHE STRING "Build the mesh2faust executable" FORCE)
set(Mesh2FaustDir ${CMAKE_CURRENT_SOURCE_DIR}/lib/faust/tools/physicalModeling/mesh2faust)
add_subdirectory(${Mesh2FaustDir})

set(CINOLIB_USES_TETGEN ON)
set(cinolib_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/cinolib)
find_package(cinolib REQUIRED)

include_directories(
    lib/implot
    lib/imspinner
    lib/glm
    lib/miniaudio
    lib/fmt/include
    lib/faust/architecture
    lib/ImGuizmo
    lib/nanosvg/src
    lib/earcut/include/mapbox
    lib/CDT/CDT/include
    ${Mesh2FaustDir}/src
)

find_package(GLEW REQUIRED)

if(APPLE)
  target_include_directories(${PROJECT_NAME} PRIVATE /opt/homebrew/Cellar/eigen/3.4.0_1/include/eigen3)
else()
  target_include_directories(${PROJECT_NAME} PRIVATE /usr/include/eigen3)
endif()

# `dynamiclib` is faust.
target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI GLEW::GLEW nfd dynamiclib mesh2faust cinolib)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-elaborated-enum-base -DIMGUI_IMPL_OPENGL_LOADER_GLEW)
