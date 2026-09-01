#pragma once

#include "RenderContext.hpp"
#include "UniformBufferObjects.hpp"
#include "helpers.hpp"
#include <vulkan/vulkan_core.h>
class Texture {
public:
    using Type = TextureFlags;
private:
    RenderContext context;
    GPUImage image;
    VkFormat format;
    std::string path;
    VkSampler sampler = VK_NULL_HANDLE;
    Type type;

    ImageInfo<stbi_uc> loadImage();
    void create();



public:
    Texture(const RenderContext& context, const std::string& path, Type type);
    VkDescriptorImageInfo descriptorInfo() const;
    void destroy();
};