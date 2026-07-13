cmake_minimum_required(VERSION 3.20)
project(ray_tracing_in_one_weekend_simd LANGUAGES CXX)

add_executable(ray_tracing_in_one_weekend_simd
    main.cpp
    camera.h
    image.h
    material.h
    path_tracer.h
    random.h
    random_sampling.h
    render_config.h
    scatter.h
    scene.h
    scene_factory.h
)

target_compile_features(ray_tracing_in_one_weekend_simd PRIVATE cxx_std_20)
target_link_libraries(ray_tracing_in_one_weekend_simd PRIVATE stb simd_util)

if(OpenMP_CXX_FOUND)
    target_link_libraries(ray_tracing_in_one_weekend_simd PRIVATE OpenMP::OpenMP_CXX)
endif()
