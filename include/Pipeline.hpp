#pragma once
#include "RenderContext.hpp"
#include "UniformBufferObjects.hpp"
#include <filesystem>
#include <vulkan/vulkan_core.h>
#include <string>
#include <vector>

class Pipeline {

private: 
    RenderContext context;
    std::filesystem::path vert, frag;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    void create(BRDF brdf = 0);

    void createLayout();

public:

    Pipeline(const RenderContext& context, const std::filesystem::path& vert, const std::filesystem::path&frag);

    const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const { return descriptorSetLayouts;}

    VkPipelineLayout getPipelineLayout() const { return pipelineLayout;}

    void bind(VkCommandBuffer cmd, const VkExtent2D& extent) const;

    void recreate(BRDF brdf = 0);

    void destroy();

};