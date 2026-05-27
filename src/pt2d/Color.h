#pragma once

#include <algorithm>
#include <cmath>

namespace pt2d {

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

inline Color make_color(float v) { return {v, v, v}; }
inline Color make_color(float r, float g, float b) { return {r, g, b}; }
inline Color operator+(Color a, Color b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
inline Color operator-(Color a, Color b) { return {a.r - b.r, a.g - b.g, a.b - b.b}; }
inline Color operator*(Color a, Color b) { return {a.r * b.r, a.g * b.g, a.b * b.b}; }
inline Color operator*(Color a, float s) { return {a.r * s, a.g * s, a.b * s}; }
inline Color operator*(float s, Color a) { return a * s; }
inline Color operator/(Color a, float s) { return {a.r / s, a.g / s, a.b / s}; }
inline Color& operator+=(Color& a, Color b) { a = a + b; return a; }
inline Color& operator*=(Color& a, Color b) { a = a * b; return a; }
inline Color& operator*=(Color& a, float s) { a = a * s; return a; }

inline bool is_black(Color c) {
    return c.r <= 0.0f && c.g <= 0.0f && c.b <= 0.0f;
}

inline Color clamp(Color c, float lo = 0.0f, float hi = 1.0f) {
    return {
        std::max(lo, std::min(hi, c.r)),
        std::max(lo, std::min(hi, c.g)),
        std::max(lo, std::min(hi, c.b)),
    };
}

inline Color tonemap(Color c) {
    return {c.r / (1.0f + c.r), c.g / (1.0f + c.g), c.b / (1.0f + c.b)};
}

inline unsigned char to_srgb8(float linear) {
    linear = std::max(0.0f, std::min(1.0f, linear));
    return static_cast<unsigned char>(std::pow(linear, 1.0f / 2.2f) * 255.0f + 0.5f);
}

} // namespace pt2d
