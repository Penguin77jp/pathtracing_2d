cmake_minimum_required(VERSION 3.20)
project(ray_tracing_in_one_weekend_simd LANGUAGES CXX)

add_executable(ray_tracing_in_one_weekend_simd
    main.cpp
    camera.h
    image.h
    material.h
    math.h
    path_tracer.h
    random.h
    random_sampling.h
    render_config.h
    scatter.h
    scene.h
    scene_factory.h
)

target_compile_features(ray_tracing_in_one_weekend_simd PRIVATE cxx_std_20)
target_link_libraries(ray_tracing_in_one_weekend_simd PRIVATE stb)

if(OpenMP_CXX_FOUND)
    target_link_libraries(ray_tracing_in_one_weekend_simd PRIVATE OpenMP::OpenMP_CXX)
endif()
if(MSVC)
    target_compile_options(ray_tracing_in_one_weekend_simd PRIVATE /arch:AVX2)
else()
    # _mm256_fmadd_ps requires FMA in addition to AVX2.
    target_compile_options(ray_tracing_in_one_weekend_simd PRIVATE -mavx2 -mfma)
endif()
