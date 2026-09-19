#pragma once

#include "../Presentation/VisualizationConfig.h"
#include "../Camera/Camera.h"
#include "../Scene/Scene.h"

namespace viz {

// Interactive viewer. Presentation, playback, and resolution come from VisualizationConfig.
int run_interactive_viewer(Scene scene, Camera camera, const VisualizationConfig& config);

} // namespace viz
