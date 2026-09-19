#include "PresentationDefaults.h"
#include "VisualizationConfig.h"

namespace viz {

SceneSettings cinematic_scene_settings(float horizon_radius) {
    SceneSettings settings;
    settings.horizon_radius = horizon_radius;
    settings.show_photon_sphere = false;
    settings.show_reference_ring = false;
    settings.show_event_horizon = true;
    settings.show_accretion_disk = false;
    settings.show_starfield = true;
    settings.use_image_starfield = false;
    settings.background = Color4::rgb(2, 4, 12);
    return settings;
}

void apply_cinematic_camera(Camera& camera) {
    CameraConfig config;
    config.auto_frame = false;
    config.distance = 7.5f;
    config.yaw = -1.05f;
    config.pitch = 0.52f;
    apply_camera_config(camera, Scene{}, config);
}

void frame_camera_on_trajectories(Camera& camera, const Scene& scene) {
    CameraConfig config;
    apply_camera_config(camera, scene, config);
}

} // namespace viz
