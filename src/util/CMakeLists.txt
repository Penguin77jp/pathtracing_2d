add_library(simd_util INTERFACE)

target_include_directories(simd_util INTERFACE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(simd_util INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(simd_util INTERFACE /arch:AVX2)
else()
    # _mm256_fmadd_ps requires FMA in addition to AVX2.
    target_compile_options(simd_util INTERFACE -mavx2 -mfma)
endif()

add_library(fft_image STATIC
    FFT.cpp
)

target_link_libraries(fft_image PUBLIC simd_util PRIVATE stb)
