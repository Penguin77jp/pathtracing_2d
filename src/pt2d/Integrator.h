#pragma once

#include <cstdint>

#include "pt2d/DebugTrace.h"
#include "pt2d/Sampler.h"
#include "pt2d/Scene.h"

namespace pt2d {

enum class IntegratorKind {
    PathTracing,
    PathTracingNEE,      // next step: direct light sampling + shadow ray visualization
    BidirectionalPT      // reserved for eye/light subpaths and vertex connection
};

struct IntegratorSettings {
    IntegratorKind kind = IntegratorKind::PathTracing;
    int max_depth = 6;
    uint64_t seed = 1;
};

struct BsdfSample {
    Vec2 dir;
    float pdf = 0.0f;
};

struct DirectionSample {
    Vec2 dir;
    float pdf = 0.0f;
};

struct PathVertex {
    // Reserved for BDPT. The GUI/debugger already treats vertices as first-class data.
    Vec2 position;
    Vec2 normal;
    Color beta;
    int depth = 0;
    int object_id = -1;
};

BsdfSample sample_diffuse_cosine_2d(Vec2 normal, Sampler& sampler);
DirectionSample sample_uniform_circle_2d(Sampler& sampler);

// Trace radiance along a ray. This is still the reusable primitive for PT, NEE, and BDPT.
Color trace(const Scene& scene, Ray2 ray, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug);

// Estimate a 2D probe value at a world-space position by sampling directions over the full circle.
// Field rendering and clicked-pixel debugging both call this function, so the image and the path
// debugger use the same code path.
Color estimate_at(const Scene& scene, Vec2 position, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug);

} // namespace pt2d
