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
  "src/util/FFT.h")
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_library(eigen INTERFACE)
target_include_directories(eigen SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/external/eigen
)
if(MSVC)
    target_compile_options(eigen INTERFACE /wd4819)
endif()

add_library(stb INTERFACE)
if(EXISTS "${PROJECT_SOURCE_DIR}/external/stb/stb_image.h")
    target_include_directories(stb INTERFACE
        ${PROJECT_SOURCE_DIR}/external/stb
    )
else()
    FetchContent_Declare(
        stb_headers
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
    )
    FetchContent_MakeAvailable(stb_headers)
    target_include_directories(stb INTERFACE
        ${stb_headers_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/external/stb
    )
endif()

if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP enabled")
else()
    message(WARNING "OpenMP was not found; production rendering will build without OpenMP parallelism")
endif()

add_subdirectory(src/util)
add_subdirectory(src/montecarlo_integration_simd)
add_subdirectory(src/ray_tracing_in_one_weekend_simd)
add_subdirectory(src/FFT_SIM)
add_subdirectory(src/raytracing)
