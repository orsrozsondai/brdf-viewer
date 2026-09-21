#version 450

layout(location = 0) in vec3 fragDir;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D hdrEquirect;

vec2 sampleSphericalMap(vec3 dir) {
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    float phi   = atan(dir.z, dir.x);
    phi = phi < 0.0 ? phi + 2.0 * 3.14159265 : phi;

    return vec2(phi / (2.0 * 3.14159265), theta / 3.14159265);
}

void main() {
    vec3 dir = normalize(fragDir);
    vec2 uv = sampleSphericalMap(dir);

    vec3 color = texture(hdrEquirect, uv).rgb;
    outColor = vec4(color, 1.0);
}