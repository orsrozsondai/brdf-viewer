#include "IBLProcessor.hpp"
#include "RenderContext.hpp"
#include "helpers.hpp"
#include <cstddef>
#include <cstdint>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

IBLProcessor::IBLProcessor(const RenderContext& context, const GPUImage& hdrImage) : context(context), hdrImage(hdrImage) {
    createRenderPass();
    createHDRSampler();
    createHDRDescriptorPool();
    createHDRDescriptorSetLayout();
    allocateHDRDescriptorSet();
    createEqToCubePipeline();
    updateHDRDescriptorSet();

    createCubeSampler();
    createComputeDSL();
    createComputePL();
    irradiancePipeline = createComputePipeline("build/shaders/irradiance.comp.spv");
    prefilterPipeline = createComputePipeline("build/shaders/prefilter.comp.spv");
    brdfPipeline = createComputePipeline("build/shaders/brdfLUT.comp.spv");
}

void IBLProcessor::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cubemap render pass!");
    }
}

void IBLProcessor::createHDRSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerInfo.mipLodBias   = 0.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy    = 1.0f;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp     = VK_COMPARE_OP_ALWAYS;

    samplerInfo.minLod       = 0.0f;
    samplerInfo.maxLod       = VK_LOD_CLAMP_NONE;
    samplerInfo.borderColor  = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if (vkCreateSampler(context.device, &samplerInfo, nullptr, &hdrSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create HDR sampler!");

}

void IBLProcessor::createHDRDescriptorPool() {
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 20;

    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 20;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = 40;

    
    if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &hdrDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR descriptor pool!");
    }

}

void IBLProcessor::createHDRDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding            = 0;
    binding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount    = 1;
    binding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &hdrDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR descriptor set layout!");
    }

}

void IBLProcessor::allocateHDRDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = hdrDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &hdrDescriptorLayout;

    if (vkAllocateDescriptorSets(context.device, &allocInfo, &hdrDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate HDR descriptor set!");
    }

}

void IBLProcessor::updateHDRDescriptorSet() {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = hdrImage.view;
    imageInfo.sampler     = hdrSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = hdrDescriptorSet;
    descriptorWrite.dstBinding      = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
}

void IBLProcessor::createEqToCubePipeline() {
    VkShaderModule vertModule = createShaderModule(context.device, readFile("build/shaders/eqToCube.vert.spv"));
    VkShaderModule fragModule = createShaderModule(context.device, readFile("build/shaders/eqToCube.frag.spv"));

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

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

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = faceSize;
    viewport.height   = faceSize;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkExtent2D extent;
    extent.height = faceSize;
    extent.width = faceSize;
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable   = VK_FALSE;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;


    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset = 0;
    range.size = 2 * sizeof(glm::mat4);
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &hdrDescriptorLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &range;

    if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &hdrPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create eqToCube pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = nullptr;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = hdrPipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &eqToCubePipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create eqToCube pipeline");

    vkDestroyShaderModule(context.device, vertModule, nullptr);
    vkDestroyShaderModule(context.device, fragModule, nullptr);
}

GPUImage IBLProcessor::createEnvironmentCubemap() {

    createImage(
        context.device,
        context.physicalDevice,
        faceSize,
        faceSize,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        envMap.image,
        envMap.memory,
        1,
        6 
    );

    envMap.view = createImageView(
        context.device,
        envMap.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1,
        6,
        VK_IMAGE_VIEW_TYPE_CUBE
    );

    transitionImageLayout(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        envMap.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        1,
        6
    );
    

    std::vector<VkFramebuffer> framebuffers(6);
    std::vector<VkImageView> faceViews(6);
    for (uint32_t face = 0; face < 6; ++face) {
        faceViews[face] = createCubemapFaceView(
            context.device,
            envMap.image,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            0,
            face
        );

        framebuffers[face] = createFramebuffer(
            context.device,
            renderPass,
            faceViews[face],
            faceSize,
            faceSize
        );
    }

    VkCommandBuffer cmd = beginSingleTimeCommands(context.device, context.commandPool);

    glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[6] = {
        glm::lookAt(glm::vec3(0), glm::vec3(1,0,0),   glm::vec3(0,-1,0)),
        glm::lookAt(glm::vec3(0), glm::vec3(-1,0,0),  glm::vec3(0,-1,0)),
        glm::lookAt(glm::vec3(0), glm::vec3(0,1,0),   glm::vec3(0,0,1)),
        glm::lookAt(glm::vec3(0), glm::vec3(0,-1,0),  glm::vec3(0,0,-1)),
        glm::lookAt(glm::vec3(0), glm::vec3(0,0,1),   glm::vec3(0,-1,0)),
        glm::lookAt(glm::vec3(0), glm::vec3(0,0,-1),  glm::vec3(0,-1,0))
    };

    for (uint32_t face = 0; face < 6; ++face) {
        VkClearValue clearValue{};
        clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass  = renderPass;
        renderPassInfo.framebuffer = framebuffers[face];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {faceSize, faceSize};
        renderPassInfo.clearValueCount   = 1;
        renderPassInfo.pClearValues      = &clearValue;

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, eqToCubePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hdrPipelineLayout,
                                0, 1, &hdrDescriptorSet, 0, nullptr);

        vkCmdPushConstants(cmd, hdrPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(glm::mat4), &captureViews[face]);
        vkCmdPushConstants(cmd, hdrPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           sizeof(glm::mat4), sizeof(glm::mat4), &captureProj);

        vkCmdDraw(cmd, 36, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
    }
    
    endSingleTimeCommands(context.device, context.commandPool, context.graphicsQueue, cmd);

    transitionImageLayout(
        context.device,
        context.commandPool,
        context.graphicsQueue,
        envMap.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1,
        6
    );

    
    for (auto fb : framebuffers)
        vkDestroyFramebuffer(context.device, fb, nullptr);

    for (auto fv : faceViews)
        vkDestroyImageView(context.device, fv, nullptr);

    return envMap;
}

void IBLProcessor::createComputeDSL() {
    VkDescriptorSetLayoutBinding bindings[2]{};

    // Input cubemap
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Output image
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    ci.bindingCount = 2;
    ci.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(context.device, &ci, nullptr, &computeDSL) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute descriptor set layout");
    }
}

void IBLProcessor::createComputePL() {
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push.offset = 0;
    push.size = sizeof(glm::vec4);

    VkPipelineLayoutCreateInfo ci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    ci.setLayoutCount = 1;
    ci.pSetLayouts = &computeDSL;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges = &push;

    if (vkCreatePipelineLayout(context.device, &ci, nullptr, &computePL) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout");
    }
}

VkPipeline IBLProcessor::createComputePipeline(const std::string& path) {
    VkPipeline pipeline;
    auto code = readFile(path);
    VkShaderModule module = createShaderModule(context.device, code);

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = module;
    stage.pName = "main";

    VkComputePipelineCreateInfo ci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    ci.stage = stage;
    ci.layout = computePL;

    if (vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline");
    }
    vkDestroyShaderModule(context.device, module, nullptr);
    return pipeline;
}

void IBLProcessor::createCubeSampler() {

    cubeSampler = createSampler(
        context.device,
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        false
    );

}

VkDescriptorSet IBLProcessor::allocateComputeDS(VkImageView input, VkImageView output) {
    VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    ai.descriptorPool = hdrDescriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &computeDSL;

    VkDescriptorSet ds;
    if (vkAllocateDescriptorSets(context.device, &ai, &ds) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    updateComputeDS(ds, input, output);
    
    return ds;
}

void IBLProcessor::updateComputeDS(VkDescriptorSet DS, VkImageView input, VkImageView output) {
    // Input cubemap
    VkDescriptorImageInfo inputInfo{};
    inputInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputInfo.imageView = input;
    inputInfo.sampler = cubeSampler;

    // Output image
    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputInfo.imageView = output;

    VkWriteDescriptorSet writes[2]{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = DS;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &inputInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = DS;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &outputInfo;

    vkUpdateDescriptorSets(context.device, 2, writes, 0, nullptr);
}



GPUImage IBLProcessor::createIrradianceMap() {
    GPUImage irradianceMap;
    const uint32_t irradianceSize = 64;
    createImage(
        context.device,
        context.physicalDevice,
        irradianceSize,
        irradianceSize,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        irradianceMap.image,
        irradianceMap.memory,
        1,
        6
    );

    irradianceMap.view = createImageView(
        context.device,
        irradianceMap.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1,
        6,
        VK_IMAGE_VIEW_TYPE_CUBE
    );

    transitionImageLayout(
        context.device, 
        context.commandPool,
        context.graphicsQueue, 
        irradianceMap.image, 
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        1,
        6
    );


    VkDescriptorSet irradianceDS = allocateComputeDS(envMap.view, irradianceMap.view);
    
    VkCommandBuffer cmd = beginSingleTimeCommands(context.device, context.commandPool);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, irradiancePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePL, 0, 1, &irradianceDS, 0, nullptr);

    computePC pc{};
    pc.roughness = 0.0f;
    pc.faceSize = faceSize;

    vkCmdPushConstants(cmd, computePL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computePC), &pc);

    vkCmdDispatch(cmd, (irradianceSize + 7) / 8, (irradianceSize + 7) / 8, 6); // 6 for cubemap


    VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr
    );

    endSingleTimeCommands(context.device, context.commandPool, context.graphicsQueue, cmd);

    transitionImageLayout(
        context.device, 
        context.commandPool,
        context.graphicsQueue, 
        irradianceMap.image, 
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1,
        6
    );

    return irradianceMap;
}

GPUImage IBLProcessor::createPrefilterMap() {
    GPUImage prefilterMap;
    prefilterMap.mipLevels = floor(log2(faceSize)) + 1;
    createImage(
        context.device,
        context.physicalDevice,
        faceSize,
        faceSize,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        prefilterMap.image,
        prefilterMap.memory,
        prefilterMap.mipLevels,
        6
    );

    prefilterMap.view = createImageView(
        context.device,
        prefilterMap.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        prefilterMap.mipLevels,
        6,
        VK_IMAGE_VIEW_TYPE_CUBE
    );

    transitionImageLayout(
        context.device, 
        context.commandPool,
        context.graphicsQueue, 
        prefilterMap.image, 
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        prefilterMap.mipLevels,
        6
    );


    std::vector<VkImageView> mipViews(prefilterMap.mipLevels);
    VkCommandBuffer cmd = beginSingleTimeCommands(context.device, context.commandPool);
    for (uint32_t mip = 0; mip < prefilterMap.mipLevels; mip++)
    {
        uint32_t size = faceSize >> mip;

        mipViews[mip] = createImageView(
            context.device,
            prefilterMap.image, 
            VK_FORMAT_R16G16B16A16_SFLOAT, 
            VK_IMAGE_ASPECT_COLOR_BIT,
            1, 
            6,
            VK_IMAGE_VIEW_TYPE_CUBE,
            mip
        );
        auto ds = allocateComputeDS(envMap.view, mipViews[mip]);
        
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prefilterPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePL, 0, 1, &ds, 0, nullptr);

        computePC pc{};
        pc.roughness = (float)mip / float(prefilterMap.mipLevels - 1);
        pc.faceSize = size;
        pc.mipLevel = mip;

        vkCmdPushConstants(cmd, computePL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(computePC), &pc);

        vkCmdDispatch(cmd,
            (size + 7) / 8,
            (size + 7) / 8,
            6
        );
    
    }
    endSingleTimeCommands(context.device, context.commandPool, context.graphicsQueue, cmd);


    for (auto view : mipViews) {
        vkDestroyImageView(context.device, view, nullptr);
    }


    transitionImageLayout(
        context.device, 
        context.commandPool,
        context.graphicsQueue, 
        prefilterMap.image, 
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        prefilterMap.mipLevels,
        6
    );

    return prefilterMap;
}

GPUImage IBLProcessor::createBRDFLUT() {
    const uint32_t lutSize = 512;
    GPUImage brdfLUT;
    createImage(
        context.device,
        context.physicalDevice,
        lutSize,
        lutSize,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0,
        brdfLUT.image,
        brdfLUT.memory,
        1,
        1
    );

    brdfLUT.view = createImageView(
        context.device,
        brdfLUT.image,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        1,
        1,
        VK_IMAGE_VIEW_TYPE_2D
    );

    transitionImageLayout(
        context.device, 
        context.commandPool,
        context.graphicsQueue, 
        brdfLUT.image, 
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        1,
        1
    );

    VkDescriptorSet brdfDS = allocateComputeDS(envMap.view, brdfLUT.view);

    auto cmd = beginSingleTimeCommands(context.device, context.commandPool);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, brdfPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePL, 0, 1, &brdfDS, 0, nullptr);

    vkCmdDispatch(cmd, (lutSize+7)/8, (lutSize+7)/8, 1);

    endSingleTimeCommands(context.device, context.commandPool, context.graphicsQueue, cmd);

    transitionImageLayout(
        context.device, 
        context.commandPool,
        context.graphicsQueue, 
        brdfLUT.image, 
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        1,
        1
    );

    return brdfLUT;
}

void IBLProcessor::destroy() {
    vkDeviceWaitIdle(context.device);
    vkDestroyPipeline(context.device, eqToCubePipeline, nullptr);
    vkDestroyPipelineLayout(context.device, hdrPipelineLayout, nullptr);
    vkDestroyDescriptorPool(context.device, hdrDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(context.device, hdrDescriptorLayout, nullptr);
    vkDestroyRenderPass(context.device, renderPass, nullptr);
    vkDestroySampler(context.device, hdrSampler, nullptr);

    vkDestroyPipeline(context.device, irradiancePipeline, nullptr);
    vkDestroyPipeline(context.device, prefilterPipeline, nullptr);
    vkDestroyPipeline(context.device, brdfPipeline, nullptr);
    vkDestroyDescriptorSetLayout(context.device, computeDSL, nullptr);
    vkDestroySampler(context.device, cubeSampler, nullptr);



    vkDestroyPipelineLayout(context.device, computePL, nullptr);
}