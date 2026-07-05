#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include "math.h"
#include "random.h"
#include "random_sampling.h"

namespace pg {

class Camera {
public:
    Camera(
        int image_width,
        int image_height,
        const Vec3f& lookfrom,
        const Vec3f& lookat,
        const Vec3f& vup,
        float vertical_fov_degrees,
        float focus_distance,
        float defocus_angle_degrees)
        : m_center(lookfrom) {
        const float safe_focus_distance = std::max(focus_distance, 0.001f);
        const float theta = vertical_fov_degrees
            * std::numbers::pi_v<float> / 180.0f;
        const float viewport_height =
            2.0f * std::tan(theta * 0.5f) * safe_focus_distance;
        const float viewport_width = viewport_height
            * static_cast<float>(image_width)
            / static_cast<float>(image_height);

        const Vec3f w = (lookfrom - lookat).normalized();
        const Vec3f u = vup.cross(w).normalized();
        const Vec3f v = w.cross(u);

        const Vec3f viewport_u = u * viewport_width;
        const Vec3f viewport_v = -v * viewport_height;

        m_pixel_delta_u = viewport_u / static_cast<float>(image_width);
        m_pixel_delta_v = viewport_v / static_cast<float>(image_height);

        const Vec3f viewport_upper_left =
            m_center
            - w * safe_focus_distance
            - viewport_u * 0.5f
            - viewport_v * 0.5f;

        m_pixel00_location = viewport_upper_left
            + (m_pixel_delta_u + m_pixel_delta_v) * 0.5f;

        const float defocus_radius = safe_focus_distance
            * std::tan(
                defocus_angle_degrees
                * std::numbers::pi_v<float> / 360.0f
            );
        m_defocus_disk_u = u * defocus_radius;
        m_defocus_disk_v = v * defocus_radius;
        m_defocus_enabled = defocus_angle_degrees > 0.0f;
    }

    Ray8 get_ray_packet(
        int pixel_x,
        int pixel_y,
        RngPacket8& rng) const {
        const Float8 offset_x = rng.next_float01() - Float8(0.5f);
        const Float8 offset_y = rng.next_float01() - Float8(0.5f);

        const Vec3f8 pixel_center =
            splat(m_pixel00_location)
            + splat(m_pixel_delta_u) * Float8(pixel_x)
            + splat(m_pixel_delta_v) * Float8(pixel_y);

        const Vec3f8 pixel_sample =
            pixel_center
            + splat(m_pixel_delta_u) * offset_x
            + splat(m_pixel_delta_v) * offset_y;

        Vec3f8 ray_origin = splat(m_center);
        if (m_defocus_enabled) {
            // The camera integration is complete; the sampling routine remains
            // SIMD_EXERCISE(3).
            const Vec3f8 disk_sample = random_in_unit_disk_packet(rng);
            ray_origin = ray_origin
                + splat(m_defocus_disk_u) * disk_sample.x
                + splat(m_defocus_disk_v) * disk_sample.y;
        }

        return Ray8{
            ray_origin,
            pixel_sample - ray_origin
        };
    }

private:
    Vec3f m_center{};
    Vec3f m_pixel00_location{};
    Vec3f m_pixel_delta_u{};
    Vec3f m_pixel_delta_v{};
    Vec3f m_defocus_disk_u{};
    Vec3f m_defocus_disk_v{};
    bool m_defocus_enabled = false;
};

} // namespace pg
