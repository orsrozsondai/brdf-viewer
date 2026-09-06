#pragma once
#include "RenderContext.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "helpers.hpp"
#include <glm/glm.hpp>


class EnvMap {
private: 
    RenderContext context;
    std::filesystem::path filePath;

    // Images
    GPUImage hdrImage;
    GPUImage environment;
    GPUImage irradiance;
    GPUImage prefilter;
    GPUImage brdfLUT;

    // Skybox rendering
    VkPipeline skyboxPipeline;
    VkPipelineLayout skyboxPipelineLayout;
    VkDescriptorSetLayout skyboxSetLayout;
    VkDescriptorPool skyboxDescriptorPool;
    VkDescriptorSet skyboxDescriptorSet;

    // Samplers
    VkSampler sampler;
    VkSampler brdfSampler;

    // Descriptor
    VkDescriptorSetLayout DSL;
    VkDescriptorSet DS;

    // Metadata
    void init();
    ImageData<float> loadImage();
    void createHDRImage();
    void createSamplers();
    void createDescriptorSet();

    void createSkyboxDescriptorSetLayout();
    void createSkyboxPipeline();
    void createSkyboxDescriptor();
    ImageData<float> generateEnv();

public:
    EnvMap(const RenderContext& context, const std::filesystem::path& path, VkDescriptorSetLayout DSL);
    EnvMap(const RenderContext& context, VkDescriptorSetLayout DSL); // use generated image
    void renderSkybox(VkCommandBuffer cmd, VkExtent2D extent, glm::mat4 view, glm::mat4 proj);
    void bindDescriptorSet(VkCommandBuffer cmd, VkPipelineLayout pl);
    
    void destroy();
};