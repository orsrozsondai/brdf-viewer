#include "Texture.hpp"
#include "RenderContext.hpp"
#include "UniformBufferObjects.hpp"
#include "helpers.hpp"
#include <iostream>
#include <stdexcept>
#include <stb_image.h>
#include <string>
#include <vulkan/vulkan_core.h>


Texture::Texture(const RenderContext& context, const std::filesystem::path& path, Type type) : context(context), path(path), type(type) {
    create();
    
}

ImageData<stbi_uc> Texture::loadImage() {

    if (path.empty()) {
        throw std::runtime_error("File path is empty");
    }
    if (path.extension().string().compare(".jpg") == 0 || path.extension().string().compare(".png") == 0) {
        int desiredChannels = 0;
        switch (type) {
            case TEXTURE_ALBEDO: desiredChannels = STBI_rgb_alpha; break;
            case TEXTURE_NORMAL_MAP: desiredChannels = STBI_rgb_alpha; break;
            case TEXTURE_ROUGHNESS_MAP: desiredChannels = STBI_grey; break;
            case TEXTURE_METALLIC_MAP: desiredChannels = STBI_grey; break;
            default: break;
        }
        ImageData<stbi_uc> res(path, desiredChannels, true);
        switch (res.channels) {
            case 1: format = VK_FORMAT_R8_UNORM; break;
            default: format = VK_FORMAT_R8G8B8A8_SRGB; res.channels = 4; break;
        }
        return res;
    }
    else {
        throw std::runtime_error("Not supported image format");
    }
    
}

void Texture::create() {

    ImageData info = loadImage();
    VkDeviceSize imageSize = info.width * info.height * info.channels * sizeof(stbi_uc);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;


    createBuffer(
        context.device,
        context.physicalDevice,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );


    void *data;
    vkMapMemory(context.device, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, info.data, imageSize);
    vkUnmapMemory(context.device, stagingMemory);

    createImage(
        context.device,
        context.physicalDevice,
        info.width,
        info.height,
        VK_SAMPLE_COUNT_1_BIT,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0,
        image.image,
        image.memory
    );

    transitionImageLayout(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        image.image,
        format,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    
    copyBufferToImage(
        context.device, 
        context.commandPool, 
        context.graphicsQueue, 
        stagingBuffer, 
        image.image, 
        info.width, 
        info.height
    );

    transitionImageLayout(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        image.image,
        format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );


    image.view = createImageView(
        context.device,
        image.image,
        format,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1,
        1,
        VK_IMAGE_VIEW_TYPE_2D
    );

    vkDestroyBuffer(context.device, stagingBuffer, nullptr);
    vkFreeMemory(context.device, stagingMemory, nullptr);

    //sampler

    sampler = createSampler(context.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0, false);
}

VkDescriptorImageInfo Texture::descriptorInfo() const {
    VkDescriptorImageInfo ret;
    ret.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ret.imageView = image.view;
    ret.sampler = sampler;

    return ret;
}

void Texture::destroy() {
    vkDeviceWaitIdle(context.device);
    image.destroy(context.device);
    if (sampler != VK_NULL_HANDLE)
        vkDestroySampler(context.device, sampler, nullptr);
    sampler = VK_NULL_HANDLE;
}