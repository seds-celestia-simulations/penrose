#pragma once

#include "../Geometry/Math.h"
#include "../Geometry/Mesh.h"
#include "../Trajectory/Trajectory.h"

#include <memory>
#include <string>
#include <vector>

namespace viz {

struct SceneSettings {
    // Geometric radius of the absorbing central region (e.g. horizon scale from the solver).
    // Not tied to a specific metric implementation.
    float horizon_radius = 1.0f;
    bool show_starfield = true;
    bool use_image_starfield = false;
    float starfield_brightness = 1.0f;
    bool show_accretion_disk = false;
    bool show_event_horizon = true;
    bool show_photon_sphere = false;
    bool show_reference_ring = false;
    Color4 background = Color4::rgb(2, 4, 12);
};

struct PlaybackState {
    double time = 0.0;
    double duration = 1.0;
    bool playing = false;
    float speed = 1.0f;
    int selected_trajectory = 0;
};

class Scene {
public:
    explicit Scene(SceneSettings settings = {});

    const SceneSettings& settings() const { return settings_; }
    const std::vector<Trajectory>& trajectories() const { return trajectories_; }
    const Mesh& black_hole_mesh() const { return black_hole_mesh_; }
    const Mesh& horizon_mesh() const { return horizon_mesh_; }
    const Mesh& photon_sphere_mesh() const { return photon_sphere_mesh_; }
    const Mesh& disk_mesh() const { return disk_mesh_; }
    const Mesh& reference_ring_mesh() const { return reference_ring_mesh_; }
    PlaybackState& playback() { return playback_; }
    const PlaybackState& playback() const { return playback_; }

    void add_trajectory(Trajectory trajectory);
    void clear_trajectories();
    void recompute_playback_duration();

private:
    SceneSettings settings_;
    std::vector<Trajectory> trajectories_;
    Mesh black_hole_mesh_;
    Mesh horizon_mesh_;
    Mesh photon_sphere_mesh_;
    Mesh disk_mesh_;
    Mesh reference_ring_mesh_;
    PlaybackState playback_;
};

} // namespace viz
