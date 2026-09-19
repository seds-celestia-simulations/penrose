// ============================================================
// KERR CONSTANTS & UNIFORMS
// ============================================================
uniform float rs;      // Schwarzschild radius (2M)
uniform float a_kerr;  // Spin parameter (0.0 = Schwarzschild, < 0.5*rs = Kerr)

#define KERR_M (0.5 * rs)
const float KERR_PI = 3.14159265359;

// ============================================================
// KERR HELPER FUNCTIONS
// ============================================================

// Returns the radius of the outer event horizon (r+)
float kerrOuterHorizon() 
{
    float M = KERR_M;
    return M + sqrt(max(M * M - a_kerr * a_kerr, 0.0));
}

// Computes the Mino-time derivative for the phi coordinate
float kerrPhiMinoDerivative(float r, float theta, float xi) 
{
    float a = a_kerr;
    // Reverted to 1e-8 for maximum precision!
    float s = sin(clamp(theta, 1e-5, KERR_PI - 1e-5));
    float s2 = max(s*s,1e-5); 
    
    // Protect against mid-step horizon dipping
    float rPlus = KERR_M + sqrt(max(KERR_M * KERR_M - a * a, 0.0));
    float safeR = max(r, rPlus + 0.005);
    
    float Delta = safeR * safeR - rs * safeR + a * a;
    float P = safeR * safeR + a * a - a * xi;
    
    return (a * P) / max(Delta, 1e-8) + (xi / max(s2, 1e-8)) - a;
}

// Solves the null geodesic equation (p^mu p_mu = 0) to find the initial time derivative
float computeKerrTdot(float r, float theta, float rdot, float thetadot, float phidot) 
{
    float a = a_kerr;
    float s = sin(theta);
    float c = cos(theta);
    float s2 = max(s * s, 1e-8);
    
    float Sigma = r * r + a * a * c * c;
    float Delta = r * r - rs * r + a * a;

    // Covariant metric components
    float gTT = -(1.0 - rs * r / Sigma);
    float gTPh = -rs * r * a * s2 / Sigma;
    float gPhPh = (r * r + a * a + rs * r * a * a * s2 / Sigma) * s2;
    float gRR = Sigma / max(Delta, 1e-8);
    float gThTh = Sigma;

    // Quadratic formula coefficients for A*tdot^2 + B*tdot + C = 0
    float A = gTT;
    float B = 2.0 * gTPh * phidot;
    float C = gPhPh * phidot * phidot + gRR * rdot * rdot + gThTh * thetadot * thetadot;

    float disc = max(B * B - 4.0 * A * C, 0.0);
    return (-B - sqrt(disc)) / (2.0 * A); 
}

// Converts Boyer-Lindquist spherical coordinates to purely mathematical Cartesian (Z-Up)
vec3 kerrCartesianFromCoord(vec4 state) 
{
    float r = state.x;
    float theta = state.y;
    float phi = state.z;
    float a = a_kerr;

    float sinTheta = sin(theta);
    float x = sqrt(r * r + a * a) * sinTheta * cos(phi);
    float y = sqrt(r * r + a * a) * sinTheta * sin(phi);
    float z = r * cos(theta);

    return vec3(x, y, z);
}

// Converts the 4D state momentum into a 3D Cartesian velocity vector
vec3 kerrCartesianVelocity(float r, float theta, float phi, float rdot, float thetadot, float phidot) 
{
    float a = a_kerr;
    float sTh = sin(theta);
    float cTh = cos(theta);
    float sPh = sin(phi);
    float cPh = cos(phi);
    
    float rSq_aSq_sqrt = sqrt(r * r + a * a);
    
    float xdot = (r * rdot / rSq_aSq_sqrt) * sTh * cPh + rSq_aSq_sqrt * cTh * cPh * thetadot - rSq_aSq_sqrt * sTh * sPh * phidot;
    float ydot = (r * rdot / rSq_aSq_sqrt) * sTh * sPh + rSq_aSq_sqrt * cTh * sPh * thetadot + rSq_aSq_sqrt * sTh * cPh * phidot;
    float zdot = rdot * cTh - r * sTh * thetadot;

    return vec3(xdot, ydot, zdot);
}

// ------------------------------------------------------------
// Kerr Integrator
// ------------------------------------------------------------

void kerrReducedDerivative(vec4 state, float thetaDot, float xi, float eta, out vec4 dState, out float dThetaDot)
{
    // The Event Horizon Brick Wall
    float rPlus = KERR_M + sqrt(max(KERR_M * KERR_M - a_kerr * a_kerr, 0.0));
    float r = max(state.x, rPlus + 0.005);
    
    float theta = state.y;
    float rDot = state.w;

    float s = sin(theta);
    float c = cos(theta);
    
    // The Polar Force Limiter
    // Prevents near-zero angular momentum rays from exploding at the poles,
    // while maintaining double-precision accuracy for the accretion disk.
    float s2 = max(s * s, 1e-5); 
    float s3 = max(s2 * abs(s), 1e-7); 

    float a = a_kerr;
    float a2 = a * a;
    float r2 = r * r;

    float Delta = r2 - rs * r + a2;
    float P = r2 + a2 - a * xi;

    float Rprime = 4.0 * r * P - (2.0 * r - rs) * ((xi - a) * (xi - a) + eta);
    float ThetaPrime = -2.0 * a2 * s * c + (2.0 * xi * xi * c) / s3;

    float phiDot = (a * P) / max(Delta, 1e-8) + (xi / s2) - a;

    dState = vec4(rDot, thetaDot, phiDot, 0.5 * Rprime);
    dThetaDot = 0.5 * ThetaPrime;
}

void kerrReducedRK4(inout vec4 state, inout float thetaDot, float xi, float eta, float h)
{
    vec4 k1, k2, k3, k4;
    float kt1, kt2, kt3, kt4;

    kerrReducedDerivative(state, thetaDot, xi, eta, k1, kt1);
    kerrReducedDerivative(state + 0.5 * h * k1, thetaDot + 0.5 * h * kt1, xi, eta, k2, kt2);
    kerrReducedDerivative(state + 0.5 * h * k2, thetaDot + 0.5 * h * kt2, xi, eta, k3, kt3);
    kerrReducedDerivative(state + h * k3, thetaDot + h * kt3, xi, eta, k4, kt4);

    state += (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
    thetaDot += (h / 6.0) * (kt1 + 2.0 * kt2 + 2.0 * kt3 + kt4);
}

void initializeKerrReduced(float r, float theta, float rdot, float thetadot, float phidot, float tdot, out float xi, out float eta, out float rMino, out float thetaMino)
{
    float a = a_kerr;
    float a2 = a * a;
    
    float s = sin(theta);
    float c = cos(theta);
    float s2 = max(s * s, 1e-8);

    // Kerr Sigma
    float Sigma = r * r + a2 * c * c;

    // Conserved quantities
    float gTT = -(1.0 - rs * r / Sigma);
    float gTPh = -rs * r * a * s2 / Sigma;
    float gPhPh = (r * r + a2 + rs * r * a2 * s2 / Sigma) * s2;

    float pT = gTT * tdot + gTPh * phidot;
    float pPhi = gTPh * tdot + gPhPh * phidot;
    float pTheta = Sigma * thetadot;

    float E = -pT;
    float Lz = pPhi;
    float invE = 1.0 / max(abs(E), 1e-8);

    xi = Lz * invE;
    
    float Q = pTheta * pTheta + c * c * (-a2 * E * E + Lz * Lz / s2);
    eta = Q * invE * invE;

    // Convert affine parameter derivatives to Mino-time
    rMino = Sigma * rdot;
    thetaMino = Sigma * thetadot;
}