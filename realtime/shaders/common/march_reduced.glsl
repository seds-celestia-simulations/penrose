void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    if (texelCoord.x >= int(uResolution.x) || texelCoord.y >= int(uResolution.y)) {
        return;
    }

    vec2 TexCoords = (vec2(texelCoord) + 0.5) / uResolution;

    vec4 ndcPos = vec4(TexCoords.x * 2.0 - 1.0, TexCoords.y * 2.0 - 1.0, 1.0, 1.0);
    vec4 worldPos = uInvProjView * ndcPos;
    vec3 rd = normalize((worldPos.xyz / worldPos.w) - uCameraPos);
    vec3 ro = uCameraPos;
if (abs(ro.y) < 0.001) {
    ro.y = 0.001; 
}
    float r0 = length(ro);
    vec3 planeNormal = cross(ro, rd);
    float planeLen = length(planeNormal);

    vec3 accumColor = vec3(0.0);
    float accumAlpha = 0.0;
    bool captured = false;
    bool escaped = false;

    float u = 1.0 / r0;
    float v = 0.0;
    float psi = 0.0;
    vec3 eR = ro / r0;
    vec3 eT = vec3(1.0, 0.0, 0.0);

    // Extremely radial rays use world-space linear stepping to avoid RK4 coordinate singularity
    if (planeLen < 0.001) {
        float dt = max(r0 * 0.01, 0.003);
        float t = 0.0;
        vec3 prevPos = ro;

        for (int i = 0; i < 200; ++i) {
            t += dt;
            vec3 pos = ro + t * rd;
            float r = length(pos);

            if (r < 1.5 * rs) { captured = true; break; }
            if (r > 25.0)     { escaped  = true; break; }

            intersectParticles(prevPos, pos, accumColor, accumAlpha);
            if (accumulateVolume(pos, prevPos, uTime, accumColor, accumAlpha)) break;

            prevPos = pos;
            // Fast outer stepping to stay within budget, fine in disk for density accuracy.
            if (r > DISK_OUTER + 1.0)  dt = max(r * 0.08, 0.2);
            else if (r > DISK_INNER)   dt = max(r * 0.03, 0.003);
            else                        dt = max(r * 0.015, 0.003);
        }
        // All rays in this branch have planeLen < event horizon: geodesic always captures.
        if (!captured && !escaped) captured = true;
    } else {
        planeNormal /= planeLen;
        eR = ro / r0;
        eT = normalize(cross(planeNormal, eR));

        float radialSpeed = dot(rd, eR);
        float tangentialSpeed = dot(rd, eT);

        u = 1.0 / r0;
        v = -radialSpeed / (r0 * tangentialSpeed);

        float stepSign = sign(tangentialSpeed);
        psi = 0.0;
        vec3 previousPos = orbitPosition(r0, psi, eR, eT);

        // 1. Calculate the emission angle for the LUT lookup
        float gamma = acos(clamp(dot(rd, eR), -1.0, 1.0));
        float lutU = (r0 - uLutRMin) / (uLutRMax - uLutRMin);
        float lutV = (gamma - 0.001) / (3.14159265 - 0.002);

        // 2. Sample the Predictive LUT
        vec2 bakedData = texture(uGeodesicLUT, vec2(lutU, lutV)).rg;
        float deltaPhi = bakedData.r;
        float minRadius = bakedData.g;

        bool fastExit = (minRadius > DISK_OUTER + 2.0 && r0 > DISK_OUTER + 2.0);

        if (r0 > DISK_OUTER + 1.0 && minRadius > DISK_OUTER + 1.0) {
            escaped = true;
            u = 0.0; // Force it out
        }
    else {
        for (int i = 0; i < 150; ++i) {
            // Force a fixed step scaling if we are in fast-exit mode to clear the loop instantly
            float current_r = 1.0 / max(u, 1e-6);
            
            // Branchless step sizing
            float stepScale = (current_r > DISK_OUTER) ? 0.2 : 0.04;
            float max_dPsi = stepScale * u * u / max(abs(v), 1e-6);
            float dPsi = min(stepScale, max_dPsi) * stepSign;
            
            orbitRK4(u, v, dPsi);
            psi += dPsi;

            float r = 1.0 / max(u, 1e-6);
            
            // Unified exit flags evaluated via selection instead of chaotic breaks
            if (u <= 0.0) { escaped = true; u = max(u, 1e-6); break; }
            if (r <= 1.5 * rs) { captured = true; break; }
            
            vec3 currentPos = orbitPosition(r, psi, eR, eT);
            // Only evaluate volume if we are actually inside the disk bounds to save texture bandwidth
            if (r >= DISK_INNER - 0.5 && r <= DISK_OUTER + 0.5) {
                //intersectParticles(previousPos, currentPos, accumColor, accumAlpha);
                if (accumulateVolume(currentPos, previousPos, uTime, accumColor, accumAlpha)) {
                    break;
                }
            }
            
                previousPos = currentPos;

            if (r > 25.0) {
                escaped = true;
                break;
            }
        }
    }
        
        // Rays that orbit the photon sphere many times may run out of iterations.
        // If they haven't escaped after 150 steps, they are virtually captured.
        if (!captured && !escaped) {
            captured = true;
        }
    }

    vec3 finalColor;

    if (captured) {
        finalColor = accumColor;
    } else {
        if (planeLen < 0.001) {
            vec3 skyColor = texture(skybox, DirectionToUV(rd)).rgb;
            finalColor = accumColor + (1.0 - accumAlpha) * skyColor;
        } else {
            float r = 1.0 / u;
            float drdPsi = -v / max(u * u, 1e-8);
            vec3 er = cos(psi) * eR + sin(psi) * eT;
            vec3 epsi = -sin(psi) * eR + cos(psi) * eT;
            vec3 outgoingDir = normalize(drdPsi * er + r * epsi);

            vec3 skyColor = texture(skybox, DirectionToUV(outgoingDir)).rgb;
            finalColor = accumColor + (1.0 - accumAlpha) * skyColor;
        }
    }

    imageStore(imgOutput, texelCoord, vec4(finalColor, 1.0));
}
