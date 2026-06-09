#include "pt2d/Integrator.h"
#include "pt2d/Spectral.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>

namespace pt2d {

BsdfSample sample_diffuse_cosine_2d(Vec2 normal, Sampler& sampler) {
    const float u = sampler.next1D();
    const float theta = std::asin(2.0f * u - 1.0f);

    const Vec2 tangent = perpendicular(normal);
    Vec2 dir = std::cos(theta) * normal + std::sin(theta) * tangent;
    dir = normalize(dir);

    const float cos_theta = std::max(0.0f, dot(normal, dir));
    return {dir, cos_theta * 0.5f};
}

DirectionSample sample_uniform_circle_2d(Sampler& sampler) {
    const float theta = sampler.next1D() * 2.0f * kPi;
    return {{std::cos(theta), std::sin(theta)}, 1.0f / (2.0f * kPi)};
}

namespace {

Vec2 reflect(Vec2 v, Vec2 n) {
    return normalize(v - 2.0f * dot(v, n) * n);
}

bool refract(Vec2 v, Vec2 n, float eta, Vec2& refracted) {
    const float cos_i = std::min(1.0f, std::max(0.0f, dot(-v, n)));
    const float sin2_t = eta * eta * std::max(0.0f, 1.0f - cos_i * cos_i);
    if (sin2_t > 1.0f) {
        return false;
    }
    const float cos_t = std::sqrt(std::max(0.0f, 1.0f - sin2_t));
    refracted = normalize(eta * v + (eta * cos_i - cos_t) * n);
    return true;
}

float schlick_fresnel(float cos_i, float eta_i, float eta_t) {
    float r0 = (eta_i - eta_t) / (eta_i + eta_t);
    r0 = r0 * r0;
    const float m = std::max(0.0f, 1.0f - cos_i);
    return r0 + (1.0f - r0) * m * m * m * m * m;
}


float cauchy_ior_at_wavelength(const Material& material, float wavelength_nm) {
    const float lambda_um = std::max(0.001f, wavelength_nm * 0.001f);
    const float eta = material.cauchy_a + material.cauchy_b / (lambda_um * lambda_um);
    return std::max(1.0f, eta);
}

float albedo_at_wavelength(const Material& material, float wavelength_nm) {
    return spectral_value_from_linear_srgb(material.albedo, wavelength_nm);
}

float emission_at_wavelength(const Material& material, float wavelength_nm) {
    return spectral_value_from_linear_srgb(material.emission, wavelength_nm);
}

Color clamp_non_negative(Color c) {
    return {std::max(0.0f, c.r), std::max(0.0f, c.g), std::max(0.0f, c.b)};
}

float clamp_light_angle(float degrees) {
    return std::max(0.0f, std::min(360.0f, degrees));
}

bool emission_direction_allowed(Vec2 light_normal, Vec2 outgoing_from_light, float angle_deg) {
    const float angle = clamp_light_angle(angle_deg);
    if (angle >= 359.999f) {
        return true;
    }
    if (angle <= 0.0f) {
        return dot(normalize(light_normal), normalize(outgoing_from_light)) >= 0.999999f;
    }
    const float half_angle = 0.5f * angle * kPi / 180.0f;
    const float cutoff = std::cos(half_angle);
    return dot(normalize(light_normal), normalize(outgoing_from_light)) >= cutoff;
}

float light_geometry_cosine(Vec2 light_normal, Vec2 outgoing_from_light, float angle_deg) {
    if (!emission_direction_allowed(light_normal, outgoing_from_light, angle_deg)) {
        return 0.0f;
    }

    const float d = dot(normalize(light_normal), normalize(outgoing_from_light));
    // 0..180 deg behaves like the original one-sided segment light. Above 180 deg,
    // treat the light as progressively two-sided so 360 deg is meaningful.
    return clamp_light_angle(angle_deg) <= 180.0f ? std::max(0.0f, d) : std::abs(d);
}


Vec2 rotate(Vec2 v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

DirectionSample sample_light_emission_direction(Vec2 light_normal, float angle_deg, Sampler& sampler) {
    const float angle = clamp_light_angle(angle_deg);
    if (angle >= 359.999f) {
        return sample_uniform_circle_2d(sampler);
    }

    const float angle_rad = std::max(1.0e-4f, angle * kPi / 180.0f);
    const float half = 0.5f * angle_rad;
    const float offset = (sampler.next1D() * 2.0f - 1.0f) * half;
    return {normalize(rotate(normalize(light_normal), offset)), 1.0f / angle_rad};
}

Color estimate_photon_final_gather(
    const PhotonMap& photon_map,
    const HitInfo& hit,
    const Material& material,
    Vec2 shading_normal,
    const PhotonMappingSettings& settings)
{
    const float radius = std::max(1.0e-4f, settings.gather_radius);
    const float radius2 = radius * radius;
    const float inv_support_length = 1.0f / std::max(1.0e-4f, 2.0f * radius);
    const Color diffuse_brdf = material.albedo * 0.5f;

    Color sum = make_color(0.0f);
    for (const Photon& photon : photon_map.photons) {
        if (settings.caustics_only && !photon.caustic) {
            continue;
        }
        const Vec2 delta = photon.position - hit.position;
        const float d2 = length_squared(delta);
        if (d2 > radius2) {
            continue;
        }

        // A small cone kernel keeps the first implementation stable and easy to debug.
        const float kernel = std::max(0.0f, 1.0f - std::sqrt(d2) / radius);
        const Vec2 wi = normalize(-photon.incoming_dir);
        const float cos_surface = std::max(0.0f, dot(shading_normal, wi));
        if (cos_surface <= 0.0f) {
            continue;
        }
        sum += photon.flux * diffuse_brdf * (cos_surface * kernel * inv_support_length);
    }

    return sum * settings.strength;
}

void trace_one_photon(
    const Scene& scene,
    Ray2 ray,
    Color flux,
    Sampler& sampler,
    const IntegratorSettings& settings,
    PhotonMap& photon_map)
{
    bool touched_specular = false;

    for (int depth = 0; depth < std::max(1, settings.photon_mapping.photon_max_depth); ++depth) {
        const HitInfo hit = scene.intersect(ray);
        if (!hit.hit) {
            return;
        }

        const Material& material = *hit.material;
        if (material.is_light()) {
            return;
        }

        if (material.is_dielectric()) {
            const bool front_face = dot(ray.dir, hit.normal) < 0.0f;
            const Vec2 n = front_face ? hit.normal : -hit.normal;
            const float eta_i = front_face ? 1.0f : material.ior;
            const float eta_t = front_face ? material.ior : 1.0f;
            const float eta = eta_i / eta_t;
            const float cos_i = std::min(1.0f, std::max(0.0f, dot(-ray.dir, n)));

            Vec2 refracted_dir;
            const bool can_refract = refract(ray.dir, n, eta, refracted_dir);
            const float fresnel = can_refract ? schlick_fresnel(cos_i, eta_i, eta_t) : 1.0f;
            const bool choose_reflect = sampler.next1D() < fresnel;
            const Vec2 next_dir = choose_reflect ? reflect(ray.dir, n) : refracted_dir;

            flux *= material.albedo;
            touched_specular = true;
            ray = {hit.position + next_dir * kEpsilon, next_dir};
            continue;
        }

        const bool store = !settings.photon_mapping.caustics_only || touched_specular;
        if (store) {
            Photon photon;
            photon.position = hit.position;
            photon.incoming_dir = ray.dir;
            photon.normal = hit.normal;
            photon.flux = flux;
            photon.caustic = touched_specular;
            photon.bounce_count = depth;
            photon_map.photons.push_back(photon);
        }

        // This first photon mapper is caustic final gathering: store the first diffuse hit
        // reached by the light path and stop there.
        return;
    }
}

Color estimate_direct_nee(
    const Scene& scene,
    const HitInfo& hit,
    const Material& material,
    Vec2 shading_normal,
    const Color& beta,
    Sampler& sampler,
    DebugRecorder* debug,
    int depth)
{
    if (scene.light_segment_ids.empty()) {
        return make_color(0.0f);
    }

    const LightSample light = scene.sample_light(sampler.next1D(), sampler.next1D());
    const Vec2 to_light = light.position - hit.position;
    const float dist = length(to_light);
    if (dist <= kEpsilon || light.pdf_length <= 0.0f) {
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::LightSample;
            event.depth = depth;
            event.hit_point = hit.position;
            event.sampled_point = light.position;
            event.sampled_object_id = light.light_object_id;
            event.beta = beta;
            event.geometric_term = 0.0f;
            event.blocked = true;
            debug->add(event);
        }
        return make_color(0.0f);
    }

    const Vec2 wi = to_light / dist;
    const float cos_surface = std::max(0.0f, dot(shading_normal, wi));
    const float cos_light = light_geometry_cosine(light.normal, -wi, light.emission_angle_deg);

    bool blocked = true;
    float g_nee = 0.0f;
    if (cos_surface > 0.0f && cos_light > 0.0f) {
        const Ray2 shadow_ray{hit.position + shading_normal * kEpsilon, wi};
        blocked = scene.occluded(shadow_ray, dist - kEpsilon);
        if (!blocked) {
            g_nee = (cos_surface * cos_light) / dist;
        }

        if (debug) {
            DebugEvent shadow;
            shadow.type = DebugEventType::ShadowRay;
            shadow.depth = depth;
            shadow.ray_origin = hit.position;
            shadow.ray_end = light.position;
            shadow.ray_dir = wi;
            shadow.distance = dist;
            shadow.blocked = blocked;
            shadow.geometric_term = g_nee;
            shadow.beta = beta;
            debug->add(shadow);
        }
    } else if (debug) {
        DebugEvent shadow;
        shadow.type = DebugEventType::ShadowRay;
        shadow.depth = depth;
        shadow.ray_origin = hit.position;
        shadow.ray_end = light.position;
        shadow.ray_dir = wi;
        shadow.distance = dist;
        shadow.blocked = true;
        shadow.geometric_term = 0.0f;
        shadow.beta = beta;
        debug->add(shadow);
    }

    const Color f = material.albedo * 0.5f;
    const Color local_direct = (g_nee > 0.0f)
        ? (f * light.emission) * (g_nee / light.pdf_length)
        : make_color(0.0f);

    if (debug) {
        DebugEvent event;
        event.type = DebugEventType::LightSample;
        event.depth = depth;
        event.hit_point = hit.position;
        event.normal = shading_normal;
        event.sampled_point = light.position;
        event.sampled_object_id = light.light_object_id;
        event.distance = dist;
        event.pdf = light.pdf_length;
        event.geometric_term = g_nee;
        event.blocked = blocked || g_nee <= 0.0f;
        event.beta = beta;
        event.l_nee = beta * local_direct;
        event.contribution = event.l_nee;
        debug->add(event);
    }

    return local_direct;
}

Color trace_recursive(
    const Scene& scene,
    Ray2 ray,
    Sampler& sampler,
    const IntegratorSettings& settings,
    DebugRecorder* debug,
    int depth,
    Color beta,
    const PhotonMap* photon_map)
{
    if (depth >= std::max(1, settings.max_depth)) {
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::Terminated;
            event.depth = depth;
            event.ray_origin = ray.origin;
            event.ray_dir = ray.dir;
            event.beta = beta;
            debug->add(event);
        }
        return make_color(0.0f);
    }

    const HitInfo hit = scene.intersect(ray);

    if (debug) {
        DebugEvent ray_event;
        ray_event.type = DebugEventType::RaySegment;
        ray_event.depth = depth;
        ray_event.ray_origin = ray.origin;
        ray_event.ray_dir = ray.dir;
        ray_event.ray_end = hit.hit ? hit.position : ray.origin + ray.dir * 12.0f;
        ray_event.beta = beta;
        debug->add(ray_event);
    }

    if (!hit.hit) {
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::Miss;
            event.depth = depth;
            event.ray_origin = ray.origin;
            event.ray_dir = ray.dir;
            event.beta = beta;
            debug->add(event);
        }
        return make_color(0.0f);
    }

    const Material& material = *hit.material;
    const Vec2 shading_normal = face_forward(hit.normal, -ray.dir);

    if (debug) {
        DebugEvent event;
        event.type = DebugEventType::Hit;
        event.depth = depth;
        event.hit_point = hit.position;
        event.normal = shading_normal;
        event.beta = beta;
        event.object_id = hit.object_id;
        debug->add(event);
    }

    if (material.is_light()) {
        const bool count_emission = (settings.kind != IntegratorKind::PathTracingNEE) || (depth == 0);
        const bool direction_allowed = emission_direction_allowed(hit.normal, -ray.dir, material.emission_angle_deg);
        const Color emitted = (count_emission && direction_allowed) ? material.emission : make_color(0.0f);
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::HitLight;
            event.depth = depth;
            event.hit_point = hit.position;
            event.normal = shading_normal;
            event.contribution = beta * emitted;
            event.l_path = beta * emitted;
            event.l_total = beta * emitted;
            event.beta = beta;
            event.object_id = hit.object_id;
            debug->add(event);
        }
        return emitted;
    }

    if (material.is_dielectric()) {
        if (depth + 1 >= std::max(1, settings.max_depth)) {
            return make_color(0.0f);
        }

        const bool front_face = dot(ray.dir, hit.normal) < 0.0f;
        const Vec2 n = front_face ? hit.normal : -hit.normal;
        const float eta_i = front_face ? 1.0f : material.ior;
        const float eta_t = front_face ? material.ior : 1.0f;
        const float eta = eta_i / eta_t;
        const float cos_i = std::min(1.0f, std::max(0.0f, dot(-ray.dir, n)));

        Vec2 refracted_dir;
        const bool can_refract = refract(ray.dir, n, eta, refracted_dir);
        const float fresnel = can_refract ? schlick_fresnel(cos_i, eta_i, eta_t) : 1.0f;
        const bool choose_reflect = sampler.next1D() < fresnel;
        const Vec2 next_dir = choose_reflect ? reflect(ray.dir, n) : refracted_dir;
        const Color next_beta = beta * material.albedo;

        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::BsdfSample;
            event.depth = depth;
            event.hit_point = hit.position;
            event.normal = n;
            event.sampled_dir = next_dir;
            event.pdf = choose_reflect ? fresnel : (1.0f - fresnel);
            event.beta = next_beta;
            event.object_id = hit.object_id;
            debug->add(event);
        }

        const Ray2 next_ray{hit.position + next_dir * kEpsilon, next_dir};
        const Color next_radiance = trace_recursive(scene, next_ray, sampler, settings, debug, depth + 1, next_beta, photon_map);
        const Color local_path = material.albedo * next_radiance;
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::DirectContribution;
            event.depth = depth;
            event.hit_point = hit.position;
            event.normal = n;
            event.beta = beta;
            event.l_nee = make_color(0.0f);
            event.l_path = beta * local_path;
            event.l_total = beta * local_path;
            event.contribution = event.l_total;
            event.object_id = hit.object_id;
            debug->add(event);
        }
        return local_path;
    }

    Color local_nee = make_color(0.0f);
    if (settings.kind == IntegratorKind::PathTracingNEE || settings.kind == IntegratorKind::PhotonMapping) {
        local_nee = estimate_direct_nee(scene, hit, material, shading_normal, beta, sampler, debug, depth);
    }

    Color local_photon = make_color(0.0f);
    if (settings.kind == IntegratorKind::PhotonMapping && photon_map) {
        local_photon = estimate_photon_final_gather(*photon_map, hit, material, shading_normal, settings.photon_mapping);
    }

    Color local_indirect = make_color(0.0f);
    if (depth + 1 < std::max(1, settings.max_depth)) {
        const BsdfSample bsdf = sample_diffuse_cosine_2d(shading_normal, sampler);
        if (bsdf.pdf > 0.0f) {
            const float cos_theta = std::max(0.0f, dot(shading_normal, bsdf.dir));
            const Color f = material.albedo * 0.5f;
            const Color bsdf_weight = f * (cos_theta / bsdf.pdf);
            const Color next_beta = beta * bsdf_weight;

            if (debug) {
                DebugEvent event;
                event.type = DebugEventType::BsdfSample;
                event.depth = depth;
                event.hit_point = hit.position;
                event.normal = shading_normal;
                event.sampled_dir = bsdf.dir;
                event.pdf = bsdf.pdf;
                event.beta = next_beta;
                event.object_id = hit.object_id;
                debug->add(event);
            }

            const Ray2 next_ray{hit.position + shading_normal * kEpsilon, bsdf.dir};
            const Color next_radiance = trace_recursive(scene, next_ray, sampler, settings, debug, depth + 1, next_beta, photon_map);
            local_indirect = bsdf_weight * next_radiance;
        }
    }

    const Color local_total = local_nee + local_photon + local_indirect;

    if (debug) {
        DebugEvent event;
        event.type = DebugEventType::DirectContribution;
        event.depth = depth;
        event.hit_point = hit.position;
        event.normal = shading_normal;
        event.beta = beta;
        event.l_nee = beta * local_nee;
        event.l_path = beta * local_indirect;
        event.l_total = beta * local_total;
        event.contribution = event.l_total;
        event.object_id = hit.object_id;
        debug->add(event);
    }

    return local_total;
}


float trace_recursive_spectral(
    const Scene& scene,
    Ray2 ray,
    Sampler& sampler,
    const IntegratorSettings& settings,
    DebugRecorder* debug,
    int depth,
    float beta,
    float wavelength_nm)
{
    if (depth >= std::max(1, settings.max_depth)) {
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::Terminated;
            event.depth = depth;
            event.ray_origin = ray.origin;
            event.ray_dir = ray.dir;
            event.beta = make_color(beta);
            debug->add(event);
        }
        return 0.0f;
    }

    const HitInfo hit = scene.intersect(ray);

    if (debug) {
        DebugEvent ray_event;
        ray_event.type = DebugEventType::RaySegment;
        ray_event.depth = depth;
        ray_event.ray_origin = ray.origin;
        ray_event.ray_dir = ray.dir;
        ray_event.ray_end = hit.hit ? hit.position : ray.origin + ray.dir * 12.0f;
        ray_event.beta = make_color(beta);
        debug->add(ray_event);
    }

    if (!hit.hit) {
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::Miss;
            event.depth = depth;
            event.ray_origin = ray.origin;
            event.ray_dir = ray.dir;
            event.beta = make_color(beta);
            debug->add(event);
        }
        return 0.0f;
    }

    const Material& material = *hit.material;
    const Vec2 shading_normal = face_forward(hit.normal, -ray.dir);

    if (debug) {
        DebugEvent event;
        event.type = DebugEventType::Hit;
        event.depth = depth;
        event.hit_point = hit.position;
        event.normal = shading_normal;
        event.beta = make_color(beta);
        event.object_id = hit.object_id;
        debug->add(event);
    }

    if (material.is_light()) {
        const bool direction_allowed = emission_direction_allowed(hit.normal, -ray.dir, material.emission_angle_deg);
        const float emitted = direction_allowed ? emission_at_wavelength(material, wavelength_nm) : 0.0f;
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::HitLight;
            event.depth = depth;
            event.hit_point = hit.position;
            event.normal = shading_normal;
            event.contribution = make_color(beta * emitted);
            event.l_path = make_color(beta * emitted);
            event.l_total = make_color(beta * emitted);
            event.beta = make_color(beta);
            event.object_id = hit.object_id;
            debug->add(event);
        }
        return emitted;
    }

    const float material_albedo = albedo_at_wavelength(material, wavelength_nm);

    if (material.is_dielectric()) {
        if (depth + 1 >= std::max(1, settings.max_depth)) {
            return 0.0f;
        }

        const float material_ior = cauchy_ior_at_wavelength(material, wavelength_nm);
        const bool front_face = dot(ray.dir, hit.normal) < 0.0f;
        const Vec2 n = front_face ? hit.normal : -hit.normal;
        const float eta_i = front_face ? 1.0f : material_ior;
        const float eta_t = front_face ? material_ior : 1.0f;
        const float eta = eta_i / eta_t;
        const float cos_i = std::min(1.0f, std::max(0.0f, dot(-ray.dir, n)));

        Vec2 refracted_dir;
        const bool can_refract = refract(ray.dir, n, eta, refracted_dir);
        const float fresnel = can_refract ? schlick_fresnel(cos_i, eta_i, eta_t) : 1.0f;
        const bool choose_reflect = sampler.next1D() < fresnel;
        const Vec2 next_dir = choose_reflect ? reflect(ray.dir, n) : refracted_dir;
        const float next_beta = beta * material_albedo;

        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::BsdfSample;
            event.depth = depth;
            event.hit_point = hit.position;
            event.normal = n;
            event.sampled_dir = next_dir;
            event.pdf = choose_reflect ? fresnel : (1.0f - fresnel);
            event.beta = make_color(next_beta);
            event.object_id = hit.object_id;
            debug->add(event);
        }

        const Ray2 next_ray{hit.position + next_dir * kEpsilon, next_dir};
        const float next_radiance = trace_recursive_spectral(scene, next_ray, sampler, settings, debug, depth + 1, next_beta, wavelength_nm);
        const float local_path = material_albedo * next_radiance;
        if (debug) {
            DebugEvent event;
            event.type = DebugEventType::DirectContribution;
            event.depth = depth;
            event.hit_point = hit.position;
            event.normal = n;
            event.beta = make_color(beta);
            event.l_nee = make_color(0.0f);
            event.l_path = make_color(beta * local_path);
            event.l_total = make_color(beta * local_path);
            event.contribution = event.l_total;
            event.object_id = hit.object_id;
            debug->add(event);
        }
        return local_path;
    }

    float local_indirect = 0.0f;
    if (depth + 1 < std::max(1, settings.max_depth)) {
        const BsdfSample bsdf = sample_diffuse_cosine_2d(shading_normal, sampler);
        if (bsdf.pdf > 0.0f) {
            const float cos_theta = std::max(0.0f, dot(shading_normal, bsdf.dir));
            const float f = material_albedo * 0.5f;
            const float bsdf_weight = f * (cos_theta / bsdf.pdf);
            const float next_beta = beta * bsdf_weight;

            if (debug) {
                DebugEvent event;
                event.type = DebugEventType::BsdfSample;
                event.depth = depth;
                event.hit_point = hit.position;
                event.normal = shading_normal;
                event.sampled_dir = bsdf.dir;
                event.pdf = bsdf.pdf;
                event.beta = make_color(next_beta);
                event.object_id = hit.object_id;
                debug->add(event);
            }

            const Ray2 next_ray{hit.position + shading_normal * kEpsilon, bsdf.dir};
            const float next_radiance = trace_recursive_spectral(scene, next_ray, sampler, settings, debug, depth + 1, next_beta, wavelength_nm);
            local_indirect = bsdf_weight * next_radiance;
        }
    }

    if (debug) {
        DebugEvent event;
        event.type = DebugEventType::DirectContribution;
        event.depth = depth;
        event.hit_point = hit.position;
        event.normal = shading_normal;
        event.beta = make_color(beta);
        event.l_nee = make_color(0.0f);
        event.l_path = make_color(beta * local_indirect);
        event.l_total = make_color(beta * local_indirect);
        event.contribution = event.l_total;
        event.object_id = hit.object_id;
        debug->add(event);
    }

    return local_indirect;
}

float trace_spectral(const Scene& scene, Ray2 ray, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug, float wavelength_nm) {
    return trace_recursive_spectral(scene, ray, sampler, settings, debug, 0, 1.0f, wavelength_nm);
}

Color estimate_at_spectral_path_tracing(const Scene& scene, Vec2 position, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug) {
    const DirectionSample direction = sample_uniform_circle_2d(sampler);

    if (debug) {
        debug->begin_path();
        DebugEvent event;
        event.type = DebugEventType::BsdfSample;
        event.depth = -1;
        event.hit_point = position;
        event.sampled_dir = direction.dir;
        event.pdf = direction.pdf;
        event.beta = make_color(1.0f);
        debug->add(event);
    }

    XYZ xyz_sum;
    const int wavelength_samples = std::clamp(settings.spectral.wavelength_samples, 1, 64);
    for (int i = 0; i < wavelength_samples; ++i) {
        const SpectralWavelengthSample wl = sample_wavelength(sampler, settings.spectral.xyz_importance);
        DebugRecorder* wavelength_debug = (i == 0) ? debug : nullptr;
        const float radiance = trace_spectral(scene, {position, direction.dir}, sampler, settings, wavelength_debug, wl.wavelength_nm);
        xyz_sum += wavelength_contribution_to_xyz(radiance, wl);
    }

    if (debug) {
        debug->end_path();
    }

    const XYZ xyz = xyz_sum / static_cast<float>(wavelength_samples);
    return clamp_non_negative(xyz_to_linear_srgb(xyz));
}

Color estimate_at_spectral_ris_direction(const Scene& scene, Vec2 position, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug, RISDirection& ris_direction) {
    const auto& ris_settings = settings.ris_direction;
    const int candidate_count = std::max(1, ris_settings.candidate_count);
    const int wavelength_samples = std::clamp(settings.spectral.wavelength_samples, 1, 64);
    constexpr float inv_two_pi = 1.0f / (2.0f * std::numbers::pi_v<float>);
    const float inv_candidate_count = 1.0f / static_cast<float>(candidate_count);

    XYZ xyz_sum;
    for (int wavelength_index = 0; wavelength_index < wavelength_samples; ++wavelength_index) {
        const SpectralWavelengthSample wl = sample_wavelength(sampler, settings.spectral.xyz_importance);
        DebugRecorder* wavelength_debug = (wavelength_index == 0) ? debug : nullptr;

        std::vector<RISDirection::AngularSample> angular_samples(static_cast<size_t>(candidate_count));
        std::vector<float> weighted_spectral_contributions(static_cast<size_t>(candidate_count), 0.0f);
        std::vector<float> weighted_y_contributions(static_cast<size_t>(candidate_count), 0.0f);

        for (int i = 0; i < candidate_count; ++i) {
            angular_samples[static_cast<size_t>(i)] = ris_direction.sample();
            const float dir_pdf = std::max(angular_samples[static_cast<size_t>(i)].pdf, 1.0e-8f);
            const Vec2 dir{std::cos(angular_samples[static_cast<size_t>(i)].theta), std::sin(angular_samples[static_cast<size_t>(i)].theta)};

            if (wavelength_debug) {
                wavelength_debug->begin_path();
            }

            const float radiance = trace_spectral(scene, {position, dir}, sampler, settings, wavelength_debug, wl.wavelength_nm);
            const float weighted_radiance = std::isfinite(radiance) && radiance > 0.0f ? radiance / dir_pdf : 0.0f;
            weighted_spectral_contributions[static_cast<size_t>(i)] = weighted_radiance;

            const XYZ candidate_xyz = wavelength_contribution_to_xyz(weighted_radiance, wl);
            weighted_y_contributions[static_cast<size_t>(i)] = std::max(0.0f, candidate_xyz.y);

            if (wavelength_debug) {
                wavelength_debug->end_path();
            }
        }

        // Phase 1 spectral RISDirection learning: keep the direction reservoir scalar and
        // update it using the XYZ Y component of the wavelength-corrected contribution.
        ris_direction.update(angular_samples, weighted_y_contributions);

        XYZ wavelength_xyz;
        for (int i = 0; i < candidate_count; ++i) {
            wavelength_xyz += wavelength_contribution_to_xyz(weighted_spectral_contributions[static_cast<size_t>(i)], wl) * (inv_two_pi * inv_candidate_count);
        }
        xyz_sum += wavelength_xyz;
    }

    const XYZ xyz = xyz_sum / static_cast<float>(wavelength_samples);
    return clamp_non_negative(xyz_to_linear_srgb(xyz));
}

} // namespace

PhotonMap build_photon_map(const Scene& scene, const IntegratorSettings& settings, uint64_t seed) {
    PhotonMap photon_map;
    const int photon_count = std::max(0, settings.photon_mapping.photon_count);
    if (photon_count <= 0 || scene.light_segment_ids.empty()) {
        return photon_map;
    }

    photon_map.photons.reserve(static_cast<size_t>(photon_count));
    for (int i = 0; i < photon_count; ++i) {
        Sampler sampler(hash_combine(seed, static_cast<uint64_t>(i)));
        const LightSample light = scene.sample_light(sampler.next1D(), sampler.next1D());
        if (light.pdf_length <= 0.0f || is_black(light.emission)) {
            continue;
        }

        const DirectionSample direction = sample_light_emission_direction(light.normal, light.emission_angle_deg, sampler);
        if (direction.pdf <= 0.0f) {
            continue;
        }

        const float cos_light = light_geometry_cosine(light.normal, direction.dir, light.emission_angle_deg);
        if (cos_light <= 0.0f) {
            continue;
        }

        const float inv_pdf = 1.0f / std::max(1.0e-8f, light.pdf_length * direction.pdf);
        const Color flux = light.emission * (cos_light * inv_pdf / static_cast<float>(photon_count));
        trace_one_photon(scene, {light.position + direction.dir * kEpsilon, direction.dir}, flux, sampler, settings, photon_map);
    }

    return photon_map;
}

Color trace(const Scene& scene, Ray2 ray, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug, const PhotonMap* photon_map) {
    return trace_recursive(scene, ray, sampler, settings, debug, 0, make_color(1.0f), photon_map);
}

Color estimate_at(const Scene& scene, Vec2 position, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug, const PhotonMap* photon_map, RISDirection* ris_direction) {
    if (settings.spectral.enabled && settings.kind == IntegratorKind::PathTracing) {
        (void)photon_map;
        (void)ris_direction;
        return estimate_at_spectral_path_tracing(scene, position, sampler, settings, debug);
    }

    if (settings.spectral.enabled && settings.kind == IntegratorKind::RISDirection) {
        (void)photon_map;
        if (ris_direction == nullptr) {
            std::cout << "RISDirection is not initialized. Returning black." << std::endl;
            return make_color(0.0f);
        }
        return estimate_at_spectral_ris_direction(scene, position, sampler, settings, debug, *ris_direction);
    }

    if (settings.kind != IntegratorKind::RISDirection) {
        DirectionSample direction = sample_uniform_circle_2d(sampler);

        if (debug) {
            debug->begin_path();
            DebugEvent event;
            event.type = DebugEventType::BsdfSample;
            event.depth = -1;
            event.hit_point = position;
            event.sampled_dir = direction.dir;
            event.pdf = direction.pdf;
            event.beta = make_color(1.0f);
            debug->add(event);
        }

        Color radiance = trace(scene, { position, direction.dir }, sampler, settings, debug, photon_map);
        if (debug) {
            debug->end_path();
        }

        return radiance;
    }

    { // mode : RIS Direction
        if (ris_direction == nullptr) {
            std::cout << "RISDirection is not initialized. Returning black." << std::endl;
            return make_color(0.0f);
        }
        const auto& _stg = settings.ris_direction;
        const int candidate_count = std::max(1, _stg.candidate_count);
        std::vector<RISDirection::AngularSample> angular_samples(static_cast<size_t>(candidate_count));
        std::vector<Color> weighed_contributions(static_cast<size_t>(candidate_count));

        for (int i = 0; i < candidate_count; ++i) {
            angular_samples[static_cast<size_t>(i)] = ris_direction->sample();
            const float pdf = std::max(angular_samples[static_cast<size_t>(i)].pdf, 1.0e-8f);
            const Vec2 dir{ std::cos(angular_samples[static_cast<size_t>(i)].theta), std::sin(angular_samples[static_cast<size_t>(i)].theta) };
            if (debug) {
                debug->begin_path();
            }
            weighed_contributions[static_cast<size_t>(i)] = trace(scene, { position, dir }, sampler, settings, debug, photon_map) / pdf;
            if (debug) {
                debug->end_path();
            }
        }
        ris_direction->update(angular_samples, weighed_contributions);
        
        Color radiance = make_color(0.0f);
        for (int i = 0; i < candidate_count; ++i) {
            radiance += weighed_contributions[static_cast<size_t>(i)] / (2.0f * std::numbers::pi_v<float>) * (1.0f / static_cast<float>(candidate_count));
        }

        return radiance;
    } // end of RIS Direction
}

} // namespace pt2d
