#pragma once

#include "../Geometry/Math.h"

#include <string>
#include <vector>

namespace viz {

struct TrajectorySample {
    double parameter = 0.0;
    Vec3 position;
};

struct TrajectoryStyle {
    Color4 color = Color4::rgb(255, 215, 100);
    Color4 glow_color = Color4::rgb(255, 150, 45, 120);
    bool gradient_along_path = false;
    Color4 gradient_start = Color4::rgb(255, 255, 245);
    Color4 gradient_end = Color4::rgb(255, 215, 100);
    float line_width = 2.5f;
    float marker_radius = 0.07f;
    float marker_glow_scale = 2.5f;
    float marker_brightness = 1.27f;
    float trail_rgb_min = 0.38f;
    float trail_alpha_min = 0.10f;
    bool show_trail = true;
    bool show_marker = true;
};

// Immutable visualization trajectory. Positions are Cartesian (x, y, z) in rs=1 units.
class Trajectory {
public:
    Trajectory(std::string name, std::vector<TrajectorySample> samples, TrajectoryStyle style,
               bool inferred_equatorial = false);

    const std::string& name() const { return name_; }
    const std::vector<TrajectorySample>& samples() const { return samples_; }
    const TrajectoryStyle& style() const { return style_; }
    bool inferred_equatorial() const { return inferred_equatorial_; }
    double min_parameter() const;
    double max_parameter() const;
    std::size_t sample_count() const { return samples_.size(); }

    // Index of the last sample with parameter <= value.
    std::size_t index_at_parameter(double value) const;

private:
    std::string name_;
    std::vector<TrajectorySample> samples_;
    TrajectoryStyle style_;
    bool inferred_equatorial_;
};

// Reduce sample count for interactive rendering while preserving endpoints.
void decimate_trajectory_samples(std::vector<TrajectorySample>& samples, std::size_t max_samples);

} // namespace viz
