#include "MeshLoader.hpp"
#include <cstddef>
#include "Vertex.hpp"
#include "tiny_obj_loader.h"
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

MeshLoader::MeshLoader(const std::string& path, const std::string& name) : path(path) {
    this->name = name.empty()?path:name;
    loadOBJ();
    std::cout << "Object loaded: " << path << std::endl;
}
MeshLoader::MeshLoader() {

}

MeshLoader::MeshLoader(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& name)
    : vertices(vertices),
      indices(indices),
      name(name) {

}

void MeshLoader::loadOBJ() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string err;

    bool success = tinyobj::LoadObj(
        &attrib,
        &shapes,
        nullptr,
        &err,
        path.c_str(),
        nullptr,
        true
    );

    if (err.ends_with('\n')) err.pop_back();

    if (!success)
        throw std::runtime_error(err);

    vertices.clear();
    indices.clear();

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            glm::vec3 position{
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            glm::vec3 normal{0.0f};

            if (index.normal_index >= 0)
            {
                normal = glm::vec3(
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                );
            }
            glm::vec2 uv{0.0f};

            if (index.texcoord_index >= 0)
            {
                uv = glm::vec2{
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            vertices.emplace_back(position, normal, uv, glm::vec3(0.0f));
            indices.push_back(indices.size());
        }
    }

    computeTangents();

}

void MeshLoader::computeTangents()
{
    for (auto& v : vertices)
        v.tangent = glm::vec3(0.0f);

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        Vertex& v0 = vertices[indices[i + 0]];
        Vertex& v1 = vertices[indices[i + 1]];
        Vertex& v2 = vertices[indices[i + 2]];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;

        glm::vec2 deltaUV1 = v1.uv - v0.uv;
        glm::vec2 deltaUV2 = v2.uv - v0.uv;

        float determinant =
            deltaUV1.x * deltaUV2.y -
            deltaUV2.x * deltaUV1.y;

        if (std::abs(determinant) < 1e-6f)
            continue;

        float f = 1.0f / determinant;

        glm::vec3 tangent;

        tangent.x = f * (
            deltaUV2.y * edge1.x -
            deltaUV1.y * edge2.x
        );

        tangent.y = f * (
            deltaUV2.y * edge1.y -
            deltaUV1.y * edge2.y
        );

        tangent.z = f * (
            deltaUV2.y * edge1.z -
            deltaUV1.y * edge2.z
        );

        v0.tangent += tangent;
        v1.tangent += tangent;
        v2.tangent += tangent;
    }

    for (auto& v : vertices)
    {
        glm::vec3 n = glm::normalize(v.normal);

        v.tangent = glm::normalize(
            v.tangent - n * glm::dot(n, v.tangent)
        );
    }
}
const std::vector<Vertex>& MeshLoader::getVertices() const {
    return vertices;
}

const std::vector<uint32_t>& MeshLoader::getIndices() const {
    return indices;
}