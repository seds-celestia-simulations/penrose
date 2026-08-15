void main()
{
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (texelCoord.x >= int(uResolution.x) || texelCoord.y >= int(uResolution.y))
    {
        return;
    }

    vec2 TexCoords = (vec2(texelCoord) + 0.5) / uResolution;
    vec4 ndcPos = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldPos = uInvProjView * ndcPos;

    vec3 rd = normalize(worldPos.xyz / worldPos.w - uCameraPos);
    vec3 ro = uCameraPos;

    // ------------------------------------------------------------
    // 1. EQUATORIAL SINGULARITY FIX
    // ------------------------------------------------------------
    if (abs(ro.y) < 0.001) 
    {
        ro.y = 0.001;
    }

    // ------------------------------------------------------------
    // 2. ANALYTICAL EMPTY SPACE FAST-FORWARD
    // ------------------------------------------------------------
    float b = dot(ro, rd);
    float c = dot(ro, ro) - (DISK_OUTER * DISK_OUTER);
    float disc = b * b - c;
    
    if (disc < 0.0) 
    {
        // Ray entirely misses the accretion disk and black hole bounding sphere.
        // Skip all Kerr RK4 math and instantly draw the skybox!
        vec3 skyColor = texture(skybox, DirectionToUV(rd)).rgb;
        imageStore(imgOutput, texelCoord, vec4(skyColor, 1.0));
        return; 
    }
    
    float t = -b - sqrt(disc);
    if (t > 0.0) 
    {
        // Fast-forward the ray origin straight to the bounding sphere
        ro += rd * t; 
    }

    // ------------------------------------------------------------
    // 3. MAP OPENGL (Y-UP) TO KERR MATH (Z-UP)
    // ------------------------------------------------------------
    vec3 kerr_ro = vec3(ro.x, -ro.z, ro.y);
    vec3 kerr_rd = vec3(rd.x, -rd.z, rd.y);

    // Initial Boyer-Lindquist coordinates (now safely in Z-Up space)
    float r0 = length(kerr_ro);
    float theta0 = acos(clamp(kerr_ro.z / r0, -1.0, 1.0));
    float phi0 = atan(kerr_ro.y, kerr_ro.x);

    float s0 = sin(theta0);
    float c0 = cos(theta0);
    float sp0 = sin(phi0);
    float cp0 = cos(phi0);

    // Spherical coordinate basis at camera.
    vec3 eR = vec3(s0 * cp0, s0 * sp0, c0);
    vec3 eTheta = vec3(c0 * cp0, c0 * sp0, -s0);
    vec3 ePhi = vec3(-sp0, cp0, 0.0);

    float rdot = dot(kerr_rd, eR);
    float thetadot = dot(kerr_rd, eTheta) / max(r0, 1e-6);
    float phidot = dot(kerr_rd, ePhi) / max(r0 * s0, 1e-6);

    // Initial t derivative from null constraint
    float tdot = computeKerrTdot(r0, theta0, rdot, thetadot, phidot);

    // Convert ray into Kerr conserved quantities
    float xi, eta, rMino, thetaMino;
    initializeKerrReduced(r0, theta0, rdot, thetadot, phidot, tdot, xi, eta, rMino, thetaMino);

    // State: x = r, y = theta, z = phi, w = dr/dgamma
    vec4 state = vec4(r0, theta0, phi0, rMino);
    float thetaVelocity = thetaMino;

    vec3 accumColor = vec3(0.0);
    float accumAlpha = 0.0;
    bool captured = false;
    bool escaped = false;

    float rPlus = kerrOuterHorizon(); 

    // Retrieve initial position and map BACK to OpenGL Y-Up space for the volume renderer
    vec3 kerrPos = kerrCartesianFromCoord(state);
    vec3 previousPos = vec3(kerrPos.x, kerrPos.z, -kerrPos.y);

    // ------------------------------------------------------------
    // Reduced Kerr integration
    // ------------------------------------------------------------
    const int MAX_STEPS = 400;

    for (int i = 0; i < MAX_STEPS; ++i)
    {
        float r = state.x;
        float theta = state.y;

        // Horizon check
        if (r <= rPlus * 1.001)
        {
            captured = true;
            break;
        }

        // Current phi derivative
        float phiVelocity = kerrPhiMinoDerivative(r, theta, xi);

        // Adaptive Mino-time step
        float maxDr;
        if (r > DISK_OUTER + 1.0) maxDr = 0.5;
        else if (r > DISK_INNER) maxDr = 0.12;
        else maxDr = 0.04;

        float h = 0.05;
        h = min(h, maxDr / max(abs(state.w), 1e-5));
        h = min(h, 0.04 / max(abs(thetaVelocity), 1e-5));
        h = min(h, 0.06 / max(abs(phiVelocity), 1e-5));
        // THE FIX: ZENO'S PARADOX BRAKING
        // Dynamically shrink the step size based on distance to the horizon.
        // It will never overshoot; it will just run out of steps and get safely trapped!
        h = min(h, max(r - rPlus, 0.0) * 0.25);
        
        h = clamp(h, 1e-5, 0.05);

        // Integrate
        kerrReducedRK4(state, thetaVelocity, xi, eta, h);

        // Handle spherical polar-coordinate pole crossing
        if (state.y < 0.0)
        {
            state.y = -state.y;
            state.z += KERR_PI;
            thetaVelocity = -thetaVelocity;
        }
        else if (state.y > KERR_PI)
        {
            state.y = 2.0 * KERR_PI - state.y;
            state.z += KERR_PI;
            thetaVelocity = -thetaVelocity;
        }

        // Get updated position and map back to OpenGL space
        vec3 currentKerrPos = kerrCartesianFromCoord(state);
        vec3 currentPos = vec3(currentKerrPos.x, currentKerrPos.z, -currentKerrPos.y);
        // --------------------------------------------------------
        // THE HORIZON TRAP
        // --------------------------------------------------------
        float currentDelta = r * r - rs * r + a_kerr * a_kerr;
        
        // If the ray crosses the horizon, OR if Delta gets dangerously 
        // close to zero (preventing math explosions), terminate the ray!
        if (r <= rPlus + 0.02 || currentDelta < 0.005)
        {
            captured = true;
            break;
        }

        // --------------------------------------------------------
        // Accretion volume
        // --------------------------------------------------------
        float newR = state.x;
        if (newR >= DISK_INNER - 0.5 && newR <= DISK_OUTER + 0.5)
        {
            if (accumulateVolume(currentPos, previousPos, uTime, accumColor, accumAlpha))
            {
                break;
            }
        }
        previousPos = currentPos;

        // Escape check
        if (newR > 25.0 && state.w > 0.0)
        {
            escaped = true;
            break;
        }
    }

    if (!captured && !escaped)
    {
        captured = true;
    }

    // ------------------------------------------------------------
    // Final color & Output
    // ------------------------------------------------------------
    vec3 finalColor;

    if (captured)
    {
        finalColor = accumColor;
    }
    else
    {
        float r = state.x;
        float theta = state.y;
        float phi = state.z;

        float phiVelocity = kerrPhiMinoDerivative(r, theta, xi);

        // Calculate velocity and map it back to OpenGL space for the skybox lookup!
        vec3 outgoingKerrVelocity = kerrCartesianVelocity(r, theta, phi, state.w, thetaVelocity, phiVelocity);
        vec3 outgoingVelocity = vec3(outgoingKerrVelocity.x, outgoingKerrVelocity.z, -outgoingKerrVelocity.y);

        vec3 outgoingDir = normalize(outgoingVelocity);

        vec3 skyColor = texture(skybox, DirectionToUV(outgoingDir)).rgb;
        finalColor = accumColor + (1.0 - accumAlpha) * skyColor;
    }

    imageStore(imgOutput, texelCoord, vec4(finalColor, 1.0));
}