#version 430 core
out vec4 FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D computeResult;
uniform vec2 uTexSize;

void main() {
    // 1. Sample the bilinearly interpolated base color
    vec3 color = texture(computeResult, TexCoords).rgb;
    
    // 2. Calculate the size of a single low-res pixel
    vec2 texel = 1.0 / uTexSize;
    
    // 3. Sample the North, South, East, and West neighbors
    vec3 n = texture(computeResult, TexCoords + vec2(0.0, texel.y)).rgb;
    vec3 s = texture(computeResult, TexCoords + vec2(0.0, -texel.y)).rgb;
    vec3 e = texture(computeResult, TexCoords + vec2(texel.x, 0.0)).rgb;
    vec3 w = texture(computeResult, TexCoords + vec2(-texel.x, 0.0)).rgb;
    
    // 4. Contrast Adaptive Sharpening (Unsharp Masking style)
    float sharpness = 0.4; // Tweak this! 0.0 is blurry, 1.0 is over-sharpened
    
    vec3 sharpened = color + (color * 4.0 - n - s - e - w) * sharpness;
    
    // Clamp to prevent negative ringing artifacts
    FragColor = vec4(max(sharpened, 0.0), 1.0); 
}