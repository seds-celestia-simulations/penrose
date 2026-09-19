#pragma once
#include <Eigen/Dense>

using namespace Eigen;

// Geodesic state: position X and tangent U in a chosen coordinate chart.
struct State {
    Vector4d X;
    Vector4d U;

    State() {
        X = Vector4d::Zero();
        U = Vector4d::Zero();
    }

    State(Vector4d pos, Vector4d vel) {
        X = pos;
        U = vel;
    }

    State operator+(const State& other) const { return State(X + other.X, U + other.U); }
    State operator*(double scalar) const { return State(X * scalar, U * scalar); }
    friend State operator*(double scalar, const State& s) { return s * scalar; }
};
