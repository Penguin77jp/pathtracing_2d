#include <cstdio>
#include <iostream>
#include <string_view>
#include <chrono>
#include <stb_image_write.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "camera.h"
#include "image.h"
#include "path_tracer.h"
#include "render_config.h"
#include "scene_factory.h"

using namespace pg;

namespace {

RenderConfig parse_options(int argc, char** argv) {
    ScenePreset scene = ScenePreset::FinalBookScene;
    RenderMode mode = RenderMode::PathTracerExercise;
    bool quick = false;
    std::string output_path;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--final") {
            scene = ScenePreset::FinalBookScene;
        } else if (arg == "--materials") {
            scene = ScenePreset::MaterialTest;
        } else if (arg == "--path") {
            mode = RenderMode::PathTracerExercise;
        } else if (arg == "--normal") {
            mode = RenderMode::NormalPreview;
        } else if (arg == "--quick") {
            quick = true;
        } else if (arg.starts_with("--output=")) {
            output_path = std::string(arg.substr(9));
        }
    }

    RenderConfig config = scene == ScenePreset::FinalBookScene
        ? final_scene_config()
        : material_test_config();
    config.mode = mode;

    if (!output_path.empty()) {
        config.output_path = output_path;
    }

    if (quick) {
        apply_quick_settings(config);
    }

    if (config.output_path.ends_with(".png") && !Image::png_supported()) {
        std::cerr
            << "stb_image_write.h was not found; using PPM output instead.\n";
        exit(1);
    }

    return config;
}
} // namespace

int main(int argc, char** argv) {
    RenderConfig config = parse_options(argc, argv);
    config.print();

    if (config.samples_per_pixel <= 0
        || config.samples_per_pixel % 8 != 0) {
        std::cerr
            << "samples_per_pixel must be a positive multiple of 8 for "
            << "SIMD packet processing.\n";
        return 1;
    }

    const Camera camera(
        config.image_width,
        config.image_height,
        config.camera.lookfrom,
        config.camera.lookat,
        config.camera.vup,
        config.camera.vertical_fov_degrees,
        config.camera.focus_distance,
        config.camera.defocus_angle_degrees
    );

    const Scene world = make_scene(config.scene);
    Image image(config.image_width, config.image_height);

    std::cout
        << "Rendering " << config.image_width << 'x' << config.image_height
        << ", samples=" << config.samples_per_pixel
        << ", spheres=" << world.sphere_count() << '\n';

    const int packet_count = config.samples_per_pixel / 8;

	const auto _start_time = std::chrono::high_resolution_clock::now();
    for (int y = 0; y < config.image_height; ++y) {
        std::clog
            << "\rScanlines remaining: "
            << (config.image_height - y)
            << ' '
            << std::flush;

#pragma omp parallel for schedule(dynamic)
        for (int x = 0; x < config.image_width; ++x) {
            Color8 accumulated_color;
            RngPacket8 rng = RngPacket8::seeded(x, y);

            for (int packet = 0; packet < packet_count; ++packet) {
                const Ray8 rays = camera.get_ray_packet(x, y, rng);
                const Color8 sample_color =
                    config.mode == RenderMode::PathTracerExercise
                    ? ray_color_path_packet(
                        rays,
                        world,
                        rng,
                        config.max_depth
                    )
                    : ray_color_normal_packet(rays, world);
                accumulated_color += sample_color;
            }

            const Vec3f averaged_color =
                accumulated_color.mean() / static_cast<float>(packet_count);
            image.store_linear_color(x, y, averaged_color);
        }
    }
	const auto _end_time = std::chrono::high_resolution_clock::now();

    std::clog << "\rDone.                      \n";


    if (!image.write(config.output_path)) {
        std::fprintf(
            stderr,
            "Failed to write %s\n",
            config.output_path.c_str()
        );
        return 1;
    }

    std::printf("Wrote %s\n", config.output_path.c_str());
    std::printf(
        "Elapsed time: %.3f seconds\n",
        std::chrono::duration<double>(_end_time - _start_time).count()
	);
    std::printf(
        "Elapsed time: %.3e ms/pixel/sample\n",
		std::chrono::duration<double>(_end_time - _start_time).count() / (config.image_width * config.image_height * config.samples_per_pixel) * 1000.0
    );
    return 0;
}
