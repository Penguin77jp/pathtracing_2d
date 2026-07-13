#include "util/FFT.h"

#include <limits>
#include <memory>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace pg
{
	namespace
	{
		struct StbiImageDeleter {
			void operator()(stbi_uc* pixels) const noexcept {
				stbi_image_free(pixels);
			}
		};
	}

	FFT2DScalar FFTImageRed(const std::filesystem::path& path) {
		int width = 0;
		int height = 0;
		int channels = 0;
		const std::string path_string = path.string();
		std::unique_ptr<stbi_uc, StbiImageDeleter> pixels(
			stbi_load(path_string.c_str(), &width, &height, &channels, 0)
		);

		if (!pixels) {
			const char* reason = stbi_failure_reason();
			throw std::runtime_error(
				"Failed to load image '" + path_string + "': " +
				(reason != nullptr ? reason : "unknown error")
			);
		}
		if (width <= 0 || height <= 0 || channels <= 0) {
			throw std::runtime_error("Loaded image has invalid dimensions or channels: " + path_string);
		}

		const size_t image_width = static_cast<size_t>(width);
		const size_t image_height = static_cast<size_t>(height);
		if (image_width > std::numeric_limits<size_t>::max() / image_height) {
			throw std::overflow_error("Image dimensions are too large: " + path_string);
		}

		std::vector<float> red_values(image_width * image_height);
		for (size_t i = 0; i < red_values.size(); ++i) {
			red_values[i] = static_cast<float>(pixels.get()[i * static_cast<size_t>(channels)]) / 255.0f;
		}

		return FFT2DScalar(red_values, image_width, image_height);
	}
}
