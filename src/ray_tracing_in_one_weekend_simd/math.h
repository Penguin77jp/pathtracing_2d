#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <immintrin.h>
#include <type_traits>

namespace pg {

class Bool8 {
public:
    // 8 lanes of 32-bit masks:
    // false = 0x00000000, true = 0xffffffff
    __m256 values = _mm256_setzero_ps();

    static Bool8 constant(bool value) {
        const __m256i bits = _mm256_set1_epi32(value ? -1 : 0);
        return Bool8{_mm256_castsi256_ps(bits)};
    }

    Bool8 operator&(const Bool8& other) const {
        return Bool8{_mm256_and_ps(values, other.values)};
    }

    Bool8 operator|(const Bool8& other) const {
        return Bool8{_mm256_or_ps(values, other.values)};
    }

    Bool8 operator^(const Bool8& other) const {
        return Bool8{_mm256_xor_ps(values, other.values)};
    }

    Bool8 operator~() const {
        const __m256 all_bits = _mm256_castsi256_ps(_mm256_set1_epi32(-1));
        return Bool8{_mm256_xor_ps(values, all_bits)};
    }

    // SIMD_EXERCISE(1): Implement with _mm256_movemask_ps.
    [[nodiscard]] bool any() const noexcept {
        return false;
    }

    // SIMD_EXERCISE(1): Implement without testing lanes one by one.
    [[nodiscard]] bool none() const noexcept {
        return false;
    }
};

class Float8 {
public:
    Float8() = default;
    explicit Float8(__m256 value) : values(value) {}
    Float8(int value) : values(_mm256_set1_ps(static_cast<float>(value))) {}
    Float8(double value) : values(_mm256_set1_ps(static_cast<float>(value))) {}
    Float8(float value) : values(_mm256_set1_ps(value)) {}

    __m256 values = _mm256_setzero_ps();

    Float8 operator+(const Float8& other) const {
        return Float8{_mm256_add_ps(values, other.values)};
    }

    Float8& operator+=(const Float8& other) {
        values = _mm256_add_ps(values, other.values);
        return *this;
    }

    Float8 operator-(const Float8& other) const {
        return Float8{_mm256_sub_ps(values, other.values)};
    }

    Float8& operator-=(const Float8& other) {
        values = _mm256_sub_ps(values, other.values);
        return *this;
    }

    Float8 operator-() const {
        return Float8{_mm256_sub_ps(_mm256_setzero_ps(), values)};
    }

    Float8 operator*(const Float8& other) const {
        return Float8{_mm256_mul_ps(values, other.values)};
    }

    Float8& operator*=(const Float8& other) {
        values = _mm256_mul_ps(values, other.values);
        return *this;
    }

    Float8 operator/(const Float8& other) const {
        return Float8{_mm256_div_ps(values, other.values)};
    }

    Bool8 operator<(const Float8& other) const {
        return Bool8{_mm256_cmp_ps(values, other.values, _CMP_LT_OQ)};
    }

    Bool8 operator<=(const Float8& other) const {
        return Bool8{_mm256_cmp_ps(values, other.values, _CMP_LE_OQ)};
    }

    Bool8 operator>(const Float8& other) const {
        return Bool8{_mm256_cmp_ps(values, other.values, _CMP_GT_OQ)};
    }

    Bool8 operator>=(const Float8& other) const {
        return Bool8{_mm256_cmp_ps(values, other.values, _CMP_GE_OQ)};
    }

    Bool8 operator==(const Float8& other) const {
        return Bool8{_mm256_cmp_ps(values, other.values, _CMP_EQ_OQ)};
    }

    Bool8 operator!=(const Float8& other) const {
        return Bool8{_mm256_cmp_ps(values, other.values, _CMP_NEQ_OQ)};
    }

    static Float8 min(const Float8& a, const Float8& b) {
        return Float8{_mm256_min_ps(a.values, b.values)};
    }

    static Float8 max(const Float8& a, const Float8& b) {
        return Float8{_mm256_max_ps(a.values, b.values)};
    }

    static Float8 abs(const Float8& value) {
        const __m256 sign_bit = _mm256_set1_ps(-0.0f);
        return Float8{_mm256_andnot_ps(sign_bit, value.values)};
    }

    static Float8 clamp(const Float8& value, const Float8& low, const Float8& high) {
        return min(max(value, low), high);
    }

    static Float8 fused_multiply_add(
        const Float8& a,
        const Float8& b,
        const Float8& c) {
        return Float8{_mm256_fmadd_ps(a.values, b.values, c.values)};
    }

    static Float8 sqrt(const Float8& value) {
        return Float8{_mm256_sqrt_ps(value.values)};
    }

    static float mean(const Float8& value) {
        __m128 low = _mm256_castps256_ps128(value.values);
        __m128 high = _mm256_extractf128_ps(value.values, 1);
        __m128 sum = _mm_add_ps(low, high);
        sum = _mm_hadd_ps(sum, sum);
        sum = _mm_hadd_ps(sum, sum);
        return _mm_cvtss_f32(sum) / 8.0f;
    }

    friend Float8 select(
        const Bool8& mask,
        const Float8& if_true,
        const Float8& if_false) {
        return Float8{
            _mm256_blendv_ps(if_false.values, if_true.values, mask.values)
        };
    }
};

template <class T>
class Vec3 {
public:
    static Vec3<T> SetZero() {
        return Vec3<T>(T(0), T(0), T(0));
	}
    static Vec3<T> SetOne() {
        return Vec3<T>(T(1), T(1), T(1));
    }
    Vec3() = default;

    Vec3(const T& x_value, const T& y_value, const T& z_value)
        : x(x_value), y(y_value), z(z_value) {}

    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 operator-() const {
        return Vec3(-x, -y, -z);
    }

    Vec3 operator*(const T& scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    Vec3 operator*(const Vec3& other) const {
        return Vec3(x * other.x, y * other.y, z * other.z);
    }

    Vec3& operator*=(const Vec3& other) {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        return *this;
    }

    Vec3 operator/(const T& scalar) const {
        return Vec3(x / scalar, y / scalar, z / scalar);
    }

    T dot(const Vec3& other) const {
        const T zz = z * other.z;
        const T yz = fma(y, other.y, zz);
        return fma(x, other.x, yz);
    }

    Vec3 cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    T length_squared() const {
        return dot(*this);
    }

    Vec3 normalized() const {
        return *this / square_root(length_squared());
    }

    void normalize() {
        const T length = square_root(length_squared());
        x = x / length;
        y = y / length;
        z = z / length;
    }

    Vec3<float> mean() const noexcept
        requires std::same_as<T, Float8>
    {
        return Vec3<float>(
            Float8::mean(x),
            Float8::mean(y),
            Float8::mean(z)
        );
    }

    friend Vec3<T> select(
        const Bool8& mask,
        const Vec3<T>& if_true,
        const Vec3<T>& if_false) {
        return Vec3<T>{
            select(mask, if_true.x, if_false.x),
            select(mask, if_true.y, if_false.y),
            select(mask, if_true.z, if_false.z)
        };
    }

    T x{};
    T y{};
    T z{};

private:
    static T fma(const T& a, const T& b, const T& c) noexcept {
        if constexpr (std::is_same_v<T, Float8>) {
            return Float8::fused_multiply_add(a, b, c);
        } else {
            return std::fma(a, b, c);
        }
    }

    static T square_root(const T& value) noexcept {
        if constexpr (std::is_same_v<T, Float8>) {
            return Float8::sqrt(value);
        } else {
            return std::sqrt(value);
        }
    }
};

using Vec3f8 = Vec3<Float8>;
using Vec3f = Vec3<float>;
using Color8 = Vec3<Float8>;

inline Vec3f8 splat(const Vec3f& value) {
    return Vec3f8(
        Float8(value.x),
        Float8(value.y),
        Float8(value.z)
    );
}

inline Vec3f8 reflect(const Vec3f8& direction, const Vec3f8& normal) {
    return direction - normal * (Float8(2.0f) * direction.dot(normal));
}

inline Vec3f8 refract(
    const Vec3f8& unit_direction,
    const Vec3f8& normal,
    const Float8& eta_ratio) {
    const Float8 cos_theta = Float8::min(
        (-unit_direction).dot(normal),
        Float8(1.0f)
    );
    const Vec3f8 perpendicular =
        (unit_direction + normal * cos_theta) * eta_ratio;
    const Float8 parallel_length = Float8::sqrt(
        Float8::abs(Float8(1.0f) - perpendicular.length_squared())
    );
    const Vec3f8 parallel = normal * (-parallel_length);
    return perpendicular + parallel;
}

template <class T>
struct Ray {
    Vec3<T> origin;
    Vec3<T> direction;

    Vec3<T> at(const T& t) const {
        return origin + direction * t;
    }

    friend Ray select(
        const Bool8& mask,
        const Ray& if_true,
        const Ray& if_false) {
        return Ray{
            select(mask, if_true.origin, if_false.origin),
            select(mask, if_true.direction, if_false.direction)
        };
    }
};

using Ray8 = Ray<Float8>;

template <class T, class Mask>
struct HitRecord {
    Vec3<T> p{};
    Vec3<T> normal{};
    T t{};
    Mask front_face = bool_constant(false);
    Mask hit = bool_constant(false);

    friend HitRecord select(
        const Mask& mask,
        const HitRecord& if_true,
        const HitRecord& if_false) {
        return HitRecord{
            select(mask, if_true.p, if_false.p),
            select(mask, if_true.normal, if_false.normal),
            select(mask, if_true.t, if_false.t),
            select_mask(mask, if_true.front_face, if_false.front_face),
            select_mask(mask, if_true.hit, if_false.hit)
        };
    }

private:
    static Mask bool_constant(bool value) noexcept {
        if constexpr (std::is_same_v<Mask, Bool8>) {
            return Bool8::constant(value);
        } else {
            return value;
        }
    }

    static Mask select_mask(
        const Mask& mask,
        const Mask& if_true,
        const Mask& if_false) noexcept {
        if constexpr (std::is_same_v<Mask, Bool8>) {
            return Bool8{
                _mm256_blendv_ps(if_false.values, if_true.values, mask.values)
            };
        } else {
            return mask ? if_true : if_false;
        }
    }
};

using HitRecord8 = HitRecord<Float8, Bool8>;

} // namespace pg
