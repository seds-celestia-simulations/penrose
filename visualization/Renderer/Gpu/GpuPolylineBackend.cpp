#include "GpuPolylineBackend.h"

#include "../../IO/StarfieldBackground.h"
#include "../StarfieldGenerator.h"
#include "../HorizonProjection.h"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace viz {
namespace {

constexpr const char* kStarfieldVert = R"(#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUv;
void main() {
    vUv = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.999, 1.0);
})";

constexpr const char* kStarfieldFrag = R"(#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uStarfield;
uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform float uBrightness;
void main() {
    vec2 ndc = vUv * 2.0 - 1.0;
    vec4 world = uInvViewProj * vec4(ndc, 1.0, 1.0);
    vec3 dir = normalize(world.xyz / world.w - uCameraPos);
    float phi = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    float u = fract((phi + 3.14159265) / (2.0 * 3.14159265));
    float v = clamp(theta / 3.14159265, 0.0, 1.0);
    FragColor = vec4(texture(uStarfield, vec2(u, v)).rgb * uBrightness, 1.0);
})";

constexpr const char* kProceduralStarVert = R"(#version 330 core
layout(location = 0) in vec4 aStar;
uniform mat4 uMVP;
uniform vec3 uCameraPos;
out vec4 vColor;
void main() {
    vec3 pos = uCameraPos + aStar.xyz * 200.0;
    gl_Position = uMVP * vec4(pos, 1.0);
    float brightness = aStar.w;
    float w_val = (brightness - 0.35) / 0.65; // inverse of brightness calculation
    vColor = vec4(brightness, brightness, 0.75 + 0.25 * w_val, 1.0);
    gl_PointSize = (w_val > 0.92) ? 4.0 : 2.0;
})";

constexpr const char* kProceduralStarFrag = R"(#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    vec2 circCoord = 2.0 * gl_PointCoord - 1.0;
    if (dot(circCoord, circCoord) > 1.0) discard;
    FragColor = vColor;
})";

constexpr const char* kHorizonMaskVert = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
out vec2 vUV;
void main() {
    vUV = aCorner * 0.5 + 0.5;
    gl_Position = vec4(aCorner, 0.0, 1.0);
})";

constexpr const char* kHorizonMaskFrag = R"(#version 330 core
in vec2 vUV;
uniform vec2 uHorizonCenter;
uniform float uHorizonRadiusPx;
uniform float uHorizonRadiusWorld;
uniform float uPhotonSphereRadiusPx;
uniform int uShowPhotonSphere;
uniform int uPass; // 0 = opaque disc, 1 = glow + photon ring
uniform vec3 uCameraPos;
uniform vec3 uCameraFwd;
uniform vec3 uCameraUp;
uniform vec3 uCameraRight;
uniform float uAspect;
uniform float uFov;
uniform mat4 uMVP;
uniform vec2 uResolution;
out vec4 FragColor;

float central_horizon_clip_depth(vec3 origin, vec3 dir) {
    if (uHorizonRadiusWorld <= 0.0) return 9999.0;
    float oc_dot_d = dot(origin, dir);
    float discriminant = oc_dot_d * oc_dot_d - (dot(origin, origin) - uHorizonRadiusWorld * uHorizonRadiusWorld);
    if (discriminant < 0.0) return 9999.0;
    float sqrt_d = sqrt(discriminant);
    float t = -oc_dot_d - sqrt_d;
    if (t < 0.0) t = -oc_dot_d + sqrt_d;
    if (t < 0.0) return 9999.0;
    vec3 hit = origin + dir * t;
    vec4 clip = uMVP * vec4(hit, 1.0);
    if (abs(clip.w) <= 1e-8) return 9999.0;
    return clip.z / clip.w;
}

void main() {
    vec2 px = vUV * uResolution;
    vec2 center_px = uHorizonCenter * uResolution;
    float dist_px = length(px - center_px);
    float horizon_px = max(uHorizonRadiusPx, 1.0);

    float ndc_x = vUV.x * 2.0 - 1.0;
    float ndc_y = vUV.y * 2.0 - 1.0;
    float tan_half = tan(uFov * 0.5);
    vec3 dir = normalize(uCameraFwd + uCameraRight * (ndc_x * tan_half * uAspect) + uCameraUp * (ndc_y * tan_half));
    float horizon_depth = central_horizon_clip_depth(uCameraPos, dir);

    if (uPass == 0) {
        if (dist_px <= horizon_px && horizon_depth != 9999.0) {
            gl_FragDepth = horizon_depth * 0.5 + 0.5;
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
        discard;
    }

    // Soft atmospheric glow outside the disc.
    float glow_outer = horizon_px * 1.85;
    float glow_alpha = 0.0;
    if (dist_px > horizon_px && dist_px < glow_outer) {
        float t = (dist_px - horizon_px) / max(1.0, glow_outer - horizon_px);
        glow_alpha = 0.22 * pow(1.0 - t, 1.6);
    }

    // Thin visual photon-sphere ring (decorative, not a GR ray trace).
    float photon_alpha = 0.0;
    if (uShowPhotonSphere != 0 && uPhotonSphereRadiusPx > horizon_px) {
        float ring_half = max(2.0, horizon_px * 0.035);
        float d = abs(dist_px - uPhotonSphereRadiusPx);
        if (d < ring_half) {
            float edge = 1.0 - d / ring_half;
            photon_alpha = 0.35 * edge * edge;
        }
    }

    float alpha = max(glow_alpha, photon_alpha);
    if (alpha < 0.01) {
        discard;
    }

    // Muted warm ash — visible but not neon.
    vec3 glow_rgb = vec3(0.70, 0.58, 0.42);
    vec3 photon_rgb = vec3(0.82, 0.78, 0.70);
    vec3 color = mix(glow_rgb, photon_rgb, clamp(photon_alpha / max(alpha, 1e-4), 0.0, 1.0));
    FragColor = vec4(color, alpha);
}
)";

constexpr const char* kPostProcessVert = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
out vec2 vUV;
void main() {
    vUV = aCorner * 0.5 + 0.5;
    gl_Position = vec4(aCorner, 0.0, 1.0);
})";

constexpr const char* kPostProcessFrag = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uSceneColor;
uniform sampler2D uBloomColor;
uniform sampler2D uSceneDepth;

uniform vec2 uHorizonCenter;
uniform float uHorizonRadiusPx;
uniform float uHorizonRadiusWorld;
uniform vec3 uCameraPos;
uniform vec3 uCameraFwd;
uniform vec3 uCameraUp;
uniform vec3 uCameraRight;
uniform float uAspect;
uniform float uFov;
uniform mat4 uMVP;

uniform float uLensingStrength;
uniform float uLensingRadiusScale;
uniform float uHaloStrength;
uniform float uHaloRadiusScale;
uniform float uLensRingRadiusScale;
uniform float uLensRingWidthScale;
uniform float uLensRingStrength;
uniform float uVignette;
uniform float uContrast;
uniform float uSaturation;

out vec4 FragColor;

float central_horizon_clip_depth(vec3 origin, vec3 dir) {
    if (uHorizonRadiusWorld <= 0.0) return 9999.0;
    float oc_dot_d = dot(origin, dir);
    float discriminant = oc_dot_d * oc_dot_d - (dot(origin, origin) - uHorizonRadiusWorld * uHorizonRadiusWorld);
    if (discriminant < 0.0) return 9999.0;
    float sqrt_d = sqrt(discriminant);
    float t = -oc_dot_d - sqrt_d;
    if (t < 0.0) t = -oc_dot_d + sqrt_d;
    if (t < 0.0) return 9999.0;
    vec3 hit = origin + dir * t;
    vec4 clip = uMVP * vec4(hit, 1.0);
    if (abs(clip.w) <= 1e-8) return 9999.0;
    return clip.z / clip.w;
}

vec4 grade_pixel(vec4 c, float contrast, float saturation) {
    c.rgb = (c.rgb - 0.5) * contrast + 0.5;
    float lum = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
    c.rgb = vec3(lum) + (c.rgb - vec3(lum)) * saturation;
    return c;
}

void main() {
    vec2 resolution = vec2(textureSize(uSceneColor, 0));
    vec2 px = vUV * resolution;
    vec2 center_px = uHorizonCenter * resolution;
    float dist_px = length(px - center_px);

    float lens_px = uHorizonRadiusPx * uLensingRadiusScale;
    float halo_px = uHorizonRadiusPx * uHaloRadiusScale;
    
    vec2 sample_uv = vUV;
    
    // Step 1: Lensing Warp
    if (dist_px < lens_px && lens_px > 1e-3) {
        float t = 1.0 - dist_px / lens_px;
        float warp = uLensingStrength * t * t * t;
        vec2 duv = vUV - uHorizonCenter;
        sample_uv = uHorizonCenter + duv * (1.0 + warp);
    }
    
    vec4 c = texture(uSceneColor, sample_uv);
    vec4 b = texture(uBloomColor, vUV); // Bloom is not warped
    
    // Step 2: Bloom Composition
    c.rgb = clamp(c.rgb + b.rgb, 0.0, 1.0);
    
    // Step 3: Halo/Ring
    if (uHaloStrength > 0.0 && dist_px > uHorizonRadiusPx * 0.85 && dist_px < halo_px) {
        float t = (dist_px - uHorizonRadiusPx * 0.85) / max(1.0, halo_px - uHorizonRadiusPx * 0.85);
        float falloff = (1.0 - t) * uHaloStrength;
        c.rgb *= (1.0 - falloff);
    }
    
    if (dist_px <= uHorizonRadiusPx) {
        // Horizon masking is now handled in the base pass
    } else {
        float ring_center = uHorizonRadiusPx * uLensRingRadiusScale;
        float ring_half_width = max(0.5, uHorizonRadiusPx * uLensRingWidthScale);
        float d = abs(dist_px - ring_center);
        if (d < ring_half_width) {
            float ring_t = 1.0 - d / ring_half_width;
            float peak = ring_t * ring_t * ring_t * uLensRingStrength;
            c.r = min(c.r + peak * 1.00, 1.0);
            c.g = min(c.g + peak * 0.94, 1.0);
            c.b = min(c.b + peak * 0.72, 1.0);
        }
    }
    
    // Step 4: Vignette
    vec2 v = vUV - 0.5;
    float vig = 1.0 - uVignette * dot(v, v) * 2.2;
    c.rgb *= vig;
    
    // Step 5: Color Grading
    FragColor = grade_pixel(c, uContrast, uSaturation);
    FragColor.a = 1.0;
})";

constexpr const char* kBloomFrag = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uSceneColor;
uniform vec2 uResolution;
uniform float uBloomThreshold;
uniform float uBloomIntensity;
uniform int uBloomRadius;
out vec4 FragColor;
void main() {
    vec4 sum = vec4(0.0);
    int radius = uBloomRadius;
    float frad = float(radius);
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            float dist = length(vec2(float(dx), float(dy)));
            if (dist > frad) continue;
            
            vec2 src_uv = vUV + vec2(float(dx), float(dy)) / uResolution;
            vec4 px = texture(uSceneColor, src_uv);
            
            float lum = dot(px.rgb, vec3(0.2126, 0.7152, 0.0722));
            if (lum < uBloomThreshold) continue;
            
            float strength = (lum - uBloomThreshold) / max(1e-4, 1.0 - uBloomThreshold);
            float w = (1.0 - dist / float(radius + 1)) * strength;
            
            sum.rgb += px.rgb * w * uBloomIntensity;
        }
    }
    FragColor = sum;
})";

constexpr const char* kLineVert = R"(#version 330 core
layout(location = 0) in vec4 aPosParam;
uniform mat4 uMVP;
uniform float uMinParam;
uniform float uMaxParam;
uniform float uPlaybackTime;
uniform float uTrailRgbMin;
uniform float uTrailAlphaMin;
uniform vec3 uColor;
uniform vec3 uGradientStart;
uniform vec3 uGradientEnd;
uniform int uUseGradient;
out vec4 vColor;
out float vKeep;
void main() {
    float currentParam = clamp(uPlaybackTime, uMinParam, uMaxParam);
    float along = (currentParam > uMinParam)
        ? (aPosParam.w - uMinParam) / (currentParam - uMinParam)
        : 1.0;
    along = clamp(along, 0.0, 1.0);
    
    vec3 baseColor = uColor;
    if (uUseGradient != 0) {
        float total_along = (uMaxParam > uMinParam) ? (aPosParam.w - uMinParam) / (uMaxParam - uMinParam) : 1.0;
        baseColor = mix(uGradientStart, uGradientEnd, clamp(total_along, 0.0, 1.0));
    }
    
    float fade = along * along;
    float rgbScale = uTrailRgbMin + (1.0 - uTrailRgbMin) * fade;
    float alphaScale = uTrailAlphaMin + (1.0 - uTrailAlphaMin) * fade;

    vColor = vec4(baseColor * rgbScale, clamp(alphaScale, 0.0, 1.0));
    vKeep = (aPosParam.w <= uPlaybackTime + 1e-5) ? 1.0 : 0.0;
    gl_Position = uMVP * vec4(aPosParam.xyz, 1.0);
})";

constexpr const char* kLineGeom = R"(#version 330 core
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;
uniform vec2 uViewport;
uniform float uWidthPx;
in vec4 vColor[];
in float vKeep[];
out vec4 gColor;
flat out vec2 gP0;
flat out vec2 gP1;
void main() {
    if (vKeep[0] < 0.5 || vKeep[1] < 0.5) return;
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;
    if (p0.w <= 0.0 || p1.w <= 0.0) return;

    vec2 ndc0 = p0.xy / p0.w;
    vec2 ndc1 = p1.xy / p1.w;
    vec2 px0 = (ndc0 * 0.5 + 0.5) * uViewport;
    vec2 px1 = (ndc1 * 0.5 + 0.5) * uViewport;
    
    vec2 dir = px1 - px0;
    float len = length(dir);
    if (len < 1e-8) return;
    dir /= len;
    vec2 perp = vec2(-dir.y, dir.x);
    
    float padding = uWidthPx * 0.5 + 1.5;
    
    vec2 t0 = px0 - dir * padding + perp * padding;
    vec2 t1 = px0 - dir * padding - perp * padding;
    vec2 t2 = px1 + dir * padding + perp * padding;
    vec2 t3 = px1 + dir * padding - perp * padding;

    gl_Position = vec4((t0 / uViewport * 2.0 - 1.0) * p0.w, p0.z, p0.w);
    gColor = vColor[0]; gP0 = px0; gP1 = px1; EmitVertex();
    gl_Position = vec4((t1 / uViewport * 2.0 - 1.0) * p0.w, p0.z, p0.w);
    gColor = vColor[0]; gP0 = px0; gP1 = px1; EmitVertex();
    gl_Position = vec4((t2 / uViewport * 2.0 - 1.0) * p1.w, p1.z, p1.w);
    gColor = vColor[1]; gP0 = px0; gP1 = px1; EmitVertex();
    gl_Position = vec4((t3 / uViewport * 2.0 - 1.0) * p1.w, p1.z, p1.w);
    gColor = vColor[1]; gP0 = px0; gP1 = px1; EmitVertex();
    EndPrimitive();
})";

constexpr const char* kLineFrag = R"(#version 330 core
in vec4 gColor;
flat in vec2 gP0;
flat in vec2 gP1;
uniform float uWidthPx;
out vec4 FragColor;

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

void main() {
    float dist = sdSegment(gl_FragCoord.xy, gP0, gP1);
    float radius = uWidthPx * 0.5;
    float edge = 1.0 - dist / (radius + 0.75);
    float alpha = clamp(edge, 0.0, 1.0);
    if (alpha < 0.02) discard;
    FragColor = vec4(gColor.rgb, gColor.a * alpha);
})";

constexpr const char* kMarkerVert = R"(#version 330 core
layout(location = 0) in vec3 aCorner;
uniform vec3 uCenter;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform float uRadius;
uniform mat4 uMVP;
uniform vec4 uColor;
out vec4 vColor;
out vec2 vLocal;
void main() {
    vLocal = aCorner.xy;
    vec3 world = uCenter + uCameraRight * (aCorner.x * uRadius) + uCameraUp * (aCorner.y * uRadius);
    vColor = uColor;
    gl_Position = uMVP * vec4(world, 1.0);
})";

constexpr const char* kMarkerFrag = R"(#version 330 core
in vec4 vColor;
in vec2 vLocal;
out vec4 FragColor;
void main() {
    float r = length(vLocal);
    if (r > 1.0) discard;
    float falloff = 1.0 - r;
    float alpha = vColor.a * falloff * falloff;
    FragColor = vec4(vColor.rgb, alpha);
})";

bool outside_horizon(const Vec3& p, float rs) {
    return p.length_squared() >= rs * rs;
}

Mat4 invert_mat4(const Mat4& m) {
    const float* src = m.m.data();
    std::array<float, 16> inv{};
    inv[0] = src[5] * src[10] * src[15] - src[5] * src[11] * src[14] - src[9] * src[6] * src[15] +
             src[9] * src[7] * src[14] + src[13] * src[6] * src[11] - src[13] * src[7] * src[10];
    inv[4] = -src[4] * src[10] * src[15] + src[4] * src[11] * src[14] + src[8] * src[6] * src[15] -
             src[8] * src[7] * src[14] - src[12] * src[6] * src[11] + src[12] * src[7] * src[10];
    inv[8] = src[4] * src[9] * src[15] - src[4] * src[11] * src[13] - src[8] * src[5] * src[15] +
             src[8] * src[7] * src[13] + src[12] * src[5] * src[11] - src[12] * src[7] * src[9];
    inv[12] = -src[4] * src[9] * src[14] + src[4] * src[10] * src[13] + src[8] * src[5] * src[14] -
              src[8] * src[6] * src[13] - src[12] * src[5] * src[10] + src[12] * src[6] * src[9];
    inv[1] = -src[1] * src[10] * src[15] + src[1] * src[11] * src[14] + src[9] * src[2] * src[15] -
             src[9] * src[3] * src[14] - src[13] * src[2] * src[11] + src[13] * src[3] * src[10];
    inv[5] = src[0] * src[10] * src[15] - src[0] * src[11] * src[14] - src[8] * src[2] * src[15] +
             src[8] * src[3] * src[14] + src[12] * src[2] * src[11] - src[12] * src[3] * src[10];
    inv[9] = -src[0] * src[9] * src[15] + src[0] * src[11] * src[13] + src[8] * src[1] * src[15] -
             src[8] * src[3] * src[13] - src[12] * src[1] * src[11] + src[12] * src[3] * src[9];
    inv[13] = src[0] * src[9] * src[14] - src[0] * src[10] * src[13] - src[8] * src[1] * src[14] +
              src[8] * src[2] * src[13] + src[12] * src[1] * src[10] - src[12] * src[2] * src[9];
    inv[2] = src[1] * src[6] * src[15] - src[1] * src[7] * src[14] - src[5] * src[2] * src[15] +
             src[5] * src[3] * src[14] + src[13] * src[2] * src[7] - src[13] * src[3] * src[6];
    inv[6] = -src[0] * src[6] * src[15] + src[0] * src[7] * src[14] + src[4] * src[2] * src[15] -
             src[4] * src[3] * src[14] - src[12] * src[2] * src[7] + src[12] * src[3] * src[6];
    inv[10] = src[0] * src[5] * src[15] - src[0] * src[7] * src[13] - src[4] * src[1] * src[15] +
              src[4] * src[3] * src[13] + src[12] * src[1] * src[7] - src[12] * src[3] * src[5];
    inv[14] = -src[0] * src[5] * src[14] + src[0] * src[6] * src[13] + src[4] * src[1] * src[14] -
              src[4] * src[2] * src[13] - src[12] * src[1] * src[6] + src[12] * src[2] * src[5];
    inv[3] = -src[1] * src[6] * src[11] + src[1] * src[7] * src[10] + src[5] * src[2] * src[11] -
             src[5] * src[3] * src[10] - src[9] * src[2] * src[7] + src[9] * src[3] * src[6];
    inv[7] = src[0] * src[6] * src[11] - src[0] * src[7] * src[10] - src[4] * src[2] * src[11] +
             src[4] * src[3] * src[10] + src[8] * src[2] * src[7] - src[8] * src[3] * src[6];
    inv[11] = -src[0] * src[5] * src[11] + src[0] * src[7] * src[9] + src[4] * src[1] * src[11] -
              src[4] * src[3] * src[9] - src[8] * src[1] * src[7] + src[8] * src[3] * src[5];
    inv[15] = src[0] * src[5] * src[10] - src[0] * src[6] * src[9] - src[4] * src[1] * src[10] +
              src[4] * src[2] * src[9] + src[8] * src[1] * src[6] - src[8] * src[2] * src[5];

    const float det = src[0] * inv[0] + src[1] * inv[4] + src[2] * inv[8] + src[3] * inv[12];
    if (std::abs(det) < 1e-12f) {
        return Mat4::identity();
    }
    const float inv_det = 1.0f / det;
    Mat4 out;
    for (int i = 0; i < 16; ++i) {
        out.m[static_cast<std::size_t>(i)] = inv[static_cast<std::size_t>(i)] * inv_det;
    }
    return out;
}

} // namespace

GpuPolylineBackend::~GpuPolylineBackend() { shutdown(); }

bool GpuPolylineBackend::initialize() {
    if (!starfield_program_.link(kStarfieldVert, kStarfieldFrag) ||
        !horizon_mask_program_.link(kHorizonMaskVert, kHorizonMaskFrag) ||
        !line_program_.link(kLineVert, kLineFrag, kLineGeom) ||
        !marker_program_.link(kMarkerVert, kMarkerFrag) ||
        !procedural_star_program_.link(kProceduralStarVert, kProceduralStarFrag) ||
        !post_program_.link(kPostProcessVert, kPostProcessFrag) ||
        !bloom_program_.link(kPostProcessVert, kBloomFrag)) {
        return false;
    }

    const float quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    glGenVertexArrays(1, &fullscreen_vao_);
    glGenBuffers(1, &fullscreen_vbo_);
    glBindVertexArray(fullscreen_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreen_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    const float marker_corners[] = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, 1, 0};
    glGenVertexArrays(1, &marker_vao_);
    glGenBuffers(1, &marker_vbo_);
    glBindVertexArray(marker_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, marker_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(marker_corners), marker_corners, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
    return true;
}

void GpuPolylineBackend::shutdown() {
    destroy_trajectories();

    if (starfield_texture_ != 0) {
        glDeleteTextures(1, &starfield_texture_);
        starfield_texture_ = 0;
    }
    auto delete_buf = [](unsigned int& vao, unsigned int& vbo) {
        if (vbo != 0) {
            glDeleteBuffers(1, &vbo);
            vbo = 0;
        }
        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
    };
    delete_buf(fullscreen_vao_, fullscreen_vbo_);
    if (scene_fbo_ != 0) {
        glDeleteFramebuffers(1, &scene_fbo_);
        glDeleteTextures(1, &scene_color_tex_);
        glDeleteTextures(1, &scene_depth_tex_);
        scene_fbo_ = 0;
    }
    if (bloom_fbo_ != 0) {
        glDeleteFramebuffers(1, &bloom_fbo_);
        glDeleteTextures(1, &bloom_color_tex_);
        bloom_fbo_ = 0;
    }
    delete_buf(marker_vao_, marker_vbo_);
    delete_buf(procedural_star_vao_, procedural_star_vbo_);
    
    if (element_vbo_ != 0) {
        glDeleteBuffers(1, &element_vbo_);
        element_vbo_ = 0;
    }
}

void GpuPolylineBackend::destroy_trajectories() {
    for (GpuTrajectoryDraw& traj : trajectories_) {
        if (traj.vbo != 0) {
            glDeleteBuffers(1, &traj.vbo);
        }
        if (traj.vao != 0) {
            glDeleteVertexArrays(1, &traj.vao);
        }
    }
    trajectories_.clear();
}

void GpuPolylineBackend::upload_starfield_texture() {
    const StarfieldBackground& bg = StarfieldBackground::instance();
    if (!bg.loaded()) {
        return;
    }
    if (starfield_texture_ == 0) {
        glGenTextures(1, &starfield_texture_);
    }
    glBindTexture(GL_TEXTURE_2D, starfield_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bg.width(), bg.height(), 0, GL_RGB, GL_UNSIGNED_BYTE,
                 bg.rgb_pixels().data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GpuPolylineBackend::upload_scene(const Scene& scene) {
    destroy_trajectories();

    const SceneSettings& settings = scene.settings();
    show_starfield_ = settings.show_starfield;
    use_image_starfield_ = settings.use_image_starfield;
    starfield_brightness_ = settings.starfield_brightness;
    show_event_horizon_ = settings.show_event_horizon;
    show_photon_sphere_ = settings.show_photon_sphere;
    background_ = settings.background;
    horizon_radius_ = settings.horizon_radius;
    photon_sphere_radius_ = settings.horizon_radius * 1.5f;

    trajectories_.reserve(scene.trajectories().size());
    for (const Trajectory& traj : scene.trajectories()) {
        if (traj.samples().empty()) {
            continue;
        }

        std::vector<float> verts;
        verts.reserve(traj.samples().size() * 4);
        for (const TrajectorySample& sample : traj.samples()) {
            verts.push_back(sample.position.x);
            verts.push_back(sample.position.y);
            verts.push_back(sample.position.z);
            verts.push_back(static_cast<float>(sample.parameter));
        }

        GpuTrajectoryDraw draw;
        draw.vertex_count = traj.samples().size();
        draw.style = traj.style();
        draw.min_parameter = traj.min_parameter();
        draw.max_parameter = traj.max_parameter();

        glGenVertexArrays(1, &draw.vao);
        glGenBuffers(1, &draw.vbo);
        glBindVertexArray(draw.vao);
        glBindBuffer(GL_ARRAY_BUFFER, draw.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                     verts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glBindVertexArray(0);

        trajectories_.push_back(draw);
    }

    upload_starfield_texture();
}

void GpuPolylineBackend::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == width_ && height == height_ && scene_fbo_ != 0) return;

    width_ = width;
    height_ = height;

    if (scene_fbo_ == 0) glGenFramebuffers(1, &scene_fbo_);
    if (scene_color_tex_ == 0) glGenTextures(1, &scene_color_tex_);
    if (scene_depth_tex_ == 0) glGenTextures(1, &scene_depth_tex_);

    glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo_);
    glBindTexture(GL_TEXTURE_2D, scene_color_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_color_tex_, 0);

    glBindTexture(GL_TEXTURE_2D, scene_depth_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, scene_depth_tex_, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // Handle error
    }

    if (bloom_fbo_ == 0) glGenFramebuffers(1, &bloom_fbo_);
    if (bloom_color_tex_ == 0) glGenTextures(1, &bloom_color_tex_);

    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo_);
    glBindTexture(GL_TEXTURE_2D, bloom_color_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_color_tex_, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GpuPolylineBackend::draw_starfield(const Camera& camera, float aspect, float brightness,
                                        const PresentationProfile& profile, const RenderOptions& options) {
    if (!show_starfield_) {
        return;
    }

    const Mat4 vp = camera.view_projection(aspect);
    const Vec3 eye = camera.position();

    glDisable(GL_DEPTH_TEST);

    if (use_image_starfield_ && starfield_texture_ != 0) {
        const Mat4 inv_vp = invert_mat4(vp);
        starfield_program_.use();
        starfield_program_.set_mat4("uInvViewProj", inv_vp.m.data());
        starfield_program_.set_vec3("uCameraPos", eye.x, eye.y, eye.z);
        starfield_program_.set_float("uBrightness", brightness);
        starfield_program_.set_int("uStarfield", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, starfield_texture_);
        glBindVertexArray(fullscreen_vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    } else {
        if (options.starfield_seed != last_star_seed_ || procedural_star_vao_ == 0) {
            std::vector<Star> stars = StarfieldGenerator::generate_stars(options.starfield_seed, 900);
            std::vector<float> data;
            data.reserve(stars.size() * 4);
            for (const Star& s : stars) {
                data.push_back(s.direction.x);
                data.push_back(s.direction.y);
                data.push_back(s.direction.z);
                data.push_back(s.brightness); // Pass brightness in w
            }

            if (procedural_star_vao_ == 0) {
                glGenVertexArrays(1, &procedural_star_vao_);
                glGenBuffers(1, &procedural_star_vbo_);
            }
            glBindVertexArray(procedural_star_vao_);
            glBindBuffer(GL_ARRAY_BUFFER, procedural_star_vbo_);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glBindVertexArray(0);
            last_star_seed_ = options.starfield_seed;
        }

        glEnable(GL_PROGRAM_POINT_SIZE);
        procedural_star_program_.use();
        procedural_star_program_.set_mat4("uMVP", vp.m.data());
        procedural_star_program_.set_vec3("uCameraPos", eye.x, eye.y, eye.z);
        glBindVertexArray(procedural_star_vao_);
        glDrawArrays(GL_POINTS, 0, 900);
        glBindVertexArray(0);
        glDisable(GL_PROGRAM_POINT_SIZE);
    }
}

void GpuPolylineBackend::draw_trajectories(const Scene& scene, const Camera& camera, float aspect,
                                           const RenderOptions& options) {
    const Mat4 mvp = camera.view_projection(aspect);
    const double playback_time = scene.playback().time;
    const float rs = horizon_radius_;
    const int selected = scene.playback().selected_trajectory;
    const Vec3 eye = camera.position();
    const float ref_dist = std::max(eye.length(), 1.0f);

    const Vec3 cam_fwd = camera.forward();
    const Vec3 cam_up = camera.up();
    const Vec3 cam_right = cam_up.cross(cam_fwd).normalized();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    for (std::size_t t = 0; t < trajectories_.size() && t < scene.trajectories().size(); ++t) {
        const Trajectory& traj = scene.trajectories()[t];
        const GpuTrajectoryDraw& gpu = trajectories_[t];
        if (gpu.vertex_count < 2) {
            continue;
        }

        const std::size_t end_idx = traj.index_at_parameter(playback_time);
        const TrajectoryStyle& style = gpu.style;
        const bool is_selected = static_cast<int>(t) == selected;

        if (style.show_trail && end_idx > 0) {
            // Solid trails (CPU-like): high floor alpha, thick screen-space ribbons.
            const float rgb_min = style.trail_rgb_min;
            const float alpha_min = style.trail_alpha_min;
            const float width_px = style.line_width;

            line_program_.use();
            line_program_.set_mat4("uMVP", mvp.m.data());
            line_program_.set_float("uMinParam", static_cast<float>(gpu.min_parameter));
            line_program_.set_float("uMaxParam", static_cast<float>(gpu.max_parameter));
            line_program_.set_float("uPlaybackTime", static_cast<float>(playback_time));
            line_program_.set_float("uTrailRgbMin", rgb_min);
            line_program_.set_float("uTrailAlphaMin", alpha_min);
            line_program_.set_vec3("uColor", style.color.r / 255.0f, style.color.g / 255.0f,
                                   style.color.b / 255.0f);
            
            if (style.gradient_along_path) {
                line_program_.set_int("uUseGradient", 1);
                line_program_.set_vec3("uGradientStart", style.gradient_start.r / 255.0f,
                                       style.gradient_start.g / 255.0f, style.gradient_start.b / 255.0f);
                line_program_.set_vec3("uGradientEnd", style.gradient_end.r / 255.0f,
                                       style.gradient_end.g / 255.0f, style.gradient_end.b / 255.0f);
            } else {
                line_program_.set_int("uUseGradient", 0);
            }

            line_program_.set_vec2("uViewport", static_cast<float>(width_),
                                   static_cast<float>(height_));
            line_program_.set_float("uWidthPx", width_px);

            glBindVertexArray(gpu.vao);

            const std::size_t max_dense = std::max<std::size_t>(1, options.trail_max_dense_segments);
            const std::size_t stride = end_idx > max_dense ? (end_idx / max_dense + 1) : 1;
            const std::size_t tail = options.trail_full_detail_tail;
            const std::size_t full_detail_from = end_idx > tail ? end_idx - tail : 0;

            std::vector<unsigned int> indices;
            indices.reserve(end_idx + 1);
            
            bool in_run = false;
            auto flush = [&]() {
                if (indices.size() > 1) {
                    if (element_vbo_ == 0) {
                        glGenBuffers(1, &element_vbo_);
                    }
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_vbo_);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                                 indices.data(), GL_STREAM_DRAW);
                    glDrawElements(GL_LINE_STRIP, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
                }
                indices.clear();
            };

            for (std::size_t i = 0; i <= end_idx; ++i) {
                if (i > 0 && stride > 1 && i < full_detail_from && (i % stride) != 0) {
                    continue;
                }
                const bool ok = outside_horizon(traj.samples()[i].position, rs);
                if (ok) {
                    indices.push_back(static_cast<unsigned int>(i));
                    in_run = true;
                } else if (in_run) {
                    flush();
                    in_run = false;
                }
            }
            if (in_run) {
                flush();
            }
            
            glBindVertexArray(0);
        }

        if (style.show_marker) {
            const Vec3 marker_pos = traj.samples()[end_idx].position;
            if (!outside_horizon(marker_pos, rs)) {
                continue;
            }

            const float radius = style.marker_radius * style.marker_glow_scale;
            const float br = style.marker_brightness;

            Color4 marker_base = style.color;
            if (style.gradient_along_path && gpu.vertex_count > 1) {
                const double span = gpu.max_parameter - gpu.min_parameter;
                const double param = traj.samples()[end_idx].parameter;
                const float gradient_t = span > 1e-12 ? static_cast<float>((param - gpu.min_parameter) / span) : 1.0f;
                const auto lerp_channel = [&](std::uint8_t x, std::uint8_t y) {
                    return (static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * gradient_t) / 255.0f;
                };
                marker_base = Color4::from_float(lerp_channel(style.gradient_start.r, style.gradient_end.r),
                                                 lerp_channel(style.gradient_start.g, style.gradient_end.g),
                                                 lerp_channel(style.gradient_start.b, style.gradient_end.b),
                                                 style.glow_color.a / 255.0f);
            }

            marker_program_.use();
            marker_program_.set_mat4("uMVP", mvp.m.data());
            marker_program_.set_vec3("uCenter", marker_pos.x, marker_pos.y, marker_pos.z);
            marker_program_.set_vec3("uCameraRight", cam_right.x, cam_right.y, cam_right.z);
            marker_program_.set_vec3("uCameraUp", cam_up.x, cam_up.y, cam_up.z);
            marker_program_.set_float("uRadius", radius);
            marker_program_.set_vec4("uColor", marker_base.r / 255.0f * br,
                                     marker_base.g / 255.0f * br, marker_base.b / 255.0f * br, style.glow_color.a / 255.0f);
            glBindVertexArray(marker_vao_);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void GpuPolylineBackend::render(const Scene& scene, const Camera& camera,
                                const RenderOptions& render_options,
                                const PresentationProfile& profile) {
    const float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    if (width_ <= 0 || height_ <= 0 || scene_fbo_ == 0) return;

    // Render Scene to Base FBO
    glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo_);
    glViewport(0, 0, width_, height_);

    glClearColor(background_.r / 255.0f, background_.g / 255.0f, background_.b / 255.0f,
                 background_.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (show_starfield_) {
        const float brightness = use_image_starfield_ ? starfield_brightness_ : 1.0f;
        draw_starfield(camera, aspect, brightness, profile, render_options);
    }

    if (show_event_horizon_ && horizon_radius_ > 0.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const HorizonScreen horizon = project_central_horizon(camera, horizon_radius_, width_, height_);
        const HorizonScreen photon =
            project_central_horizon(camera, photon_sphere_radius_, width_, height_);

        auto set_horizon_uniforms = [&]() {
            horizon_mask_program_.use();
            horizon_mask_program_.set_vec2("uHorizonCenter", horizon.center_u, 1.0f - horizon.center_v);
            horizon_mask_program_.set_float("uHorizonRadiusPx", horizon.radius_px);
            horizon_mask_program_.set_float("uHorizonRadiusWorld", horizon_radius_);
            horizon_mask_program_.set_float("uPhotonSphereRadiusPx", photon.radius_px);
            horizon_mask_program_.set_int("uShowPhotonSphere", show_photon_sphere_ ? 1 : 0);
            horizon_mask_program_.set_vec3("uCameraPos", camera.position().x, camera.position().y,
                                           camera.position().z);
            horizon_mask_program_.set_vec3("uCameraFwd", camera.forward().x, camera.forward().y,
                                           camera.forward().z);
            horizon_mask_program_.set_vec3("uCameraUp", camera.up().x, camera.up().y, camera.up().z);
            const Vec3 right = camera.up().cross(camera.forward()).normalized();
            horizon_mask_program_.set_vec3("uCameraRight", right.x, right.y, right.z);
            horizon_mask_program_.set_float("uAspect", aspect);
            horizon_mask_program_.set_float("uFov", camera.fov_y());
            const Mat4 mvp = camera.view_projection(aspect);
            horizon_mask_program_.set_mat4("uMVP", mvp.m.data());
            horizon_mask_program_.set_vec2("uResolution", static_cast<float>(width_),
                                           static_cast<float>(height_));
        };

        // Pass 0: opaque filled disc with depth write.
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        set_horizon_uniforms();
        horizon_mask_program_.set_int("uPass", 0);
        glBindVertexArray(fullscreen_vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Pass 1: glow + photon ring without depth test so FX are actually visible.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        set_horizon_uniforms();
        horizon_mask_program_.set_int("uPass", 1);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }

    // Trajectories and Markers
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // Don't write depth, prevents self z-fighting
    glDepthFunc(GL_LEQUAL);
    
    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);

    draw_trajectories(scene, camera, aspect, render_options);
    
    glBlendEquation(GL_FUNC_ADD);

    // Bloom Pass
    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo_);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    bloom_program_.use();
    bloom_program_.set_int("uSceneColor", 0);
    bloom_program_.set_vec2("uResolution", static_cast<float>(width_), static_cast<float>(height_));
    bloom_program_.set_float("uBloomThreshold", profile.bloom_threshold);
    bloom_program_.set_float("uBloomIntensity", profile.bloom_intensity);
    bloom_program_.set_int("uBloomRadius", profile.bloom_radius);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_color_tex_);
    
    glBindVertexArray(fullscreen_vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // Post-Process Pass (Base composition)
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Default framebuffer
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);

    const HorizonScreen horizon = project_central_horizon(camera, horizon_radius_, width_, height_);

    post_program_.use();
    post_program_.set_int("uSceneColor", 0);
    post_program_.set_int("uBloomColor", 1);
    post_program_.set_int("uSceneDepth", 2);

    post_program_.set_vec2("uHorizonCenter", horizon.center_u, 1.0f - horizon.center_v);
    post_program_.set_float("uHorizonRadiusPx", horizon.radius_px);
    post_program_.set_float("uHorizonRadiusWorld", horizon_radius_);
    
    post_program_.set_vec3("uCameraPos", camera.position().x, camera.position().y, camera.position().z);
    post_program_.set_vec3("uCameraFwd", camera.forward().x, camera.forward().y, camera.forward().z);
    post_program_.set_vec3("uCameraUp", camera.up().x, camera.up().y, camera.up().z);
    
    const Vec3 right = camera.up().cross(camera.forward()).normalized();
    post_program_.set_vec3("uCameraRight", right.x, right.y, right.z);
    post_program_.set_float("uAspect", aspect);
    post_program_.set_float("uFov", camera.fov_y());
    
    const Mat4 mvp = camera.view_projection(aspect);
    post_program_.set_mat4("uMVP", mvp.m.data());

    post_program_.set_float("uLensingStrength", profile.enabled ? profile.lensing_strength : 0.0f);
    post_program_.set_float("uLensingRadiusScale", profile.lensing_radius_scale);
    post_program_.set_float("uHaloStrength", profile.enabled ? profile.halo_strength : 0.0f);
    post_program_.set_float("uHaloRadiusScale", profile.halo_radius_scale);
    post_program_.set_float("uLensRingRadiusScale", profile.lens_ring_radius_scale);
    post_program_.set_float("uLensRingWidthScale", profile.lens_ring_width_scale);
    post_program_.set_float("uLensRingStrength", profile.enabled ? profile.lens_ring_strength : 0.0f);
    post_program_.set_float("uVignette", profile.enabled ? profile.vignette : 0.0f);
    post_program_.set_float("uContrast", profile.enabled ? profile.contrast : 1.0f);
    post_program_.set_float("uSaturation", profile.enabled ? profile.saturation : 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_color_tex_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloom_color_tex_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, scene_depth_tex_);

    glBindVertexArray(fullscreen_vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

} // namespace viz
