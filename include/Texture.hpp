#pragma once

#include "RenderContext.hpp"
#include "UniformBufferObjects.hpp"
#include "helpers.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>
class Texture {
public:
    using Type = TextureType;
private:
    RenderContext context;
    GPUImage image;
    VkFormat format;
    std::filesystem::path path;
    VkSampler sampler = VK_NULL_HANDLE;
    Type type;

    ImageData<stbi_uc> loadImage();
    void create();
    void generateMipmaps(uint32_t width, uint32_t height);


public:
    Texture(const RenderContext& context, const std::filesystem::path& path, Type type);
    const Type getType() const;
    VkDescriptorImageInfo descriptorInfo() const;
    void destroy();
};