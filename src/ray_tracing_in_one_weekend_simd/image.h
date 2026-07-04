#pragma once

#include <vector>
#include <algorithm>
#include <stb_image_write.h>

#include "math.h"

namespace pg {

class Image {
public:
    Image(int width, int height)
        : m_width(width),
          m_height(height),
          m_pixels(width * height * 3, 0) {
    }
    size_t get_index(const int x, const int y) const {
        return (y * m_width + x) * 3;
	}
    size_t get_index(const int x, const int y, const int c) const {
        return (y * m_width + x) * 3 + c;
    }
    void store_packet(int x, int y, const Vec3f& color) {
        const auto index = get_index(x, y);
        m_pixels[index + 0] = std::clamp(static_cast<int>(color.x * 255.0f), 0, 255);
        m_pixels[index + 1] = std::clamp(static_cast<int>(color.y * 255.0f), 0, 255);
        m_pixels[index + 2] = std::clamp(static_cast<int>(color.z * 255.0f), 0, 255);
    }

    bool write_png(const char* path) const {
        const int stride_bytes = m_width * 3;
        return stbi_write_png(
            path,
            m_width,
            m_height,
            3,
            m_pixels.data(),
            stride_bytes
        ) != 0;
    }

private:
    int m_width;
    int m_height;
    std::vector<unsigned char> m_pixels;
};

}
