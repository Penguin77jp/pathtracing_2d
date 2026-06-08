cmake_minimum_required(VERSION 3.16)

project(pathtracing_2d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

find_package(OpenGL REQUIRED)
find_package(OpenMP QUIET)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
 )
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(pathtracing_2d
    src/main.cpp
    src/pt2d/App.cpp
    src/pt2d/Integrator.cpp
    src/pt2d/Scene.cpp
    src/pt2d/PngWriter.cpp
    src/pt2d/SceneSerializer.cpp
    src/pt2d/ProductionApp.cpp
    src/pt2d/RISDirection.cpp
 )

target_include_directories(pathtracing_2d PRIVATE src)
target_link_libraries(pathtracing_2d PRIVATE imgui glfw OpenGL::GL)

if(OpenMP_CXX_FOUND)
    target_link_libraries(pathtracing_2d PRIVATE OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP enabled")
else()
    message(WARNING "OpenMP was not found; production rendering will build without OpenMP parallelism")
endif()

if(APPLE)
    target_compile_definitions(pathtracing_2d PRIVATE GL_SILENCE_DEPRECATION)
endif()
