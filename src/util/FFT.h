#pragma once

#include <array>
#include <vector>
#include <stdexcept>
#include <numbers>
#include <cmath>
#include <filesystem>

namespace pg
{
	struct Complex {
		Complex() = default;
		Complex(float real, float imag) : real(real), imag(imag) {}
		Complex operator+ (const Complex& other) const {
			return Complex(real + other.real, imag + other.imag);
		}
		Complex operator- (const Complex& other) const {
			return Complex(real - other.real, imag - other.imag);
		}
		Complex operator* (const Complex& other) const {
			return Complex(real * other.real - imag * other.imag, real * other.imag + imag * other.real);
		}
		float abs() const {
			return std::sqrt(real * real + imag * imag);
		}

		float real, imag;
	};
	template <class T>
	struct Matrix {
		// row-major order
		Matrix(const std::vector<std::vector<T>>& input) {
			if (input.empty()) {
				throw std::invalid_argument("Input matrix cannot be empty.");
			}
			cols = input.size();
			rows = input[0].size();

			values.resize(rows * cols);
			for (size_t i = 0; i < cols; ++i) {
				if (input[i].size() != rows) {
					throw std::invalid_argument("All rows must have the same number of columns.");
				}
				for (size_t j = 0; j < rows; ++j) {
					values[j * cols + i] = input[i][j];
				}
			}
		}
		size_t rows, cols;
		std::vector<T> values;
		// ( v[0], v[1] )
		// ( v[2], v[3] )
	};

	inline Complex twiddle(const int k, const int N) {
		const float angle = -2.0f * std::numbers::pi * k / N;
		return Complex(std::cos(angle), std::sin(angle));
	}

	inline std::vector<Complex> FFT_recursive(const std::vector<Complex>& input) {
		if (input.size() == 1) {
			return { Complex(input[0].real, input[0].imag)};
		}
		else {
			std::vector<Complex> even, odd;
			for (size_t i = 0; i < input.size(); ++i) {
				if (i % 2 == 0) {
					even.push_back(input[i]);
				}
				else {
					odd.push_back(input[i]);
				}
			}

			std::vector<Complex> even_fft = FFT_recursive(even);
			std::vector<Complex> odd_fft = FFT_recursive(odd);
			for (int i = 0; i < odd_fft.size(); ++i) {
				odd_fft[i] = odd_fft[i] * twiddle(i, input.size());
			}

			std::vector<Complex> result(input.size());
			for (int i = 0; i < result.size() / 2; ++i) {
				result[i] = even_fft[i] + odd_fft[i];
			}
			for (int i = 0; i < result.size() / 2; ++i) {
				result[i + result.size() / 2] = even_fft[i] - odd_fft[i];
			}

			return result;
		}
	}
	inline std::vector<Complex> FFT_recursive(const std::vector<float>& input) {
		std::vector<Complex> complex_input(input.size());
		for (size_t i = 0; i < input.size(); ++i) {
			complex_input[i] = Complex(input[i], 0.0f);
		}
		return FFT_recursive(complex_input);
	}

	inline bool is_power_of_two(size_t n) {
		return (n != 0) && ((n & (n - 1)) == 0);
	}

	class FFT1DScalar {
	public:
		FFT1DScalar(const std::vector<float>& data) {
			if (data.size() == 0) {
				throw std::invalid_argument("Input matrix cannot be empty.");
			}
			if (!is_power_of_two(data.size())) {
				throw std::invalid_argument("Input size must be a power of two.");
			}

			m_size = data.size();
			m_frequancy = FFT_recursive(
				data
			);

			m_frequancy_abs.resize(m_frequancy.size());
			for (size_t i = 0; i < m_frequancy.size(); ++i) {
				m_frequancy_abs[i] = std::sqrt(m_frequancy[i].real * m_frequancy[i].real + m_frequancy[i].imag * m_frequancy[i].imag);
			}
		}
		std::vector<Complex> frequancy() const {
			return m_frequancy;
		}
		std::vector<float> frequancy_abs() const {
			return m_frequancy_abs;
		}
	private:
		size_t m_size;
		std::vector<Complex> m_frequancy;
		std::vector<float> m_frequancy_abs;
	};

	class FFT2DScalar {
	public:
		FFT2DScalar(const std::vector<std::vector<float>>& data) {
			std::vector<float> flattened_data;
			for (int i=0;i<data.size();++i){
				for (int j=0;j<data[i].size();++j){
					flattened_data.push_back(data[i][j]);
				}
			}
			*this = FFT2DScalar(flattened_data, data[0].size(), data.size());
		}
		FFT2DScalar(const std::vector<float>& data, const size_t width, const size_t height) {
			if (width == 0 || height == 0) {
				throw std::invalid_argument("Input matrix cannot be empty.");
			}
			if (data.size() != width * height) {
				throw std::invalid_argument("Input size does not match width and height.");
			}
			if (!is_power_of_two(width) || !is_power_of_two(height)) {
				throw std::invalid_argument("Input width and height must be powers of two.");
			}

			m_size = { width, height };
			m_frequancy.resize(width * height);
			// rows fft
			for (int y = 0; y < height; ++y) {
				auto result = FFT_recursive(
					std::vector<float>(
						data.begin() + y * width,
						data.begin() + (y + 1) * width
					)
				);
				for (int x = 0; x < width; ++x) {
					m_frequancy[index(x, y)] = result[x];
				}
			}

			// cols fft
			for (int x = 0; x < width; ++x) {
				std::vector<Complex> col_data(height);
				for (int y = 0; y < height; ++y) {
					col_data[y] = m_frequancy[index(x, y)];
				}
				auto result = FFT_recursive(col_data);
				for (int y = 0; y < height; ++y) {
					m_frequancy[index(x, y)] = result[y];
				}
			}

			m_frequancy_abs.resize(m_frequancy.size());
			for (size_t i = 0; i < m_frequancy.size(); ++i) {
				m_frequancy_abs[i] = m_frequancy[i].abs();
			}
		}
		size_t index(const size_t x, const size_t y) const {
			return y * m_size[0] + x;
		}
		std::vector<std::vector<Complex>> frequancy(const bool shift) const {
			const size_t width = m_size[0];
			const size_t height = m_size[1];

			std::vector<std::vector<Complex>> result(
				height,
				std::vector<Complex>(width)
			);

			for (size_t y = 0; y < height; ++y) {
				for (size_t x = 0; x < width; ++x) {
					const size_t destination_x = shift ? (x + width / 2) % width : x;
					const size_t destination_y = shift ? (y + height / 2) % height : y;
					result[destination_y][destination_x] = m_frequancy[index(x, y)];
				}
			}
			return result;
		}
		std::vector<std::vector<float>> frequancy_abs(const bool shift) const {
			const size_t width = m_size[0];
			const size_t height = m_size[1];

			std::vector<std::vector<float>> result(
				height,
				std::vector<float>(width)
			);

			for (size_t y = 0; y < height; ++y) {
				for (size_t x = 0; x < width; ++x) {
					const size_t destination_x = shift ? (x + width / 2) % width : x;
					const size_t destination_y = shift ? (y + height / 2) % height : y;
					result[destination_y][destination_x] = m_frequancy_abs[index(x, y)];
				}
			}
			return result;
		}

		
	private:
		std::array<size_t, 2> m_size;
		std::vector<Complex> m_frequancy;
		std::vector<float> m_frequancy_abs;
	};

	FFT2DScalar FFTImageRed(const std::filesystem::path& path);

}
