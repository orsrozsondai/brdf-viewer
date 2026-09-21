#include "Texture.hpp"
#include "RenderContext.hpp"
#include "UniformBufferObjects.hpp"
#include "helpers.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <stb_image.h>
#include <string>
#include <vulkan/vulkan_core.h>


Texture::Texture(const RenderContext& context, const std::filesystem::path& path, Type type) : context(context), path(path), type(type) {
    create();
    
}

const Texture::Type Texture::getType() const {
    return type;
}

ImageData<stbi_uc> Texture::loadImage() {

    if (path.empty()) {
        throw std::runtime_error("File path is empty");
    }
    if (path.extension().string().compare(".jpg") == 0 || path.extension().string().compare(".png") == 0) {
        int desiredChannels = 0;
        switch (type) {
            case TEXTURE_ALBEDO: desiredChannels = STBI_rgb_alpha; format = VK_FORMAT_R8G8B8A8_SRGB; break;
            case TEXTURE_NORMAL_MAP: desiredChannels = STBI_rgb_alpha; format = VK_FORMAT_R8G8B8A8_UNORM; break;
            case TEXTURE_ROUGHNESS_MAP: desiredChannels = STBI_grey; format = VK_FORMAT_R8_UNORM; break;
            case TEXTURE_METALLIC_MAP: desiredChannels = STBI_grey; format = VK_FORMAT_R8_UNORM; break;
            case TEXTURE_EMISSION_MAP: desiredChannels = STBI_rgb_alpha; format = VK_FORMAT_R8G8B8A8_SRGB; break;
            case TEXTURE_AMBIENT_OCCLUSION_MAP: desiredChannels = STBI_grey; format = VK_FORMAT_R8_UNORM; break;
            default: break;
        }
        ImageData<stbi_uc> res(path, desiredChannels, true);
        res.channels = desiredChannels;
        return res;
    }
    else {
        throw std::runtime_error("Not supported image format");
    }
    
}

void Texture::create() {

    ImageData info = loadImage();
    image.mipLevels = std::floor(std::log2(std::max(info.width, info.height))) + 1;
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
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0,
        image.image,
        image.memory,
        image.mipLevels
    );

    transitionImageLayout(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        image.image,
        format,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        image.mipLevels
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

    generateMipmaps(info.width, info.height);

    image.view = createImageView(
        context.device,
        image.image,
        format,
        VK_IMAGE_ASPECT_COLOR_BIT,
        image.mipLevels,
        1,
        VK_IMAGE_VIEW_TYPE_2D
    );

    vkDestroyBuffer(context.device, stagingBuffer, nullptr);
    vkFreeMemory(context.device, stagingMemory, nullptr);

    //sampler


    sampler = createSampler(
        context.device,
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        image.mipLevels,
        true
    );
}

VkDescriptorImageInfo Texture::descriptorInfo() const {
    VkDescriptorImageInfo ret;
    ret.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ret.imageView = image.view;
    ret.sampler = sampler;

    return ret;
}

void Texture::generateMipmaps(uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer =
        beginSingleTimeCommands(context.device, context.commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image.image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = static_cast<int32_t>(width);
    int32_t mipHeight = static_cast<int32_t>(height);

    for (uint32_t i = 1; i < image.mipLevels; i++) {

        // Previous mip becomes the source for the blit.
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        VkImageBlit blit{};
        
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;

        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = {
            mipWidth,
            mipHeight,
            1
        };

        int32_t nextWidth = std::max(1, mipWidth / 2);
        int32_t nextHeight = std::max(1, mipHeight / 2);

        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = {
            nextWidth,
            nextHeight,
            1
        };

        vkCmdBlitImage(
            commandBuffer,
            image.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR
        );

        // The source mip is now finished.
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }

    // Last mip level still needs to transition.
    barrier.subresourceRange.baseMipLevel = image.mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        commandBuffer
    );
}

void Texture::destroy() {
    vkDeviceWaitIdle(context.device);
    image.destroy(context.device);
    if (sampler != VK_NULL_HANDLE)
        vkDestroySampler(context.device, sampler, nullptr);
    sampler = VK_NULL_HANDLE;
}