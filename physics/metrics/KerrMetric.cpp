#include "KerrMetric.h"

#include <array>
#include <cmath>

namespace Spacetime {
namespace {

constexpr int kDim = 4;

struct KerrGeometry {
    double rs = 0.0;
    double a = 0.0;
    double r = 0.0;
    double theta = 0.0;
    double sin_theta = 0.0;
    double cos_theta = 0.0;
    double sigma = 0.0;
    double delta = 0.0;
    double A = 0.0;
};

KerrGeometry make_geometry(double rs, double spin, const Eigen::Vector4d& X) {
    KerrGeometry geometry;
    geometry.rs = rs;
    geometry.a = spin;
    geometry.r = X[1];
    geometry.theta = X[2];
    geometry.sin_theta = std::sin(geometry.theta);
    geometry.cos_theta = std::cos(geometry.theta);
    geometry.sigma = geometry.r * geometry.r + geometry.a * geometry.a * geometry.cos_theta * geometry.cos_theta;
    geometry.delta = geometry.r * geometry.r - geometry.rs * geometry.r + geometry.a * geometry.a;
    const double sin2 = geometry.sin_theta * geometry.sin_theta;
    const double rho2 = geometry.r * geometry.r + geometry.a * geometry.a;
    geometry.A = rho2 * rho2 - geometry.a * geometry.a * geometry.delta * sin2;
    return geometry;
}

using Matrix4 = std::array<std::array<double, kDim>, kDim>;

Matrix4 metric_covariant(const KerrGeometry& g) {
    Matrix4 metric{};
    const double sin2 = g.sin_theta * g.sin_theta;
    metric[0][0] = -(1.0 - g.rs * g.r / g.sigma);
    metric[0][3] = metric[3][0] = -g.rs * g.a * g.r * sin2 / g.sigma;
    metric[1][1] = g.sigma / g.delta;
    metric[2][2] = g.sigma;
    metric[3][3] = sin2 * g.A / g.sigma;
    return metric;
}

Matrix4 metric_contravariant(const KerrGeometry& g) {
    Matrix4 inverse{};
    const double sin2 = g.sin_theta * g.sin_theta;
    const double denom = g.sigma * g.delta;
    inverse[0][0] = g.A / denom;
    inverse[0][3] = inverse[3][0] = g.rs * g.a * g.r / denom;
    inverse[1][1] = g.delta / g.sigma;
    inverse[2][2] = 1.0 / g.sigma;
    inverse[3][3] = (g.delta - g.a * g.a * sin2) / denom;
    return inverse;
}

Matrix4 metric_derivative_r(const KerrGeometry& g) {
    Matrix4 derivative{};
    const double sin2 = g.sin_theta * g.sin_theta;
    const double sigma_r = 2.0 * g.r;
    const double delta_r = 2.0 * g.r - g.rs;
    const double rho2 = g.r * g.r + g.a * g.a;
    const double A_r = 4.0 * g.r * rho2 - g.a * g.a * sin2 * delta_r;

    derivative[0][0] = g.rs * (g.sigma - g.r * sigma_r) / (g.sigma * g.sigma);
    derivative[0][3] = derivative[3][0] =
        -g.rs * g.a * sin2 * (g.sigma - g.r * sigma_r) / (g.sigma * g.sigma);
    derivative[1][1] = (sigma_r * g.delta - g.sigma * delta_r) / (g.delta * g.delta);
    derivative[2][2] = sigma_r;
    derivative[3][3] = sin2 * (A_r * g.sigma - g.A * sigma_r) / (g.sigma * g.sigma);
    return derivative;
}

Matrix4 metric_derivative_theta(const KerrGeometry& g) {
    Matrix4 derivative{};
    const double sin2 = g.sin_theta * g.sin_theta;
    const double sin_cos = g.sin_theta * g.cos_theta;
    const double sigma_theta = -2.0 * g.a * g.a * sin_cos;
    const double A_theta = -2.0 * g.a * g.a * g.a * g.a * sin_cos * g.delta;

    derivative[0][0] = g.rs * g.r * sigma_theta / (g.sigma * g.sigma);
    derivative[0][3] = derivative[3][0] =
        -g.rs * g.a * g.r * (sin2 * sigma_theta + 2.0 * sin_cos * g.sigma) / (g.sigma * g.sigma);
    derivative[1][1] = sigma_theta / g.delta;
    derivative[2][2] = sigma_theta;
    derivative[3][3] =
        (2.0 * sin_cos * g.A + sin2 * (A_theta * g.sigma - g.A * sigma_theta)) / (g.sigma * g.sigma);
    return derivative;
}

double partial_metric(int coord, int mu, int nu, const KerrGeometry& geometry) {
    if (coord == 1) {
        return metric_derivative_r(geometry)[mu][nu];
    }
    if (coord == 2) {
        return metric_derivative_theta(geometry)[mu][nu];
    }
    return 0.0;
}

} // namespace

KerrMetric::KerrMetric(double rs, double spin) : rs_(rs), spin_(spin) {}

double KerrMetric::outer_horizon_radius() const {
    const double M = rs_ / 2.0;
    return M + std::sqrt(std::max(M * M - spin_ * spin_, 0.0));
}

double KerrMetric::christoffel(int mu, int alpha, int beta, const Eigen::Vector4d& X) const {
    if (alpha > beta) {
        std::swap(alpha, beta);
    }

    const KerrGeometry geometry = make_geometry(rs_, spin_, X);
    const Matrix4 inverse = metric_contravariant(geometry);

    double connection = 0.0;
    for (int sigma = 0; sigma < kDim; ++sigma) {
        const double dg_beta_sigma_alpha = partial_metric(alpha, beta, sigma, geometry);
        const double dg_alpha_sigma_beta = partial_metric(beta, alpha, sigma, geometry);
        const double dg_sigma_alpha_beta = partial_metric(sigma, alpha, beta, geometry);
        connection += inverse[mu][sigma] * (dg_beta_sigma_alpha + dg_alpha_sigma_beta - dg_sigma_alpha_beta);
    }
    return 0.5 * connection;
}

} // namespace Spacetime
