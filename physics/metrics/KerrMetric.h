#pragma once

#include <spacetime/Metric.h>

namespace Spacetime {

class KerrMetric : public Metric {
public:
    KerrMetric(double rs, double spin);

    double christoffel(int mu, int alpha, int beta, const Eigen::Vector4d& X) const override;

    double rs() const { return rs_; }
    double spin() const { return spin_; }
    double outer_horizon_radius() const;

private:
    double rs_;
    double spin_;
};

} // namespace Spacetime
