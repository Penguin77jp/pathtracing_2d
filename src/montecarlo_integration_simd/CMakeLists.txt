add_executable(montecarlo_integration_simd
    main.cpp
)

if(MSVC)
    target_compile_options(montecarlo_integration_simd PRIVATE /arch:AVX2)
else()
    target_compile_options(montecarlo_integration_simd PRIVATE -mavx2)
endif()
