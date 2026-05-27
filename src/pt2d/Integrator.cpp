#include "pt2d/Integrator.h"

#include <algorithm>
#include <cmath>

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
    const float cos_light = std::max(0.0f, dot(light.normal, -wi));

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
    Color beta)
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

    const Material& material = scene.materials[hit.material_id];
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
        const Color emitted = count_emission ? material.emission : make_color(0.0f);
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
        const Color next_radiance = trace_recursive(scene, next_ray, sampler, settings, debug, depth + 1, next_beta);
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
    if (settings.kind == IntegratorKind::PathTracingNEE) {
        local_nee = estimate_direct_nee(scene, hit, material, shading_normal, beta, sampler, debug, depth);
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
            const Color next_radiance = trace_recursive(scene, next_ray, sampler, settings, debug, depth + 1, next_beta);
            local_indirect = bsdf_weight * next_radiance;
        }
    }

    const Color local_total = local_nee + local_indirect;

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

} // namespace

Color trace(const Scene& scene, Ray2 ray, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug) {
    return trace_recursive(scene, ray, sampler, settings, debug, 0, make_color(1.0f));
}

Color estimate_at(const Scene& scene, Vec2 position, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug) {
    const DirectionSample direction = sample_uniform_circle_2d(sampler);

    if (debug) {
        DebugEvent event;
        event.type = DebugEventType::BsdfSample;
        event.depth = -1;
        event.hit_point = position;
        event.sampled_dir = direction.dir;
        event.pdf = direction.pdf;
        event.beta = make_color(1.0f);
        debug->add(event);
    }

    return trace(scene, {position, direction.dir}, sampler, settings, debug);
}

} // namespace pt2d
