#pragma once

#include "RenderContext.hpp"
#include "helpers.hpp"
#include <vulkan/vulkan_core.h>
class Texture {
public:
    enum Type {
        ALBEDO,
        NORMAL_MAP,
        ROUGHNESS_MAP,
        METALLIC_MAP
    };
private:
    RenderContext context;
    GPUImage image;
    VkFormat format;
    std::string path;
    VkSampler sampler = VK_NULL_HANDLE;
    Type type;

    ImageInfo loadImage();
    void create();



public:
    Texture(const RenderContext& context, const std::string& path, Type type);
    VkDescriptorImageInfo descriptorInfo() const;
    void destroy();
};