#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 worldPosition;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec3 worldTangent;


layout(set = 0, binding = 0) uniform MVP {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

void main() {
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(inPos, 1.0);
    worldPosition = (mvp.model * vec4(inPos, 1.0)).xyz;
    worldNormal = normalize(mat3(transpose(inverse(mvp.model))) * inNormal);
    worldTangent = normalize(mat3(transpose(inverse(mvp.model))) * inTangent);
    outUV = inUV;
}
