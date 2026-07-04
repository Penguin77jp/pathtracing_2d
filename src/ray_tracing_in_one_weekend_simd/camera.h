#pragma once

#include "math.h"
#include "random.h"
#include <numbers>


namespace pg {

class Camera {
public:
    Camera(const int image_width, const int image_height,
		const Vec3f origin, const Vec3f lookat, const int long_side_fov)
    {
		m_origin = origin;
		m_dir_z = -(lookat - origin).normalized();
		m_dir_x = Vec3f(0.0f, 1.0f, 0.0f).cross(m_dir_z).normalized();
		m_dir_y = m_dir_z.cross(m_dir_x).normalized();
		m_image_width = image_width;
		m_image_height = image_height;
		m_aspect_ratio = static_cast<float>(image_height) / static_cast<float>(image_width);
		m_screen_long_side_length = 2.0f * std::tan(long_side_fov * std::numbers::pi_v<float> / 360.0f);
    }

    Ray8 get_ray_packet(
        const int i_u,
        const int i_v,
        RngPacket8& io_rng) const
    {
        // ピクセル内のランダムな位置をサンプリングする
        const Float8 u = Float8(i_u) + io_rng.next_float01();

        const Float8 v = Float8(i_v) + io_rng.next_float01();

        // 長辺方向を基準にした、投影面上の1ピクセルの大きさ
        const float long_side_pixel_count = std::max(m_image_width, m_image_height);

        const Float8 pixel_size = Float8(m_screen_long_side_length / long_side_pixel_count);

        // 画像中心を原点とする投影面座標
        //
        // screen_x:
        //   左が負、右が正
        //
        // screen_y:
        //   画像配列では上から下へvが増えるため符号を反転し、
        //   上を正、下を負にする
        const Float8 screen_x =
            (u - Float8(m_image_width * 0.5f)) * pixel_size;

        const Float8 screen_y =
            (Float8(m_image_height * 0.5f) - v) * pixel_size;

        const Vec3f8 ray_origin(
            Float8(m_origin.x),
            Float8(m_origin.y),
            Float8(m_origin.z)
        );

        const Vec3f8 camera_right(
            Float8(m_dir_x.x),
            Float8(m_dir_x.y),
            Float8(m_dir_x.z)
        );

        const Vec3f8 camera_up(
            Float8(m_dir_y.x),
            Float8(m_dir_y.y),
            Float8(m_dir_y.z)
        );

        // m_dir_zはカメラ後方を向いているため、前方は-m_dir_z
        const Vec3f8 camera_forward(
            Float8(-m_dir_z.x),
            Float8(-m_dir_z.y),
            Float8(-m_dir_z.z)
        );

        // 距離1の投影面上の点へ向かうベクトル
        const Vec3f8 ray_direction =
            (
                camera_forward +
                camera_right * screen_x +
                camera_up * screen_y
                ).normalized();

        return Ray8{
            ray_origin,
            ray_direction
        };
    }

private:
	Vec3f m_origin;
	Vec3f m_dir_x;
	Vec3f m_dir_y;
	Vec3f m_dir_z;
	float m_image_width;
	float m_image_height;
	float m_aspect_ratio;
	float m_screen_long_side_length;
};

}
