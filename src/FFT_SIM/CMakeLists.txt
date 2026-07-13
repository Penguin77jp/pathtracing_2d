add_executable(FFT_SIM
    main.cpp
)

target_link_libraries(FFT_SIM PRIVATE simd_util fft_image)
