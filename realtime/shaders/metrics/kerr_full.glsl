const float rs = 0.25;
const float a_kerr = 0.99 * rs * 0.5;

float computeTdot(float r, float theta, float rdot, float thetadot, float phidot) {
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    float a2 = a_kerr * a_kerr;
    float Sigma = r * r + a2 * cosTheta * cosTheta;
    float Delta = r * r - rs * r + a2;

    float gTT = -(1.0 - rs * r / Sigma);
    float gTPh = -rs * r * a_kerr * sinTheta * sinTheta / Sigma;
    float gRR = Sigma / Delta;
    float gThTh = Sigma;
    float gPhPh = (r * r + a2 + rs * r * a2 * sinTheta * sinTheta / Sigma)
                  * sinTheta * sinTheta;

    float beta = gTPh * phidot;
    float gamma = gRR * rdot * rdot + gThTh * thetadot * thetadot + gPhPh * phidot * phidot;
    float disc = beta * beta - gTT * gamma;
    return (-beta - sqrt(max(disc, 0.0))) / gTT;
}

void geodesicDerivative(vec4 pos, vec4 vel, out vec4 dpos, out vec4 dvel) {
    float r = pos.y;
    float theta = pos.z;
    float tdot = vel.x;
    float rdot = vel.y;
    float thetadot = vel.z;
    float phidot = vel.w;

    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    float sin2Theta = sinTheta * sinTheta;
    float cos2Theta = cosTheta * cosTheta;
    float a2 = a_kerr * a_kerr;

    float Sigma = r * r + a2 * cos2Theta;
    float Sigma2 = Sigma * Sigma;
    float Delta = r * r - rs * r + a2;

    float dSigma_dr = 2.0 * r;
    float dSigma_dth = -2.0 * a2 * sinTheta * cosTheta;
    float dDelta_dr = 2.0 * r - rs;

    float dgTT_dr = rs * (a2 * cos2Theta - r * r) / Sigma2;
    float dgTT_dth = -2.0 * rs * r * a2 * sinTheta * cosTheta / Sigma2;

    float dgTPh_dr = -rs * a_kerr * sin2Theta * (a2 * cos2Theta - r * r) / Sigma2;
    float dgTPh_dth = -2.0 * rs * r * a_kerr * sinTheta * cosTheta * (r * r + a2) / Sigma2;

    float Delta2 = Delta * Delta;
    float dgRR_dr = (dSigma_dr * Delta - Sigma * dDelta_dr) / Delta2;
    float dgRR_dth = dSigma_dth / Delta;

    float dgThTh_dr = dSigma_dr;
    float dgThTh_dth = dSigma_dth;

    float sin3Theta = sin2Theta * sinTheta;
    float sin4Theta = sin2Theta * sin2Theta;
    float dgPhPh_dr = 2.0 * r * sin2Theta
                     + rs * a2 * sin4Theta * (a2 * cos2Theta - r * r) / Sigma2;
    float dgPhPh_dth = 2.0 * (r * r + a2) * sinTheta * cosTheta
                      + 2.0 * rs * r * a2 * sin3Theta * cosTheta
                        * (2.0 * Sigma + a2 * sin2Theta) / Sigma2;

    float t2 = tdot * tdot;
    float r2 = rdot * rdot;
    float th2 = thetadot * thetadot;
    float ph2 = phidot * phidot;

    float S_t = -(dgTT_dr * rdot * tdot + dgTT_dth * thetadot * tdot
                + dgTPh_dr * rdot * phidot + dgTPh_dth * thetadot * phidot);

    float S_r = 0.5 * dgTT_dr * t2 + dgTPh_dr * tdot * phidot
              - 0.5 * dgRR_dr * r2 + 0.5 * dgThTh_dr * th2 + 0.5 * dgPhPh_dr * ph2
              - dgRR_dth * thetadot * rdot;

    float S_th = 0.5 * dgTT_dth * t2 + dgTPh_dth * tdot * phidot
               + 0.5 * dgRR_dth * r2 - 0.5 * dgThTh_dth * th2 + 0.5 * dgPhPh_dth * ph2
               - dgThTh_dr * rdot * thetadot;

    float S_ph = -(dgTPh_dr * rdot * tdot + dgTPh_dth * thetadot * tdot
                 + dgPhPh_dr * rdot * phidot + dgPhPh_dth * thetadot * phidot);

    float gInvTT = -(r * r + a2 + rs * r * a2 * sin2Theta / Sigma) / Delta;
    float gInvTPh = -rs * r * a_kerr / (Sigma * Delta);
    float gInvRR = Delta / Sigma;
    float gInvThTh = 1.0 / Sigma;
    float gInvPhPh = (1.0 - rs * r / Sigma) / (Delta * max(sin2Theta, 1e-12));

    dpos = vel;
    dvel.x = gInvTT * S_t + gInvTPh * S_ph;
    dvel.y = gInvRR * S_r;
    dvel.z = gInvThTh * S_th;
    dvel.w = gInvTPh * S_t + gInvPhPh * S_ph;
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
    float cosTheta = cos(theta);
    float cosPhi = cos(phi);
    float sinPhi = sin(phi);
    return vec3(
        (r * cosPhi + a_kerr * sinPhi) * sinTheta,
        (r * sinPhi - a_kerr * cosPhi) * sinTheta,
        r * cosTheta
    );
}
