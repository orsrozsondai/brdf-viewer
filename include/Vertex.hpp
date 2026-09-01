#pragma once

#include <glm/glm.hpp>
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;
    Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv, const glm::vec3& tangent) : position(position), normal(normal), uv(uv), tangent(tangent) {}
};