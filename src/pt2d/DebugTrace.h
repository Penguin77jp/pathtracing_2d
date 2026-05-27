#pragma once

#include <vector>

#include "pt2d/Color.h"
#include "pt2d/Math.h"

namespace pt2d {

enum class DebugEventType {
    RaySegment,
    Hit,
    BsdfSample,
    LightSample,
    ShadowRay,
    DirectContribution,
    HitLight,
    Miss,
    Terminated
};

struct DebugEvent {
    DebugEventType type = DebugEventType::Terminated;
    int depth = 0;

    Vec2 ray_origin;
    Vec2 ray_dir;
    Vec2 ray_end;

    Vec2 hit_point;
    Vec2 normal;

    Vec2 sampled_point;
    Vec2 sampled_dir;

    float pdf = 0.0f;
    float distance = 0.0f;
    float geometric_term = 0.0f;
    bool blocked = false;

    Color beta = make_color(1.0f);
    Color contribution = make_color(0.0f);
    Color l_nee = make_color(0.0f);
    Color l_path = make_color(0.0f);
    Color l_total = make_color(0.0f);

    int object_id = -1;
    int sampled_object_id = -1;
};

class DebugRecorder {
public:
    void clear() { events_.clear(); }
    const std::vector<DebugEvent>& events() const { return events_; }

    void add(DebugEvent event) { events_.push_back(event); }

private:
    std::vector<DebugEvent> events_;
};

} // namespace pt2d
