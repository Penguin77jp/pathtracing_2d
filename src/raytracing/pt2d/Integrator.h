#pragma once

#include <cstdint>
#include <vector>

#include "pt2d/DebugTrace.h"
#include "pt2d/Sampler.h"
#include "pt2d/Scene.h"
#include "pt2d/RISDirection.h"

namespace pt2d {

enum class IntegratorKind {
    PathTracing,
    PathTracingNEE,
    RISDirection,
    PhotonMapping
};

struct PhotonMappingSettings {
    int photon_count = 20000;
    int photon_max_depth = 8;
    float gather_radius = 0.12f;
    float strength = 1.0f;
    bool caustics_only = true;
};

struct RISDirectionSettings {
    int num_bins = 16;
    float min_probability_percent = 10;
    int smooth_sigma_deg = 15;
    int candidate_count = 16;

    bool spatial_reuse_enabled = false;
    int spatial_radius = 1;
    float spatial_strength = 0.25f;
    int spatial_interval = 1;
};

struct SpectralSettings {
    bool enabled = false;
    int wavelength_samples = 1;
    bool xyz_importance = true;
};

struct IntegratorSettings {
    IntegratorKind kind = IntegratorKind::PathTracing;
    int max_depth = 6;
    uint64_t seed = 1;
    PhotonMappingSettings photon_mapping;
	RISDirectionSettings ris_direction;
	SpectralSettings spectral;
};

struct Photon {
    Vec2 position;
    Vec2 incoming_dir; // Direction the photon traveled when it hit the surface.
    Vec2 normal;
    Color flux = make_color(0.0f);
    bool caustic = false;
    int bounce_count = 0;
};

struct PhotonMap {
    std::vector<Photon> photons;
};

struct BsdfSample {
    Vec2 dir;
    float pdf = 0.0f;
};

struct DirectionSample {
    Vec2 dir;
    float pdf = 0.0f;
};

BsdfSample sample_diffuse_cosine_2d(Vec2 normal, Sampler& sampler);
DirectionSample sample_uniform_circle_2d(Sampler& sampler);

// Build a 2D caustic photon map by shooting photons from segment lights through dielectric objects.
PhotonMap build_photon_map(const Scene& scene, const IntegratorSettings& settings, uint64_t seed);

// Trace radiance along a ray. This is the reusable primitive for PT, NEE, and photon final gathering.
Color trace(const Scene& scene, Ray2 ray, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug, const PhotonMap* photon_map = nullptr);

// Estimate a 2D probe value at a world-space position by sampling directions over the full circle.
// Field rendering and clicked-pixel debugging both call this function, so the image and the path
// debugger use the same code path.
Color estimate_at(const Scene& scene, Vec2 position, Sampler& sampler, const IntegratorSettings& settings, DebugRecorder* debug, const PhotonMap* photon_map = nullptr, RISDirection* ris_direction=nullptr);

} // namespace pt2d
