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
    int path_id = -1;
};

class DebugRecorder {
public:
    void clear() {
        events_.clear();
        current_path_id_ = -1;
        next_path_id_ = 0;
    }

    const std::vector<DebugEvent>& events() const { return events_; }

    int begin_path() {
        current_path_id_ = next_path_id_++;
        return current_path_id_;
    }

    void end_path() { current_path_id_ = -1; }

    int current_path_id() const { return current_path_id_; }

    void add(DebugEvent event) {
        if (event.path_id < 0) {
            event.path_id = current_path_id_;
        }
        events_.push_back(event);
    }

private:
    std::vector<DebugEvent> events_;
    int current_path_id_ = -1;
    int next_path_id_ = 0;
};

} // namespace pt2d
