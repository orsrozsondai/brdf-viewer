#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <glm/glm.hpp>

#include "Vertex.hpp"

class MeshLoader {
public:

    MeshLoader(const std::filesystem::path& path, const std::string& name = "");
    MeshLoader(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& name);
    MeshLoader();

    const std::vector<Vertex>& getVertices() const;
    const std::vector<uint32_t>& getIndices() const;
    const std::string& getName() const {return name;}

private:
    void loadOBJ();
    void computeTangents();

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::filesystem::path path;
    std::string name;
};