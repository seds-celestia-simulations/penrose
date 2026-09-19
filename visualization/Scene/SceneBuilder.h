#pragma once

#include "Scene.h"

#include <span>
#include <vector>

struct State;

namespace viz {

class SceneBuilder {
public:
    // Empty scene with only horizon_radius set. Prefer VisualizationConfig + build_scene().
    static Scene default_scene(float horizon_radius = 1.0f);
    static Scene from_states(std::span<const State> states, const std::string& name = "trajectory");
    static Scene from_states(const std::vector<State>& states, const std::string& name = "trajectory");
    static Scene from_trajectories(std::vector<Trajectory> trajectories, float horizon_radius = 1.0f);
};

} // namespace viz
