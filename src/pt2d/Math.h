#pragma once

#include <algorithm>
#include <cmath>

namespace pt2d {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1.0e-4f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 make_vec2(float x, float y) { return {x, y}; }
inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator-(Vec2 a) { return {-a.x, -a.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline Vec2 operator*(float s, Vec2 a) { return a * s; }
inline Vec2 operator/(Vec2 a, float s) { return {a.x / s, a.y / s}; }
inline Vec2& operator+=(Vec2& a, Vec2 b) { a = a + b; return a; }

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float length_squared(Vec2 a) { return dot(a, a); }
inline float length(Vec2 a) { return std::sqrt(length_squared(a)); }
inline Vec2 normalize(Vec2 a) {
    const float len = length(a);
    if (len <= 0.0f) return {1.0f, 0.0f};
    return a / len;
}
inline Vec2 perpendicular(Vec2 a) { return {-a.y, a.x}; }
inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return a * (1.0f - t) + b * t; }
inline float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
inline Vec2 face_forward(Vec2 n, Vec2 reference) {
    return dot(n, reference) >= 0.0f ? n : -n;
}

struct Ray2 {
    Vec2 origin;
    Vec2 dir;
};

} // namespace pt2d
