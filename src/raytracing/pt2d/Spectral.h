#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "pt2d/Color.h"
#include "pt2d/Sampler.h"

namespace pt2d {

struct XYZ {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline XYZ operator+(XYZ a, XYZ b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline XYZ operator*(XYZ a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline XYZ operator/(XYZ a, float s) { return {a.x / s, a.y / s, a.z / s}; }
inline XYZ& operator+=(XYZ& a, XYZ b) { a = a + b; return a; }

struct SpectralWavelengthSample {
    float wavelength_nm = 550.0f;
    float pdf = 1.0f / 400.0f;
    XYZ cmf;
};

constexpr float kVisibleWavelengthMinNm = 380.0f;
constexpr float kVisibleWavelengthMaxNm = 780.0f;
constexpr float kVisibleWavelengthRangeNm = kVisibleWavelengthMaxNm - kVisibleWavelengthMinNm;
constexpr float kCmfIntegralY = 106.91973464f;

inline float asymmetric_gaussian(float x, float center, float sigma_left, float sigma_right) {
    const float sigma = x < center ? sigma_left : sigma_right;
    const float t = (x - center) / sigma;
    return std::exp(-0.5f * t * t);
}

// Wyman/Sloan/Shirley style analytic approximation of the CIE 1931 color matching functions.
// This is the same compact approximation used in the referenced Qiita article.
inline XYZ color_matching_xyz(float wavelength_nm) {
    const float l = wavelength_nm;
    XYZ xyz;
    xyz.x = 1.056f * asymmetric_gaussian(l, 599.8f, 37.9f, 31.0f)
        + 0.362f * asymmetric_gaussian(l, 442.0f, 16.0f, 26.7f)
        - 0.065f * asymmetric_gaussian(l, 501.1f, 20.4f, 26.2f);
    xyz.y = 0.821f * asymmetric_gaussian(l, 568.8f, 46.9f, 40.5f)
        + 0.286f * asymmetric_gaussian(l, 530.9f, 16.3f, 31.1f);
    xyz.z = 1.217f * asymmetric_gaussian(l, 437.0f, 11.8f, 36.0f)
        + 0.681f * asymmetric_gaussian(l, 459.0f, 26.0f, 13.8f);
    xyz.x = std::max(0.0f, xyz.x);
    xyz.y = std::max(0.0f, xyz.y);
    xyz.z = std::max(0.0f, xyz.z);
    return xyz;
}

inline XYZ linear_srgb_to_xyz(Color rgb) {
    return {
        0.4124564f * rgb.r + 0.3575761f * rgb.g + 0.1804375f * rgb.b,
        0.2126729f * rgb.r + 0.7151522f * rgb.g + 0.0721750f * rgb.b,
        0.0193339f * rgb.r + 0.1191920f * rgb.g + 0.9503041f * rgb.b,
    };
}

inline Color xyz_to_linear_srgb(XYZ xyz) {
    return {
         3.2404542f * xyz.x - 1.5371385f * xyz.y - 0.4985314f * xyz.z,
        -0.9692660f * xyz.x + 1.8760108f * xyz.y + 0.0415560f * xyz.z,
         0.0556434f * xyz.x - 0.2040259f * xyz.y + 1.0572252f * xyz.z,
    };
}

inline float luminance_srgb(Color rgb) {
    return 0.2126729f * rgb.r + 0.7151522f * rgb.g + 0.0721750f * rgb.b;
}

// Projects a linear-sRGB color onto the CMF basis. The coefficients below are
// integral_y * inverse(integral cmf_i(lambda) * cmf_j(lambda) d lambda), computed
// for color_matching_xyz() on [380,780] nm. It is not a full spectral upsampling
// method, but it makes a single RGB value round-trip through XYZ as closely as
// possible with this compact basis.
inline float spectral_value_from_linear_srgb(Color rgb, float wavelength_nm) {
    if (is_black(rgb)) {
        return 0.0f;
    }

    const XYZ target = linear_srgb_to_xyz(rgb);
    const float cx =  3.85370881f * target.x - 2.77433533f * target.y - 0.53845440f * target.z;
    const float cy = -2.77433533f * target.x + 3.38992543f * target.y + 0.30538158f * target.z;
    const float cz = -0.53845440f * target.x + 0.30538158f * target.y + 0.84145295f * target.z;
    const XYZ cmf = color_matching_xyz(wavelength_nm);
    const float v = cx * cmf.x + cy * cmf.y + cz * cmf.z;

    // The least-squares basis can go slightly negative for saturated colors. Negative
    // radiance/reflectance is unstable in a path tracer, so keep the spectral value non-negative.
    return std::max(0.0f, v);
}

class XyzImportanceTable {
public:
    static constexpr int kBinCount = 512;

    XyzImportanceTable() {
        const float width = kVisibleWavelengthRangeNm / static_cast<float>(kBinCount);
        float integral = 0.0f;
        for (int i = 0; i < kBinCount; ++i) {
            const float lambda = kVisibleWavelengthMinNm + (static_cast<float>(i) + 0.5f) * width;
            const XYZ cmf = color_matching_xyz(lambda);
            weights_[i] = std::max(0.0f, cmf.x + cmf.y + cmf.z);
            integral += weights_[i] * width;
        }
        integral_ = std::max(1.0e-8f, integral);

        float accum = 0.0f;
        for (int i = 0; i < kBinCount; ++i) {
            accum += weights_[i] * width / integral_;
            cdf_[i] = accum;
        }
        cdf_[kBinCount - 1] = 1.0f;
    }

    SpectralWavelengthSample sample(Sampler& sampler) const {
        const float u = sampler.next1D();
        const auto it = std::lower_bound(cdf_.begin(), cdf_.end(), u);
        int bin = static_cast<int>(it - cdf_.begin());
        bin = std::clamp(bin, 0, kBinCount - 1);

        const float width = kVisibleWavelengthRangeNm / static_cast<float>(kBinCount);
        const float lambda = kVisibleWavelengthMinNm + (static_cast<float>(bin) + sampler.next1D()) * width;
        const XYZ cmf = color_matching_xyz(lambda);
        const float pdf = std::max(1.0e-8f, weights_[bin] / integral_);
        return {lambda, pdf, cmf};
    }

private:
    std::array<float, kBinCount> weights_{};
    std::array<float, kBinCount> cdf_{};
    float integral_ = 1.0f;
};

inline SpectralWavelengthSample sample_wavelength(Sampler& sampler, bool xyz_importance) {
    if (!xyz_importance) {
        const float lambda = kVisibleWavelengthMinNm + sampler.next1D() * kVisibleWavelengthRangeNm;
        return {lambda, 1.0f / kVisibleWavelengthRangeNm, color_matching_xyz(lambda)};
    }

    static const XyzImportanceTable table;
    return table.sample(sampler);
}

inline XYZ wavelength_contribution_to_xyz(float spectral_radiance, const SpectralWavelengthSample& wl) {
    const float inv_pdf_y = 1.0f / std::max(1.0e-8f, wl.pdf * kCmfIntegralY);
    return {
        spectral_radiance * wl.cmf.x * inv_pdf_y,
        spectral_radiance * wl.cmf.y * inv_pdf_y,
        spectral_radiance * wl.cmf.z * inv_pdf_y,
    };
}

} // namespace pt2d
