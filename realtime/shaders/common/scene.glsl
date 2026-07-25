#include "skybox.glsl"
#include "disk.glsl"

struct Particle {
    vec4 stateX;
    vec4 stateU;
    vec3 color;
    float mass;
    float radius;
    float pad0, pad1, pad2;
};

layout(std430, binding = 2) readonly buffer ParticleBuffer {
    Particle particles[];
};

void intersectParticles(vec3 p0, vec3 p1, inout vec3 accumColor, inout float accumAlpha) {
    vec3 d = p1 - p0;
    for (int p = 0; p < particles.length(); ++p) {
        vec3 center = particles[p].stateX.xyz;
        float radius = particles[p].radius;
        vec3 f = p0 - center;
        
        float a = dot(d, d);
        float b = 2.0 * dot(f, d);
        float c = dot(f, f) - radius * radius;
        
        float discriminant = b * b - 4.0 * a * c;
        if (discriminant > 0.0) {
            discriminant = sqrt(discriminant);
            float t1 = (-b - discriminant) / (2.0 * a);
            float t2 = (-b + discriminant) / (2.0 * a);
            
            if ((t1 >= 0.0 && t1 <= 1.0) || (t2 >= 0.0 && t2 <= 1.0)) {
                float pAlpha = 0.95;
                accumColor += (1.0 - accumAlpha) * particles[p].color * pAlpha;
                accumAlpha += (1.0 - accumAlpha) * pAlpha;
            }
        }
    }
}
