#pragma once

#include <immintrin.h>
#include <cmath>
#include <concepts>

namespace pg {

class Float8
{
public:
	Float8() = default;
	__m256 values = _mm256_setzero_ps();
	explicit Float8(__m256 v) : values(v) {}
    Float8(const int a) : values(_mm256_set1_ps(static_cast<float>(a))) {}
    Float8(const double a) : values(_mm256_set1_ps(static_cast<float>(a))) {}
    Float8(const float a) : values(_mm256_set1_ps(a)) {}

    Float8 operator+(const Float8& other) const {
        return Float8{ _mm256_add_ps(values, other.values) };
	}
    Float8& operator+=(const Float8& other) {
        values = _mm256_add_ps(values, other.values);
        return *this;
	}
    Float8 operator-(const Float8& other) const {
        return Float8{ _mm256_sub_ps(values, other.values) };
	}
    Float8 operator-() const {
        return Float8{ _mm256_sub_ps(_mm256_setzero_ps(), values) };
    }
    Float8 operator*(const Float8& other) const {
        return Float8{ _mm256_mul_ps(values, other.values) };
    }
    Float8 operator/(const Float8& other) const {
        return Float8{ _mm256_div_ps(values, other.values) };
    }
    static Float8 FusedMultiplyAdd(const Float8& a, const Float8& b, const Float8& c) {
		// a * b + c
        return Float8{ _mm256_fmadd_ps(a.values, b.values, c.values) };
	}
    static Float8 Sqrt(const Float8& a) {
        return Float8{ _mm256_sqrt_ps(a.values) };
	}
    static float Mean(const Float8& a) {

		__m128 low = _mm256_castps256_ps128(a.values); // = [a0, a1, a2, a3]
		__m128 high = _mm256_extractf128_ps(a.values, 1); // = [a4, a5, a6, a7]

		__m128 sum = _mm_add_ps(low, high); // [a0 + a4, a1 + a5, a2 + a6, a3 + a7]

		sum = _mm_hadd_ps(sum, sum); // = [a0 + a4 + a1 + a5,   a2 + a6 + a3 + a7, ?, ?]
		sum = _mm_hadd_ps(sum, sum); // = [a0 + a4 + a1 + a5 + a2 + a6 + a3 + a7, ?, ?, ?]

        return _mm_cvtss_f32(sum) / 8.0f;
	}
};

class Bool8
{
public:
    // 8 lanes of 32-bit masks:
    // false = 0x00000000, true = 0xffffffff
	__m256 values;
    static Bool8 Constant(bool value)
    {
        const __m256i bits = _mm256_set1_epi32(value ? -1 : 0);
        return Bool8{_mm256_castsi256_ps(bits)};
    }
};

template <class T>
class Vec3 {
public:
	Vec3() = default;

    Vec3(const T& in_x, const T& in_y, const T& in_z) 
		: x(in_x), y(in_y), z(in_z){}

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
    Vec3 operator/(const T& scalar) const {
        return Vec3(x / scalar, y / scalar, z / scalar);
	}
    T dot(const Vec3& other) const {
        // Without FMA, the expression
        //
        //     x * other.x + y * other.y + z * other.z
        //
        // is roughly expanded into the following AVX operations:
        //
        // __m256 x_mul  = _mm256_mul_ps(x.values, other.x.values);
        // __m256 y_mul  = _mm256_mul_ps(y.values, other.y.values);
        // __m256 z_mul  = _mm256_mul_ps(z.values, other.z.values);
        // __m256 xy_sum = _mm256_add_ps(x_mul, y_mul);
        // __m256 result = _mm256_add_ps(xy_sum, z_mul);
        //
        // This requires five vector arithmetic instructions:
        // three multiplications and two additions.
        const T zz = z * other.z;
		const T yz = fma(y, other.y, zz);
		const T xyz = fma(x, other.x, yz);
        // It can reduce to 3 operations
		// __m256 zz_mul = _mm256_mul_ps(z, other.z);
		// __m256 yz_fma = _mm256_fmadd_ps(y, other.y, zz_mul); y * other.y + zz_mul
		// __m256 xyz_fma = _mm256_fmadd_ps(x, other.x, yz_fma); x * other.x + yz_fma
        // 
		return xyz;
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
        const T length = sqrt(length_squared());
		return *this / length;
    }
    void normalize() {
		const T len = sqrt(length_squared());
        x = x / len;
        y = y / len;
		z = z / len;
    }
    Vec3<float> mean() const noexcept
		requires std::same_as<T, Float8>
    {
		return Vec3<float>(Float8::Mean(x), Float8::Mean(y), Float8::Mean(z));
    }

private:
    static T fma(const T& a, const T& b, const T& c) noexcept
    {
        if constexpr (std::is_same_v<T, Float8>) {
            return Float8::FusedMultiplyAdd(a, b, c);
        } else {
			return std::fma(a, b, c);
		}
	}
    static T sqrt(const T& a) noexcept
    {
        if constexpr (std::is_same_v<T, Float8>) {
            return Float8::Sqrt(a);
        }
        else {
            return std::sqrt(a);
        }
    }

public:
    T x{};
    T y{};
    T z{};
};


using Vec3f8 = Vec3<Float8>;
using Vec3f = Vec3<float>;
using Color8 = Vec3<Float8>;

template<class T>
struct Ray {
    Vec3<T> origin;
    Vec3<T> direction;
};
using Ray8 = Ray<Float8>;

template <class T, class Mask>
struct HitRecord {
    Vec3<T> p{};
    Vec3<T> normal{};
    T t{};
    Mask front_face = bool_constant(false);
    Mask hit = bool_constant(false);
private:
    static Mask bool_constant(const bool value) noexcept
    {
        if constexpr (std::is_same_v<Mask, Bool8>) {
            return Bool8::Constant(value);
        } else {
            return value;
		}
    }
};
using HitRecord8 = HitRecord<Float8, Bool8>;

}
