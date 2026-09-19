const float rs = 0.25;
void orbitDerivative(float u, float v, out float du, out float dv) {
    du = v;
    dv = -u + 1.5 * rs * u * u;
}

void orbitRK4(inout float u, inout float v, float h) {
    float k1u, k1v, k2u, k2v, k3u, k3v, k4u, k4v;
    orbitDerivative(u, v, k1u, k1v);
    orbitDerivative(u + 0.5 * h * k1u, v + 0.5 * h * k1v, k2u, k2v);
    orbitDerivative(u + 0.5 * h * k2u, v + 0.5 * h * k2v, k3u, k3v);
    orbitDerivative(u + h * k3u, v + h * k3v, k4u, k4v);
    u += (h / 6.0) * (k1u + 2.0 * k2u + 2.0 * k3u + k4u);
    v += (h / 6.0) * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
}

vec3 orbitPosition(float r, float psi, vec3 eR, vec3 eT) {
    return r * (cos(psi) * eR + sin(psi) * eT);
}
