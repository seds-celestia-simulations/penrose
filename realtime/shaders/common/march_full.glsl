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

    float r0 = length(ro);
    float theta0 = acos(clamp(ro.z / r0, -1.0, 1.0));
    float phi0 = atan(ro.y, ro.x);

    float sinTheta0 = sin(theta0);
    float cosTheta0 = cos(theta0);
    float sinPhi0 = sin(phi0);
    float cosPhi0 = cos(phi0);

    vec3 eR = normalize(ro);
    vec3 eTheta = vec3(cosTheta0*cosPhi0, cosTheta0*sinPhi0, -sinTheta0);
    vec3 ePhi = vec3(-sinPhi0, cosPhi0, 0.0);

    float rdot = dot(rd, eR);
    float thetadot = dot(rd, eTheta) / r0;
    float phidot = dot(rd, ePhi) / (r0 * max(sinTheta0, 1e-6));

    float tdot = computeTdot(r0, theta0, rdot, thetadot, phidot);

    vec3 accumColor = vec3(0.0);
    float accumAlpha = 0.0;
    bool captured = false;
    bool escaped = false;

    vec4 pos = vec4(0.0, r0, theta0, phi0);
    vec4 vel = vec4(tdot, rdot, thetadot, phidot);

    for (int i = 0; i < 150; ++i) {
        float r = pos.y;

        float max_dr = 0.15;
        if (r > DISK_OUTER + 1.0) max_dr = 1.0;
        else if (r < DISK_INNER) max_dr = 0.05;

        float h = max(r * 0.03, 0.003);
        if (abs(vel.y) > 1e-6) {
            h = min(h, max_dr / abs(vel.y));
        }

        if (vel.y < 0.0) {
            h = min(h, (r - rs) * 0.5);
        }

        float jitter = 1.0 + (hash(vec3(float(texelCoord.x), float(texelCoord.y), float(i))) * 2.0 - 1.0) * 0.2;
        h *= jitter;

        vec3 previousPos = cartesianFromCoord(pos);

        geodesicRK4(pos, vel, h);

        vec3 currentPos = cartesianFromCoord(pos);
        float newR = pos.y;

        intersectParticles(previousPos, currentPos, accumColor, accumAlpha);
        if (accumulateVolume(currentPos, previousPos, uTime, accumColor, accumAlpha)) break;

        if (newR <= 1.5 * rs) {
            captured = true;
            break;
        }

        if (newR > 25.0 && vel.y > 0.0) {
            escaped = true;
            break;
        }
    }

    if (!captured && !escaped) captured = true;

    vec3 finalColor;

    if (captured) {
        finalColor = accumColor;
    } else {
        float r = pos.y;
        float theta = pos.z;
        float phi = pos.w;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);

        float xdot = sinTheta*cosPhi*vel.y + r*cosTheta*cosPhi*vel.z - r*sinTheta*sinPhi*vel.w;
        float ydot = sinTheta*sinPhi*vel.y + r*cosTheta*sinPhi*vel.z + r*sinTheta*cosPhi*vel.w;
        float zdot = cosTheta*vel.y - r*sinTheta*vel.z;

        vec3 outgoingDir = normalize(vec3(xdot, ydot, zdot));
        vec3 skyColor = texture(skybox, DirectionToUV(outgoingDir)).rgb;
        finalColor = accumColor + (1.0 - accumAlpha) * skyColor;
    }

    imageStore(imgOutput, texelCoord, vec4(finalColor, 1.0));
}
