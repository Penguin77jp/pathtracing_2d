#pragma once

#include <cmath>
#include <cstdint>
#include <random>

#include "render_config.h"
#include "scene.h"

namespace pg {

inline Scene make_material_test_scene() {
    Scene world;

    world.add_sphere(
        Vec3f(0.0f, -100.5f, -1.0f),
        100.0f,
        Material::lambertian(Vec3f(0.8f, 0.8f, 0.0f))
    );
    world.add_sphere(
        Vec3f(0.0f, 0.0f, -1.2f),
        0.5f,
        Material::lambertian(Vec3f(0.1f, 0.2f, 0.5f))
    );
    world.add_sphere(
        Vec3f(-1.0f, 0.0f, -1.0f),
        0.5f,
        Material::dielectric(1.5f)
    );
    world.add_sphere(
        Vec3f(-1.0f, 0.0f, -1.0f),
        0.4f,
        Material::dielectric(1.0f / 1.5f)
    );
    world.add_sphere(
        Vec3f(1.0f, 0.0f, -1.0f),
        0.5f,
        Material::metal(Vec3f(0.8f, 0.6f, 0.2f), 0.0f)
    );

    return world;
}

inline Scene make_final_book_scene(std::uint32_t seed = 0x5eed1234u) {
    Scene world;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    const auto random_float = [&]() {
        return unit(rng);
    };

    const auto random_range = [&](float min_value, float max_value) {
        return min_value + (max_value - min_value) * random_float();
    };

    const auto random_color = [&]() {
        return Vec3f(random_float(), random_float(), random_float());
    };

    world.add_sphere(
        Vec3f(0.0f, -1000.0f, 0.0f),
        1000.0f,
        Material::lambertian(Vec3f(0.5f, 0.5f, 0.5f))
    );

    for (int a = -11; a < 11; ++a) {
        for (int b = -11; b < 11; ++b) {
            const float choose_material = random_float();
            const Vec3f center(
                static_cast<float>(a) + 0.9f * random_float(),
                0.2f,
                static_cast<float>(b) + 0.9f * random_float()
            );

            if ((center - Vec3f(4.0f, 0.2f, 0.0f)).length_squared() <= 0.81f) {
                continue;
            }

            if (choose_material < 0.8f) {
                const Vec3f albedo = random_color() * random_color();
                world.add_sphere(
                    center,
                    0.2f,
                    Material::lambertian(albedo)
                );
            } else if (choose_material < 0.95f) {
                const Vec3f albedo(
                    random_range(0.5f, 1.0f),
                    random_range(0.5f, 1.0f),
                    random_range(0.5f, 1.0f)
                );
                const float fuzz = random_range(0.0f, 0.5f);
                world.add_sphere(
                    center,
                    0.2f,
                    Material::metal(albedo, fuzz)
                );
            } else {
                world.add_sphere(
                    center,
                    0.2f,
                    Material::dielectric(1.5f)
                );
            }
        }
    }

    world.add_sphere(
        Vec3f(0.0f, 1.0f, 0.0f),
        1.0f,
        Material::dielectric(1.5f)
    );
    world.add_sphere(
        Vec3f(-4.0f, 1.0f, 0.0f),
        1.0f,
        Material::lambertian(Vec3f(0.4f, 0.2f, 0.1f))
    );
    world.add_sphere(
        Vec3f(4.0f, 1.0f, 0.0f),
        1.0f,
        Material::metal(Vec3f(0.7f, 0.6f, 0.5f), 0.0f)
    );

    return world;
}

inline Scene make_scene(ScenePreset preset) {
    if (preset == ScenePreset::FinalBookScene) {
        return make_final_book_scene();
    }
    return make_material_test_scene();
}

} // namespace pg
