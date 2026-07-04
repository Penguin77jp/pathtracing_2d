add_executable(pathtracing_2d
    main.cpp
    pt2d/App.cpp
    pt2d/Integrator.cpp
    pt2d/Scene.cpp
    pt2d/PngWriter.cpp
    pt2d/SceneSerializer.cpp
    pt2d/ProductionApp.cpp
    pt2d/RISDirection.cpp
)

target_include_directories(pathtracing_2d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pathtracing_2d PRIVATE imgui glfw OpenGL::GL)

if(OpenMP_CXX_FOUND)
    target_link_libraries(pathtracing_2d PRIVATE OpenMP::OpenMP_CXX)
endif()

if(APPLE)
    target_compile_definitions(pathtracing_2d PRIVATE GL_SILENCE_DEPRECATION)
endif()
