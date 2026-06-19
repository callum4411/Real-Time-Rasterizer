cmake_minimum_required(VERSION 3.25)
project(Renderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# GLFW
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM (header-only)
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1)
FetchContent_MakeAvailable(glm)

# GLAD (local generated)
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# sources
file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES}
        src/Shader.h
        src/Camera.cpp
        src/Camera.h
        src/Mesh.cpp
        src/Mesh.h
        src/Texture.cpp
        src/Texture.h
        src/Transform.h
        src/Entity.h
        src/Framebuffer.cpp
        src/Framebuffer.h)

# Single-header libraries in external/include
target_include_directories(${PROJECT_NAME} PRIVATE external/include)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad glm::glm)

# Give the program the absolute path to the assets folder as a compile-time
# string, so loadOBJ() finds models no matter what the working directory is.
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/")


