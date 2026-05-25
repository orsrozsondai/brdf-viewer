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

            vertices.emplace_back(position, normal, glm::vec3(0.0f));
            indices.push_back(indices.size());
        }
    }

    computeTangents();

}

void MeshLoader::computeTangents() {
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        Vertex& v0 = vertices[indices[i + 0]];
        Vertex& v1 = vertices[indices[i + 1]];
        Vertex& v2 = vertices[indices[i + 2]];

        glm::vec3 n_lerp = glm::normalize(v0.normal + v1.normal + v2.normal);

        v0.tangent += glm::cross(
            std::abs(v0.normal.y) == 1 ? n_lerp : v0.normal,
            glm::vec3(0,1,0)
        );
        v1.tangent += glm::cross(
            std::abs(v1.normal.y) == 1  ? n_lerp : v1.normal,
            glm::vec3(0,1,0)
        );
        v2.tangent += glm::cross(
            std::abs(v2.normal.y) == 1 ? n_lerp : v2.normal,
            glm::vec3(0,1,0)
        );
    }

    for (auto& v : vertices)
        v.tangent = glm::normalize(v.tangent);
}

const std::vector<Vertex>& MeshLoader::getVertices() const {
    return vertices;
}

const std::vector<uint32_t>& MeshLoader::getIndices() const {
    return indices;
}