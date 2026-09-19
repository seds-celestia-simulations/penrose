#include "Scene.h"

#include <algorithm>

namespace viz {

Scene::Scene(SceneSettings settings) : settings_(settings) {
    const float rs = settings_.horizon_radius;
    black_hole_mesh_ = make_uv_sphere(24, 36, rs * 0.35f);
    horizon_mesh_ = make_uv_sphere(24, 36, rs);
    photon_sphere_mesh_ = make_uv_sphere(16, 24, rs * 1.5f);
    disk_mesh_ = make_annulus(rs * 1.5f, rs * 8.0f, 96);
    reference_ring_mesh_ = make_annulus(rs * 3.8f, rs * 4.2f, 128);
}

void Scene::add_trajectory(Trajectory trajectory) {
    trajectories_.push_back(std::move(trajectory));
    recompute_playback_duration();
}

void Scene::clear_trajectories() {
    trajectories_.clear();
    recompute_playback_duration();
}

void Scene::recompute_playback_duration() {
    double max_param = 1.0;
    for (const Trajectory& traj : trajectories_) {
        max_param = std::max(max_param, traj.max_parameter());
    }
    playback_.duration = std::max(max_param, 1e-6);
    playback_.time = std::clamp(playback_.time, 0.0, playback_.duration);
}

} // namespace viz
