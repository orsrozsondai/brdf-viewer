#include "Object.hpp"
#include "MeshLoader.hpp"
#include "Pipeline.hpp"
#include "RenderContext.hpp"
#include "Texture.hpp"
#include "UniformBufferObjects.hpp"
#include "helpers.hpp"
#include <cstdint>
#include <cstring>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float4.hpp>
#include <stdexcept>
#include <vector>
#include <array>
#include <vulkan/vulkan_core.h>

Object::Object(const RenderContext& context, Pipeline* pipeline, MeshLoader* pMesh) 
        : pipeline(pipeline), context(context), mesh(pMesh) {
    if (mesh->getVertices().empty()) throw std::runtime_error("vertices is empty");
    if (mesh->getIndices().empty()) throw std::runtime_error("indices is empty");
    createVertexBuffer();
    uploadVertices();
    createIndexBuffer();
    uploadIndices();
    createUniformBuffers();
    createDescriptorSets();
    updateDescriptorSets();
    updateModelMat();
}

void Object::createVertexBuffer() {
    VkDeviceSize size = sizeof(mesh->getVertices()[0]) * mesh->getVertices().size();
    createBuffer(
        context.device,
        context.physicalDevice,
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer,
        vertexMemory
    );
}

void Object::createIndexBuffer() {
    VkDeviceSize size = sizeof(mesh->getIndices()[0]) * mesh->getIndices().size();
    createBuffer(
        context.device,
        context.physicalDevice,
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer,
        indexMemory
    );
}

void Object::uploadVertices() {
    VkDeviceSize size = sizeof(mesh->getVertices()[0]) * mesh->getVertices().size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    createBuffer(
        context.device,
        context.physicalDevice,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );
    
    void* data;
    vkMapMemory(context.device, stagingMemory, 0, size, 0, &data);
    memcpy(data, mesh->getVertices().data(), (size_t)size);
    vkUnmapMemory(context.device, stagingMemory);

    copyBuffer(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        stagingBuffer,
        vertexBuffer,
        size
    );

    vkDestroyBuffer(context.device, stagingBuffer, nullptr);
    vkFreeMemory(context.device, stagingMemory, nullptr);

}

void Object::uploadIndices() {
    VkDeviceSize size = sizeof(mesh->getIndices()[0]) * mesh->getIndices().size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    createBuffer(
        context.device,
        context.physicalDevice,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );
    
    void* data;
    vkMapMemory(context.device, stagingMemory, 0, size, 0, &data);
    memcpy(data, mesh->getIndices().data(), (size_t)size);
    vkUnmapMemory(context.device, stagingMemory);

    copyBuffer(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        stagingBuffer,
        indexBuffer,
        size
    );

    vkDestroyBuffer(context.device, stagingBuffer, nullptr);
    vkFreeMemory(context.device, stagingMemory, nullptr);
}

void Object::draw(VkCommandBuffer cmd, size_t frameIndex) const {
    VkBuffer buffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdBindDescriptorSets(
       cmd,
       VK_PIPELINE_BIND_POINT_GRAPHICS,
       pipeline->getPipelineLayout(),
       0,                     
       1,
       &descriptorSets[frameIndex],
       0,
       nullptr
    );
    
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    vkCmdDrawIndexed(cmd, (mesh->getIndices().size()), 1, 0, 0, 0);
}

void Object::update(const Camera& camera) {
    mvpUBO.view = camera.view();
    mvpUBO.proj = camera.proj();
    for (uint32_t index = 0; index < context.imageCount; index++) {
        memcpy(vs_uniformBuffersMapped[index], &mvpUBO, sizeof(MVP_UBO));
        memcpy(fs_uniformBuffersMapped[index], &materialUBO, sizeof(MaterialUBO));
    }
}

Pipeline* Object::getPipeline() const {
    return pipeline;
}

void Object::destroy(){
    vkDeviceWaitIdle(context.device);
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context.device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }

    if (vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, vertexMemory, nullptr);
        vertexMemory = VK_NULL_HANDLE;
    }

    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context.device, indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }

    if (indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context.device, indexMemory, nullptr);
        indexMemory = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < context.imageCount; i++) {
        if (vs_uniformBuffers[i] != VK_NULL_HANDLE) vkDestroyBuffer(context.device, vs_uniformBuffers[i], nullptr);
        vs_uniformBuffers[i] = VK_NULL_HANDLE;
        if (vs_uniformBuffersMemory[i] != VK_NULL_HANDLE) vkFreeMemory(context.device, vs_uniformBuffersMemory[i], nullptr);
        vs_uniformBuffersMemory[i] = VK_NULL_HANDLE;
        if (fs_uniformBuffers[i] != VK_NULL_HANDLE) vkDestroyBuffer(context.device, fs_uniformBuffers[i], nullptr);
        fs_uniformBuffers[i] = VK_NULL_HANDLE;
        if (fs_uniformBuffersMemory[i] != VK_NULL_HANDLE) vkFreeMemory(context.device, fs_uniformBuffersMemory[i], nullptr);
        fs_uniformBuffersMemory[i] = VK_NULL_HANDLE;

    }
    if (descriptorSets[0] != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(
            context.device,
            context.descriptorPool,
            descriptorSets.size(),
            descriptorSets.data()
        );
        descriptorSets[0] = VK_NULL_HANDLE;
    }
}

void Object::createUniformBuffers() {
    vs_uniformBuffers = std::vector<VkBuffer>(context.imageCount);
    vs_uniformBuffersMemory = std::vector<VkDeviceMemory>(context.imageCount);
    vs_uniformBuffersMapped = std::vector<void*>(context.imageCount);

    VkDeviceSize size = sizeof(MVP_UBO);

    for (size_t i = 0; i < context.imageCount; i++) {
        createBuffer(
            context.device,
            context.physicalDevice,
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            vs_uniformBuffers[i],
            vs_uniformBuffersMemory[i]
        );
        vkMapMemory(context.device, vs_uniformBuffersMemory[i], 0, size, 0, &vs_uniformBuffersMapped[i]);

    }
    fs_uniformBuffers = std::vector<VkBuffer>(context.imageCount);
    fs_uniformBuffersMemory = std::vector<VkDeviceMemory>(context.imageCount);
    fs_uniformBuffersMapped = std::vector<void*>(context.imageCount);

    size = sizeof(MaterialUBO);

    for (size_t i = 0; i < context.imageCount; i++) {
        createBuffer(
            context.device,
            context.physicalDevice,
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            fs_uniformBuffers[i],
            fs_uniformBuffersMemory[i]
        );
        vkMapMemory(context.device, fs_uniformBuffersMemory[i], 0, size, 0, &fs_uniformBuffersMapped[i]);

    }
}

MaterialUBO* Object::ubo() {
    return &materialUBO;
}

void Object::addTexture(Texture* pTexture) {
    textures.push_back(pTexture);
}

void Object::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(
        context.imageCount,
        pipeline->getDescriptorSetLayouts()[0]
    );

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.descriptorPool;
    allocInfo.descriptorSetCount = layouts.size();
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets = std::vector<VkDescriptorSet>(context.imageCount);

    vkAllocateDescriptorSets(
        context.device,
        &allocInfo,
        descriptorSets.data()
    );
}

void Object::updateDescriptorSets() {
    std::vector<VkDescriptorImageInfo> imageInfos;
    for (Texture* texture : textures) {
        imageInfos.push_back(texture->descriptorInfo());
    }
    for (size_t i = 0; i < context.imageCount; i++)
    {
        VkDescriptorBufferInfo vsBufferInfo{};
        vsBufferInfo.buffer = vs_uniformBuffers[i];
        vsBufferInfo.offset = 0;
        vsBufferInfo.range  = sizeof(MVP_UBO);

        VkDescriptorBufferInfo fsBufferInfo{};
        fsBufferInfo.buffer = fs_uniformBuffers[i];
        fsBufferInfo.offset = 0;
        fsBufferInfo.range  = sizeof(MaterialUBO);

        uint writeCount = imageInfos.empty() ? 2 : 3;

        std::vector<VkWriteDescriptorSet> writes{writeCount};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptorSets[i];
        writes[0].dstBinding = 0; 
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &vsBufferInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = descriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &fsBufferInfo;

        // textures
        if (!imageInfos.empty()) {
            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = descriptorSets[i];
            writes[2].dstBinding = 2;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].descriptorCount = imageInfos.size();
            writes[2].pImageInfo = imageInfos.empty() ? nullptr : imageInfos.data();
        }

        vkUpdateDescriptorSets(
            context.device,
            writes.size(),
            writes.data(),
            0, nullptr
        );
    }
}

void Object::setScale(float scale) {
    this->scale = scale;
    updateModelMat();
}
void Object::setPosition(const glm::vec3& pos) {
    position = pos;
    updateModelMat();
}

const glm::vec3& Object::getPosition() const {
    return position;
}

void Object::updateModelMat() {
    mvpUBO.model = glm::mat4(1.0f);
    mvpUBO.model = glm::translate(mvpUBO.model, position);
    mvpUBO.model = glm::scale(mvpUBO.model, glm::vec3(scale));
}