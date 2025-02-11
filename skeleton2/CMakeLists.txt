cmake_minimum_required(VERSION 3.10)
project(BallsSimulation)

set(CMAKE_CXX_STANDARD 17)

include_directories(
  ${CMAKE_SOURCE_DIR}/glimac/third-party/glad/include
  ${CMAKE_SOURCE_DIR}/glimac
  ${CMAKE_SOURCE_DIR}/external/imgui
  ${CMAKE_SOURCE_DIR}/external/imgui/backends
)

file(GLOB SRC_FILES src/*.cpp)

# Add ImGui source files (including SDL2 backend)
file(GLOB IMGUI_SRC
  external/imgui/*.cpp
  external/imgui/backends/imgui_impl_sdl2.cpp
  external/imgui/backends/imgui_impl_opengl3.cpp
)

# Create executable with all sources
add_executable(BallsSimulation ${SRC_FILES} ${IMGUI_SRC})

# Find and link SDL2 using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
target_include_directories(BallsSimulation PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_directories(BallsSimulation PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(BallsSimulation ${SDL2_LIBRARIES})
target_compile_options(BallsSimulation PRIVATE ${SDL2_CFLAGS_OTHER})

find_package(glm REQUIRED)
include_directories(/opt/homebrew/include)

# Add GLAD library
add_library(glad STATIC glimac/third-party/glad/src/glad.c) 
target_include_directories(glad PUBLIC glimac/third-party/glad/include)

# Link GLAD to BallsSimulation
target_link_libraries(BallsSimulation glad)