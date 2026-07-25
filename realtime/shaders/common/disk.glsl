#include "noise.glsl"
layout(binding = 2) uniform sampler3D uNoise3D;

const float DISK_INNER = 0.75;
const float DISK_OUTER = 5.0;
const float DISK_HEIGHT_INNER = 0.5;
const float DISK_HEIGHT_OUTER = 2.0;

const float DENSITY_SCALE = 1.0;
const float DOPPLER_STRENGTH = 4.0;

vec3 blackbodyColor(float T) {
    vec3 cool = vec3(0.3, 0.01, 0.0);
    vec3 warm = vec3(1.0, 0.25, 0.02);
    vec3 hot  = vec3(1.0, 0.85, 0.55);
    vec3 veryHot = vec3(0.7, 0.75, 1.0);

    if (T < 0.25) return mix(cool, warm, T / 0.25);
    else if (T < 0.5) return mix(warm, hot, (T - 0.25) / 0.25);
    else if (T < 0.75) return mix(hot, veryHot, (T - 0.5) / 0.25);
    else return veryHot;
}

bool accumulateVolume(vec3 currentPos, vec3 previousPos, float uTime,
                      inout vec3 accumColor, inout float accumAlpha) {

    float diskR = length(currentPos.xy);
    float zDist = abs(currentPos.z);

    float radial01 = (diskR - DISK_INNER) / (DISK_OUTER - DISK_INNER);
    float localHeight = mix(DISK_HEIGHT_INNER, DISK_HEIGHT_OUTER, pow(radial01, 0.8));

    if (diskR > DISK_INNER && diskR < DISK_OUTER && zDist < localHeight * 2.5) {

        float phi = atan(currentPos.y, currentPos.x);
        const float TWO_PI = 6.28318530718;

        float omega = 3.0 / pow(diskR, 1.5);
        float rotation = mod(omega * uTime, TWO_PI);
        float shearedPhi = phi + rotation;

        float spiralPhase = phi - log(diskR + 1.0) * 2.5 + mod(uTime * 0.15, TWO_PI);
        float spiralWeight = 0.25 * max(1.0 - radial01, 0.0);
        float spiralOffset = spiralWeight * sin(spiralPhase);

        vec3 noiseCoord = vec3(diskR * cos(shearedPhi), diskR * sin(shearedPhi), currentPos.z * 1.5);
        vec3 texCoord = noiseCoord * 0.15;

        float noiseVal = texture(uNoise3D, texCoord).r;

        float edgeFade = smoothstep(0.0, 0.15, radial01) * (1.0 - smoothstep(0.6, 1.0, radial01));
        float verticalFade = exp(-2.0 * zDist * zDist / (localHeight * localHeight));

        float density = smoothstep(0.2, 0.8, noiseVal) * edgeFade * verticalFade * DENSITY_SCALE;

        if (density > 0.0) {
            vec3 plasmaVel = normalize(vec3(currentPos.y, -currentPos.x, 0.0));

            vec3 delta = currentPos - previousPos;
            vec3 rayDir = normalize(delta + vec3(1e-8));
            float beaming = dot(plasmaVel, rayDir);

            float beta = sqrt(rs / (2.0 * diskR));
            float gamma_lorentz = 1.0 / sqrt(max(1.0 - beta * beta, 1e-6));
            float dopplerFactor = 1.0 / (gamma_lorentz * (1.0 - beta * beaming));
            dopplerFactor = clamp(mix(1.0, dopplerFactor, DOPPLER_STRENGTH), 0.3, 3.0);

            float stepSize = length(currentPos - previousPos);
            float tempNormalized = pow(DISK_INNER / max(diskR, 1e-6), 0.75);
            float alpha = clamp(density * stepSize * (1.0 + 2.0 * tempNormalized), 0.0, 1.0);

            vec3 color = blackbodyColor(tempNormalized);

            float gravRedshift = sqrt(max(1.0 - rs / diskR, 0.01));
            vec3 redShifted = vec3(color.r, color.r * 0.6, color.r * 0.3);
            color = mix(color, redShifted, 1.0 - gravRedshift);
            color *= gravRedshift * 1.5;

            color *= dopplerFactor;

            float glow = exp(-diskR * 3.0) * 0.4;
            vec3 coronaColor = vec3(0.3, 0.4, 0.8) * glow;
            color += coronaColor;

            accumColor += (1.0 - accumAlpha) * color * alpha;
            accumAlpha += (1.0 - accumAlpha) * alpha;

            if (accumAlpha > 0.99) return true;
        }
    }
    return false;
}