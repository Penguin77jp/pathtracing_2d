#include <cstdio>

#include "util/math.h"
#include "util/FFT.h"

int main() {

    std::vector<std::vector<float>> input_fft;
    input_fft =
    {
        {1.f, 2.f},
    };

    pg::FFT2DScalar fft(input_fft);
    for (const auto& c : fft.frequancy(false)) {
        for (const auto& v : c) {
            printf("%f j%f ", v.real, v.imag);
		}
		printf("\n");
	}


    return 0;
}
