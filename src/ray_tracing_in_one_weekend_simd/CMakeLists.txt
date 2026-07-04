add_executable(ray_tracing_in_one_weekend_simd
    main.cpp
    camera.h
    image.h
    math.h
    random.h
    scene.h
)

target_link_libraries(ray_tracing_in_one_weekend_simd PRIVATE stb)

if(MSVC)
    target_compile_options(ray_tracing_in_one_weekend_simd PRIVATE /arch:AVX2)
else()
    target_compile_options(ray_tracing_in_one_weekend_simd PRIVATE -mavx2)
endif()
