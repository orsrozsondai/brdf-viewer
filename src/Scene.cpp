#include "Scene.hpp"
#include "Camera.hpp"
#include "EnvMap.hpp"
#include "MeshLoader.hpp"
#include "Object.hpp"
#include "Pipeline.hpp"
#include "RenderContext.hpp"
#include "UniformBufferObjects.hpp"
#include <cstddef>
#include <glm/ext/vector_float3.hpp>
#include <memory>
#include <array>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "Config.hpp"

Scene::Scene(const RenderContext& context, Pipeline* pPipeline, Camera* pCamera) : context(context), pipeline(pPipeline), camera(pCamera) {
    meshes = std::vector<std::unique_ptr<MeshLoader>>();
    createUniformBuffers();
    createDescriptorSets();
    updateDescriptorSets();
}

void Scene::addMesh(std::unique_ptr<MeshLoader> mesh) {
    meshes.push_back(std::move(mesh));
    if (objects.empty()) createObjects();
}

void Scene::createDescriptorSets() {
    descriptorSets = std::vector<VkDescriptorSet>(context.imageCount);
    std::vector<VkDescriptorSetLayout> layouts(
        context.imageCount,
        pipeline->getDescriptorSetLayouts()[1]
    );
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.descriptorPool;
    allocInfo.descriptorSetCount = layouts.size();
    allocInfo.pSetLayouts = layouts.data();

    vkAllocateDescriptorSets(
        context.device,
        &allocInfo,
        descriptorSets.data()
    );

}

void Scene::updateDescriptorSets() {
    for (size_t i = 0; i < context.imageCount; i++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(SceneUBO);


        std::array<VkWriteDescriptorSet, 1> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptorSets[i];
        writes[0].dstBinding = 0; 
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;


        vkUpdateDescriptorSets(
            context.device,
            writes.size(),
            writes.data(),
            0, nullptr
        );
    }
}

void Scene::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(context.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context.device, buffer, &memRequirements);

    uint32_t memoryTypeIndex;
    bool found = false;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(
        context.physicalDevice,
        &memProperties
    );

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            found = true;
            break;
        }
    }

    if (!found) throw std::runtime_error("failed to find suitable memory type!");

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(context.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(context.device, buffer, bufferMemory, 0);
}

void Scene::createUniformBuffers() {
    uniformBuffers = std::vector<VkBuffer>(context.imageCount);
    uniformBuffersMemory = std::vector<VkDeviceMemory>(context.imageCount);
    uniformBuffersMapped = std::vector<void*>(context.imageCount);

    VkDeviceSize size = sizeof(SceneUBO);

    for (size_t i = 0; i < context.imageCount; i++) {
        createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
        vkMapMemory(context.device, uniformBuffersMemory[i], 0, size, 0, &uniformBuffersMapped[i]);
    }
}

void Scene::createObjects() {
    for (int i = 0; i < MAX_OBJECT_COUNT; i++) {
        Object* obj = new Object(context, pipeline, meshes[meshIndex].get());
        objects.push_back(obj);
        obj->ubo()->albedo = glm::vec3((float)(i+1) / (float)(MAX_OBJECT_COUNT+2))*0.2f;
    }
    arrangeObjects();
}

void Scene::destroyObjects() {
    for (int i = 0; i < MAX_OBJECT_COUNT; i++) {
        objects[i]->destroy();
        delete objects[i];
    }
    objects.clear();
}

void Scene::setMeshIndex(int index) {
    meshIndex = index;
    destroyObjects();
    createObjects();
}
const std::vector<const char*> Scene::getMeshNames() const {
    std::vector<const char*> names(meshes.size());
    for (size_t i = 0; i < names.size(); i++) {
        names[i] = meshes[i].get()->getName().c_str();
    }
    return names;
}

void Scene::update() {
    
    for (int i = 0; i < currentObjectCount; i++) {
        objects[i]->update(*camera);
    }

    for (uint32_t index = 0; index < context.imageCount; index++) {
        sceneUBO.camPos = camera->getPosition();
        memcpy(uniformBuffersMapped[index], &sceneUBO, sizeof(SceneUBO));
    }
}

void Scene::draw(VkCommandBuffer cmd, VkExtent2D extent, size_t frameIndex) {
    if (env && sceneUBO.ibl) {
        env->renderSkybox(cmd, extent, camera->view(), camera->proj());
    }
    pipeline->bind(cmd, extent);

    vkCmdBindDescriptorSets(
       cmd,
       VK_PIPELINE_BIND_POINT_GRAPHICS,
       pipeline->getPipelineLayout(),
       1,                     
       1,
       &descriptorSets[frameIndex],
       0,
       nullptr
    );

    if (env) 
        env->bindDescriptorSet(cmd, pipeline->getPipelineLayout());

    for (int i = 0; i < currentObjectCount; i++) {
        objects[i]->draw(cmd, frameIndex);
    }
}

Pipeline* Scene::getPipeline() const {
    return pipeline;
}

SceneUBO* Scene::ubo() {
    return &sceneUBO;
}

Object* Scene::selectedObject() {
    return objects[selectedObjectIndex];
}

void Scene::selectObject(int index) {
    if (index < 0 || index >= currentObjectCount) return;
    selectedObjectIndex = index;
    camera->lookAt(selectedObject()->getPosition());

}
void Scene::cycleSelected(int dir) {
    if (dir == -1) {
        selectedObjectIndex--;
        if (selectedObjectIndex == -1) selectedObjectIndex = currentObjectCount-1;
    }
    else if (dir == 1) {
        selectedObjectIndex++;
        if (selectedObjectIndex == currentObjectCount) selectedObjectIndex = 0;
    }
    selectObject(selectedObjectIndex);
}

void Scene::setObjectCount(int c) {
    currentObjectCount = c;
    if (currentObjectCount <= selectedObjectIndex) {
        selectedObjectIndex = currentObjectCount - 1;
        selectObject(selectedObjectIndex);
    }
}

void Scene::arrangeObjects() {
    for (size_t i = 0; i < objects.size(); i++) {
        objects[i]->setPosition({i*objectDistance, 0, 0});
    }
}

void Scene::applySettingsToAll() {
    MaterialUBO& current = *(objects[selectedObjectIndex]->ubo());
    for (auto obj : objects) {
        *(obj->ubo()) = current;
    }
}

void Scene::setObjectDistance(float d) {
    objectDistance = d;
    arrangeObjects();
    selectObject(selectedObjectIndex);
}

void Scene::interpolateFloat(MaterialParameters param) {
    float first = *(float*)objects.front()->ubo()->get(param);
    float last = *(float*)objects[currentObjectCount-1]->ubo()->get(param);
    float d = (last - first) / (currentObjectCount - 1);
    for (size_t i = 1; i < currentObjectCount-1; i++) {
        float* val = (float*)objects[i]->ubo()->get(param);
        *val = first + d * i;
    }
}

void Scene::interpolateVec3(MaterialParameters param) {
    glm::vec3 first = *(glm::vec3*)objects.front()->ubo()->get(param);
    glm::vec3 last = *(glm::vec3*)objects[currentObjectCount-1]->ubo()->get(param);
    glm::vec3 d = (last - first) / (float)(currentObjectCount - 1);
    for (size_t i = 1; i < currentObjectCount-1; i++) {
        glm::vec3* val = (glm::vec3*)objects[i]->ubo()->get(param);
        *val = first + d * (float)i;
    }
}

void Scene::interpolate(MaterialParameters param) {
    if (param == ALBEDO) interpolateVec3(param);
    else interpolateFloat(param);
}

bool Scene::isObjectInterpolated() {
    return (selectedObjectIndex > 0) & (selectedObjectIndex < currentObjectCount-1);  
}

void Scene::addEnvMap(EnvMap* pEnv) {
    env = pEnv;
}

void Scene::destroy() {
    if (env) env->destroy();
    destroyObjects();
    for (VkBuffer& buffer : uniformBuffers) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context.device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
    }

    for (VkDeviceMemory& memory : uniformBuffersMemory) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(context.device, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    }
    pipeline->destroy();
}