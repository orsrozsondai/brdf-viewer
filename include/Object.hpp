#pragma once

#include "Camera.hpp"
#include "MeshLoader.hpp"
#include "Pipeline.hpp"
#include "RenderContext.hpp"
#include "Texture.hpp"
#include "UniformBufferObjects.hpp"
#include <cstddef>
#include <cstring>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

class Object {
private:
    Pipeline* pipeline;
    RenderContext context;
    MeshLoader* mesh;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;

    std::vector<VkBuffer> vs_uniformBuffers;
    std::vector<VkDeviceMemory> vs_uniformBuffersMemory;
    std::vector<void*> vs_uniformBuffersMapped;
    std::vector<VkBuffer> fs_uniformBuffers;
    std::vector<VkDeviceMemory> fs_uniformBuffersMemory;
    std::vector<void*> fs_uniformBuffersMapped;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<std::shared_ptr<Texture>> textures;

    MVP_UBO mvpUBO;
    MaterialUBO materialUBO;

    glm::vec3 position = {0,0,0};
    float scale = 1.0;

    void createVertexBuffer();

    void uploadVertices();

    void createIndexBuffer();

    void uploadIndices();

    void createUniformBuffers();

    void createDescriptorSets();

    void updateDescriptorSets();

    void updateModelMat();

    public:

    Object(const RenderContext& context, Pipeline* pipeline, MeshLoader* pMesh);

    Object() {};

    void draw(VkCommandBuffer cmd, size_t frameIndex) const;

    Pipeline* getPipeline() const;

    void update(const Camera& camera);

    MaterialUBO* ubo();
    void addTexture(std::shared_ptr<Texture> pTexture);

    void setScale(float scale);
    void setPosition(const glm::vec3& pos);
    const glm::vec3& getPosition() const;

    void destroy();
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

};