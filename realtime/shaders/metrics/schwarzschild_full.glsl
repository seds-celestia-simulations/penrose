const float rs = 0.25;

float computeTdot(float r, float theta, float rdot, float thetadot, float phidot) {
    float f = max(1.0 - rs / r, 1e-6);
    float sinTheta = sin(theta);
    return sqrt(rdot * rdot / (f * f)
              + (r * r / f) * (thetadot * thetadot + sinTheta * sinTheta * phidot * phidot));
}

void geodesicDerivative(vec4 pos, vec4 vel, out vec4 dpos, out vec4 dvel) {
    float r = pos.y;
    float theta = pos.z;
    float f = max(1.0 - rs / r, 1e-6);
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    float sin2Theta = sinTheta * sinTheta;

    float tdot = vel.x;
    float rdot = vel.y;
    float thetadot = vel.z;
    float phidot = vel.w;

    dpos = vel;

    dvel.x = -(rs / (r * r * f)) * 2.0 * tdot * rdot;

    dvel.y = -(rs * f / (2.0 * r * r)) * tdot * tdot
             + (rs / (2.0 * r * r * f)) * rdot * rdot
             + r * f * thetadot * thetadot
             + r * f * sin2Theta * phidot * phidot;

    dvel.z = -(2.0 / r) * rdot * thetadot
             + sinTheta * cosTheta * phidot * phidot;

    dvel.w = -(2.0 / r) * rdot * phidot
             - 2.0 * (cosTheta / max(sinTheta, 1e-6)) * thetadot * phidot;
}

void geodesicRK4(inout vec4 pos, inout vec4 vel, float h) {
    vec4 k1p, k1v, k2p, k2v, k3p, k3v, k4p, k4v;

    geodesicDerivative(pos, vel, k1p, k1v);
    geodesicDerivative(pos + 0.5*h*k1p, vel + 0.5*h*k1v, k2p, k2v);
    geodesicDerivative(pos + 0.5*h*k2p, vel + 0.5*h*k2v, k3p, k3v);
    geodesicDerivative(pos + h*k3p, vel + h*k3v, k4p, k4v);

    pos += (h/6.0) * (k1p + 2.0*k2p + 2.0*k3p + k4p);
    vel += (h/6.0) * (k1v + 2.0*k2v + 2.0*k3v + k4v);
}

vec3 cartesianFromCoord(vec4 pos) {
    float r = pos.y;
    float theta = pos.z;
    float phi = pos.w;
    float sinTheta = sin(theta);
    return vec3(
        r * sinTheta * cos(phi),
        r * sinTheta * sin(phi),
        r * cos(theta)
    );
}
