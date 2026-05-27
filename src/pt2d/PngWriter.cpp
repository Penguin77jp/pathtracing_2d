#include "pt2d/PngWriter.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace pt2d {
namespace {

void write_u32_be(std::FILE* f, std::uint32_t v) {
    const unsigned char bytes[4] = {
        static_cast<unsigned char>((v >> 24) & 0xFF),
        static_cast<unsigned char>((v >> 16) & 0xFF),
        static_cast<unsigned char>((v >> 8) & 0xFF),
        static_cast<unsigned char>(v & 0xFF),
    };
    std::fwrite(bytes, 1, 4, f);
}

std::uint32_t crc32_bytes(const unsigned char* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

std::uint32_t adler32_bytes(const unsigned char* data, std::size_t size) {
    std::uint32_t s1 = 1;
    std::uint32_t s2 = 0;
    for (std::size_t i = 0; i < size; ++i) {
        s1 = (s1 + data[i]) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    return (s2 << 16) | s1;
}

bool write_chunk(std::FILE* f, const char type[4], const std::vector<unsigned char>& data) {
    write_u32_be(f, static_cast<std::uint32_t>(data.size()));
    std::fwrite(type, 1, 4, f);
    if (!data.empty()) {
        std::fwrite(data.data(), 1, data.size(), f);
    }

    std::vector<unsigned char> crc_data;
    crc_data.reserve(4 + data.size());
    crc_data.insert(crc_data.end(), type, type + 4);
    crc_data.insert(crc_data.end(), data.begin(), data.end());
    write_u32_be(f, crc32_bytes(crc_data.data(), crc_data.size()));
    return std::ferror(f) == 0;
}

std::vector<unsigned char> make_zlib_stored_stream(const std::vector<unsigned char>& raw) {
    std::vector<unsigned char> out;
    out.reserve(raw.size() + raw.size() / 65535u * 5u + 16u);

    out.push_back(0x78); // zlib header, 32K window
    out.push_back(0x01); // fastest algorithm / no compression

    std::size_t offset = 0;
    while (offset < raw.size()) {
        const std::size_t chunk = std::min<std::size_t>(65535u, raw.size() - offset);
        const bool final_block = (offset + chunk == raw.size());
        out.push_back(final_block ? 0x01 : 0x00); // BFINAL + stored block

        const std::uint16_t len = static_cast<std::uint16_t>(chunk);
        const std::uint16_t nlen = static_cast<std::uint16_t>(~len);
        out.push_back(static_cast<unsigned char>(len & 0xFF));
        out.push_back(static_cast<unsigned char>((len >> 8) & 0xFF));
        out.push_back(static_cast<unsigned char>(nlen & 0xFF));
        out.push_back(static_cast<unsigned char>((nlen >> 8) & 0xFF));
        out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset), raw.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
        offset += chunk;
    }

    const std::uint32_t adler = adler32_bytes(raw.data(), raw.size());
    out.push_back(static_cast<unsigned char>((adler >> 24) & 0xFF));
    out.push_back(static_cast<unsigned char>((adler >> 16) & 0xFF));
    out.push_back(static_cast<unsigned char>((adler >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(adler & 0xFF));

    return out;
}

} // namespace

bool write_png_rgba8(const char* path, int width, int height, const unsigned char* rgba_data) {
    if (!path || width <= 0 || height <= 0 || !rgba_data) {
        return false;
    }

    std::FILE* f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }

    const unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    std::fwrite(signature, 1, 8, f);

    std::vector<unsigned char> ihdr(13, 0);
    ihdr[0] = static_cast<unsigned char>((width >> 24) & 0xFF);
    ihdr[1] = static_cast<unsigned char>((width >> 16) & 0xFF);
    ihdr[2] = static_cast<unsigned char>((width >> 8) & 0xFF);
    ihdr[3] = static_cast<unsigned char>(width & 0xFF);
    ihdr[4] = static_cast<unsigned char>((height >> 24) & 0xFF);
    ihdr[5] = static_cast<unsigned char>((height >> 16) & 0xFF);
    ihdr[6] = static_cast<unsigned char>((height >> 8) & 0xFF);
    ihdr[7] = static_cast<unsigned char>(height & 0xFF);
    ihdr[8] = 8; // bit depth
    ihdr[9] = 6; // RGBA
    ihdr[10] = 0; // compression
    ihdr[11] = 0; // filter
    ihdr[12] = 0; // interlace
    if (!write_chunk(f, "IHDR", ihdr)) {
        std::fclose(f);
        return false;
    }

    std::vector<unsigned char> raw;
    raw.reserve(static_cast<std::size_t>(height) * (1u + 4u * static_cast<std::size_t>(width)));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0); // filter type 0
        const unsigned char* row = rgba_data + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4u;
        raw.insert(raw.end(), row, row + static_cast<std::size_t>(width) * 4u);
    }

    const std::vector<unsigned char> idat = make_zlib_stored_stream(raw);
    if (!write_chunk(f, "IDAT", idat)) {
        std::fclose(f);
        return false;
    }

    if (!write_chunk(f, "IEND", {})) {
        std::fclose(f);
        return false;
    }

    const bool ok = std::fclose(f) == 0;
    return ok;
}

} // namespace pt2d
