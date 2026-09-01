#include "EnvMap.hpp"
#include "IBLProcessor.hpp"
#include "RenderContext.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan_core.h>
#include "helpers.hpp"
#include "stb_image.h"
#include <iostream>

EnvMap::EnvMap(const RenderContext& context, const std::string& path, VkDescriptorSetLayout DSL) : context(context), filePath(path), DSL(DSL) {
    init();
}
EnvMap::EnvMap(const RenderContext& context, VkDescriptorSetLayout DSL) : context(context), filePath(""), DSL(DSL) {
    init();
}

ImageInfo<float> EnvMap::loadImage() {
    ImageInfo<float> res;

    if (filePath.empty()) {
        res = generateEnv();
        std::cout << "Image generated: " << res.width << " x " << res.height << std::endl;
    }
    else {
        res.data = stbi_loadf(filePath.c_str(), &res.width, &res.height, &res.channels, STBI_rgb_alpha);
        if (res.data)
            std::cout << "Image loaded: " << res.width << " x " << res.height << std::endl;
        else
            throw std::runtime_error("Failed to load image: " + filePath);
    }
    return res;
}

ImageInfo<float> EnvMap::generateEnv() {

    ImageInfo<float> img;
    img.width = 4096;
    img.height = 2048;
    img.channels = 4; // RGBA

    img.data = new float[img.width * img.height * 4];

    const float PI = 3.14159265359f;

    // Sun direction
    float sunDir[3] = { 0.0f, 0.8f, 0.6f };
    float len = std::sqrt(sunDir[0]*sunDir[0] + sunDir[1]*sunDir[1] + sunDir[2]*sunDir[2]);
    sunDir[0] /= len;
    sunDir[1] /= len;
    sunDir[2] /= len;

    for (int y = 0; y < img.height; ++y)
    {
        float v = (float)y / (img.height - 1);
        float theta = v * PI;

        for (int x = 0; x < img.width; ++x)
        {
            float u = (float)x / (img.width - 1);
            float phi = u * 2.0f * PI;

            // Direction
            float dir[3];
            dir[0] = std::sin(theta) * std::cos(phi);
            dir[1] = std::cos(theta);
            dir[2] = std::sin(theta) * std::sin(phi);

            float color[3];

            if (dir[1] >= 0.0f)
            {
                // ---- SKY ----
                float t = dir[1]; // already [0,1]

                float skyColor[3] = {
                    (1.0f - t) * 0.8f + t * 0.1f,
                    (1.0f - t) * 0.9f + t * 0.4f,
                    (1.0f - t) * 1.0f + t * 0.8f
                };

                color[0] = skyColor[0];
                color[1] = skyColor[1];
                color[2] = skyColor[2];
            }
            else
            {
                // ---- GROUND ----
                float t = -dir[1]; // [0,1] downward

                // Dark ground color (slightly warm)
                float groundColor[3] = {
                    0.05f + 0.1f * t,
                    0.04f + 0.08f * t,
                    0.03f + 0.06f * t
                };

                color[0] = groundColor[0];
                color[1] = groundColor[1];
                color[2] = groundColor[2];
            }


            // ---- SUN ----
            float dot = dir[0]*sunDir[0] + dir[1]*sunDir[1] + dir[2]*sunDir[2];
            float sun = std::pow(std::fmax(dot, 0.0f), 1000.0f);
            float sunIntensity = 30.0f;

            if (dir[1] >= 0.0f)
            {
                // SKY
                float t = dir[1];
            
                color[0] = (1.0f - t) * 0.8f + t * 0.1f;
                color[1] = (1.0f - t) * 0.9f + t * 0.4f;
                color[2] = (1.0f - t) * 1.0f + t * 0.8f;
            }
            else
            {
                // GROUND
                float t = -dir[1];
            
                color[0] = 0.05f + 0.1f * t;
                color[1] = 0.04f + 0.08f * t;
                color[2] = 0.03f + 0.06f * t;
            }

            int idx = (y * img.width + x) * 4;
            img.data[idx + 0] = color[0] + sun * sunIntensity;
            img.data[idx + 1] = color[1] + sun * sunIntensity;
            img.data[idx + 2] = color[2] + sun * sunIntensity;
            img.data[idx + 3] = 1.0f;
        }
    }

    return img;

}

void EnvMap::init() {
    createHDRImage();
    IBLProcessor proc(context, hdrImage);
    environment = proc.createEnvironmentCubemap();
    createSamplers();
    createSkyboxDescriptorSetLayout();
    createSkyboxPipeline();
    createSkyboxDescriptor();

    irradiance = proc.createIrradianceMap();
    prefilter = proc.createPrefilterMap();
    brdfLUT = proc.createBRDFLUT();

    createDescriptorSet();
    proc.destroy();
}

void EnvMap::createHDRImage() {

    ImageInfo image = loadImage();
    VkDeviceSize imageSize = image.width * image.height * 4 * sizeof(float);

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
    memcpy(data, image.data, imageSize);
    vkUnmapMemory(context.device, stagingMemory);

    createImage(
        context.device,
        context.physicalDevice,
        image.width,
        image.height,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0,
        hdrImage.image,
        hdrImage.memory
    );

    transitionImageLayout(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        hdrImage.image,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    
    copyBufferToImage(
        context.device, 
        context.commandPool, 
        context.graphicsQueue, 
        stagingBuffer, 
        hdrImage.image, 
        image.width, 
        image.height
    );

    transitionImageLayout(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        hdrImage.image,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );


    hdrImage.view = createImageView(
        context.device,
        hdrImage.image,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1,
        1,
        VK_IMAGE_VIEW_TYPE_2D
    );

    vkDestroyBuffer(context.device, stagingBuffer, nullptr);
    vkFreeMemory(context.device, stagingMemory, nullptr);

    stbi_image_free(image.data);
}

void EnvMap::createSamplers() {
    sampler = createSampler(
        context.device,
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        11,
        true
    );

    brdfSampler = createSampler(
        context.device,
        VK_FILTER_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        false
    );

}

void EnvMap::createDescriptorSet() {
    
    VkDescriptorImageInfo irradianceInfo {
        sampler,
        irradiance.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo prefilterInfo {
        sampler,
        prefilter.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo brdfInfo {
        brdfSampler,
        brdfLUT.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorSetAllocateInfo allocInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &DSL;
    allocInfo.pNext = nullptr;

    vkAllocateDescriptorSets(context.device, &allocInfo, &DS);

    std::array<VkWriteDescriptorSet, 3> writes{};

    writes[0] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = DS,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &irradianceInfo
    };

    writes[1] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = DS,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &prefilterInfo
    };

    writes[2] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = DS,
        .dstBinding = 2,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &brdfInfo
    };

    vkUpdateDescriptorSets(context.device, writes.size(), writes.data(), 0, nullptr);
}
    
void EnvMap::bindDescriptorSet(VkCommandBuffer cmd, VkPipelineLayout pl) {
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pl, 2, 1, &DS, 0, nullptr);
}

void EnvMap::createSkyboxDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;

    vkCreateDescriptorSetLayout(context.device, &info, nullptr, &skyboxSetLayout);
}

void EnvMap::createSkyboxDescriptor() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &skyboxDescriptorPool);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = skyboxDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &skyboxSetLayout;

    vkAllocateDescriptorSets(context.device, &allocInfo, &skyboxDescriptorSet);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = environment.view;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = skyboxDescriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
}

void EnvMap::createSkyboxPipeline() {
    auto vertCode = readFile("build/shaders/skybox.vert.spv");
    auto fragCode = readFile("build/shaders/skybox.frag.spv");

    VkShaderModule vertModule = createShaderModule(context.device, vertCode);
    VkShaderModule fragModule = createShaderModule(context.device, fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 0;
    vertexInput.pVertexBindingDescriptions      = nullptr;
    vertexInput.vertexAttributeDescriptionCount = 0;
    vertexInput.pVertexAttributeDescriptions    = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;


    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = context.samples;

    VkPipelineColorBlendAttachmentState colorBlend{};
    colorBlend.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &colorBlend;

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(glm::mat4) * 2;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &skyboxSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;

    vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &skyboxPipelineLayout);

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = shaderStages;
    info.pDynamicState = &dynamic;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &ms;
    info.pColorBlendState = &blend;
    info.pDepthStencilState = &depthStencil;

    info.layout = skyboxPipelineLayout;
    info.renderPass = context.renderPass;
    info.subpass = 0;

    if (vkCreateGraphicsPipelines(
        context.device,
        VK_NULL_HANDLE,
        1,
        &info,
        nullptr,
        &skyboxPipeline
    ) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline!");
    }
    vkDestroyShaderModule(context.device, vertModule, nullptr);
    vkDestroyShaderModule(context.device, fragModule, nullptr);
}

void EnvMap::renderSkybox(VkCommandBuffer cmd, VkExtent2D extent, glm::mat4 view, glm::mat4 proj) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);

    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width  = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        skyboxPipelineLayout,
        0, 1,
        &skyboxDescriptorSet,
        0, nullptr);

    vkCmdPushConstants(cmd, skyboxPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(glm::mat4), &view);

    vkCmdPushConstants(cmd, skyboxPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT,
        sizeof(glm::mat4), sizeof(glm::mat4), &proj);

    vkCmdDraw(cmd, 36, 1, 0, 0);
}

void EnvMap::destroy() {
    vkDeviceWaitIdle(context.device);
    hdrImage.destroy(context.device);
    environment.destroy(context.device);
    irradiance.destroy(context.device);
    prefilter.destroy(context.device);
    brdfLUT.destroy(context.device);
    vkDestroyPipeline(context.device, skyboxPipeline, nullptr);
    vkDestroyPipelineLayout(context.device, skyboxPipelineLayout, nullptr);
    vkDestroyDescriptorPool(context.device, skyboxDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(context.device, skyboxSetLayout, nullptr);
    vkDestroySampler(context.device, sampler, nullptr);
    vkDestroySampler(context.device, brdfSampler, nullptr);
}