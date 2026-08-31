#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct GPUImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t mipLevels = 1;

    void destroy(VkDevice device);
};

struct ImageInfo {
    float* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
};

void copyBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size
);

void createBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory);

void createImage(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    uint32_t width,
    uint32_t height,
    VkSampleCountFlagBits samples,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImageCreateFlags flags,
    VkImage& image,
    VkDeviceMemory& imageMemory,
    uint32_t mipLevels = 1,
    uint32_t layers = 1
);

void copyBufferToImage(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height,
    uint32_t layerCount = 1
);

VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags,
    uint32_t mipLevels,
    uint32_t layerCount,
    VkImageViewType viewType,
    uint32_t mip = 0
);

VkImageView createCubemapFaceView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    uint32_t mipLevel,
    uint32_t face
);

void transitionImageLayout(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    uint32_t mipLevels = 1,
    uint32_t layerCount = 1
);

std::vector<char> readFile(const std::string& path);

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code); 

VkFramebuffer createFramebuffer(
    VkDevice device,
    VkRenderPass renderPass,
    VkImageView imageView,
    uint32_t width,
    uint32_t height,
    uint32_t layers = 1
);

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

void endSingleTimeCommands(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkCommandBuffer commandBuffer
);

VkSampler createSampler(
    VkDevice device,
    VkFilter filter,
    VkSamplerAddressMode addressMode,
    float maxLod,
    bool enableAnisotropy = true
);