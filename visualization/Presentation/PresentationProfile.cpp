#include "PresentationProfile.h"

namespace viz {

PresentationProfile PresentationProfile::cinematic_default() {
    PresentationProfile profile;
    profile.enabled = true;
    profile.bloom_threshold = 0.55f;
    profile.bloom_intensity = 0.85f;
    profile.bloom_radius = 3;
    profile.lensing_strength = 0.07f;
    profile.lensing_radius_scale = 1.85f;
    profile.contrast = 1.12f;
    profile.saturation = 1.15f;
    profile.vignette = 0.22f;
    profile.halo_radius_scale = 1.12f;
    profile.halo_strength = 0.0f;
    profile.lens_ring_strength = 0.62f;
    profile.lens_ring_radius_scale = 1.045f;
    profile.lens_ring_width_scale = 0.032f;
    return profile;
}

PresentationProfile PresentationProfile::interactive_default() {
    return PresentationProfile::cinematic_default();
}

} // namespace viz
