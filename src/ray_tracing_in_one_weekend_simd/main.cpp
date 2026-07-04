#include <cstdio>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "camera.h"
#include "image.h"
#include "scene.h"

using namespace pg;

int main(void) {
    const int cam_scale_k = 1;
    const int cam_scale = cam_scale_k * 60;
	const int cam_width = 16 * cam_scale;
	const int cam_height = 9 * cam_scale;
	const int cam_samples = 8 * 100;
    Vec3f cam_org(0.0f, 0.0f, -1.0f);
    Vec3f cam_lookat( 0.0f, 0.0f, 0.0f );
	float cam_fov = 90.0f;
	Camera camera(cam_width, cam_height, cam_org, cam_lookat, cam_fov);
    if (cam_samples % 8 > 0) {
		std::cerr << "cam_samples must be a multiple of 8 for SIMD packet processing." << std::endl << "cam_samples = " << cam_samples << std::endl;
		return 1;
    }

    Scene world;
    world.add_sphere(Vec3f(0.0f, 0.0f, -1.0f), 0.5f);
    world.add_sphere(Vec3f(0.0f, -100.5f, -1.0f), 100.0f);

    Image image( cam_width,cam_height );

    for (int y = 0; y < cam_height; ++y) {
        for (int x = 0; x < cam_width; ++x) {
            Color8 accumulated_color;
            for (int s = 0; s < cam_samples/8; ++s) {
                RngPacket8 rng = RngPacket8::seeded(x, y);
                const Ray8 rays = camera.get_ray_packet(x, y, rng);
                const Color8 sample_color = ray_color_packet(rays, world);
                accumulated_color += rays.direction;
            }
			const Vec3f result_color = accumulated_color.mean() / static_cast<float>(cam_samples / 8);
            image.store_packet(x, y, result_color);
        }
    }

    const char* output_path = "ray_tracing_in_one_weekend_simd.png";
    if (!image.write_png(output_path)) {
        std::printf("Failed to write %s\n", output_path);
        return 1;
    }

    std::printf("Wrote %s\n", output_path);
    return 0;
}
