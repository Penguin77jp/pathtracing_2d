#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "math.h"

#if __has_include(<stb_image_write.h>)
    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include <stb_image_write.h>
    #define PG_HAS_STB_IMAGE_WRITE 1
#elif __has_include(<stb/stb_image_write.h>)
    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include <stb/stb_image_write.h>
    #define PG_HAS_STB_IMAGE_WRITE 1
#else
    #define PG_HAS_STB_IMAGE_WRITE 0
#endif

namespace pg {

class Image {
public:
    Image(int width, int height)
        : m_width(width),
          m_height(height),
          m_pixels(static_cast<std::size_t>(width * height * 3), 0) {}

    void store_linear_color(int x, int y, const Vec3f& linear_color) {
        const std::size_t index = get_index(x, y);
        m_pixels[index + 0] = encode_channel(linear_color.x);
        m_pixels[index + 1] = encode_channel(linear_color.y);
        m_pixels[index + 2] = encode_channel(linear_color.z);
    }

    bool write(const std::string& path) const {
        if (ends_with(path, ".png")) {
#if PG_HAS_STB_IMAGE_WRITE
            const int stride_bytes = m_width * 3;
            return stbi_write_png(
                path.c_str(),
                m_width,
                m_height,
                3,
                m_pixels.data(),
                stride_bytes
            ) != 0;
#else
            return false;
#endif
        }

        return write_ppm(path);
    }

    static constexpr bool png_supported() noexcept {
        return PG_HAS_STB_IMAGE_WRITE != 0;
    }

private:
    std::size_t get_index(int x, int y) const {
        return static_cast<std::size_t>((y * m_width + x) * 3);
    }

    static unsigned char encode_channel(float linear_value) {
        // Linear RGB -> gamma 2.0 display RGB, following the book.
        const float gamma_value = linear_value > 0.0f
            ? std::sqrt(linear_value)
            : 0.0f;
        const float clamped = std::clamp(gamma_value, 0.0f, 0.999f);
        return static_cast<unsigned char>(256.0f * clamped);
    }

    bool write_ppm(const std::string& path) const {
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            return false;
        }
        output << "P6\n" << m_width << ' ' << m_height << "\n255\n";
        output.write(
            reinterpret_cast<const char*>(m_pixels.data()),
            static_cast<std::streamsize>(m_pixels.size())
        );
        return output.good();
    }

    static bool ends_with(const std::string& value, const std::string& suffix) {
        return value.size() >= suffix.size()
            && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    int m_width;
    int m_height;
    std::vector<unsigned char> m_pixels;
};

} // namespace pg
