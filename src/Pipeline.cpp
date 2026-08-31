#include "Pipeline.hpp"
#include "UniformBufferObjects.hpp"
#include "Vertex.hpp"
#include "helpers.hpp"
#include <cstdint>
#include <array>
#include <glm/ext/vector_float3.hpp>
#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan_core.h>


Pipeline::Pipeline(const RenderContext& context, const std::string& vert, const std::string&frag) : context(context), vert(vert), frag(frag) {
    createLayout();
    create();
}

void Pipeline::createLayout() {
    VkDescriptorSetLayoutBinding vsUboLayoutBinding{};
    vsUboLayoutBinding.binding = 0;
    vsUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vsUboLayoutBinding.descriptorCount = 1;
    vsUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutBinding materialUboLayoutBinding{};
    materialUboLayoutBinding.binding = 1;
    materialUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialUboLayoutBinding.descriptorCount = 1;
    materialUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutBinding textureLayoutBindings{};
    textureLayoutBindings.binding = 2;
    textureLayoutBindings.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    textureLayoutBindings.descriptorCount = 4;
    textureLayoutBindings.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    
    VkDescriptorSetLayoutBinding sceneUboLayoutBinding{};
    sceneUboLayoutBinding.binding = 0;
    sceneUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneUboLayoutBinding.descriptorCount = 1;
    sceneUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
        vsUboLayoutBinding,
        materialUboLayoutBinding,
        textureLayoutBindings
    };

    std::array<VkDescriptorSetLayoutBinding, 3> iblBindings;
    iblBindings[0].binding = 0;
    iblBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    iblBindings[0].descriptorCount = 1;
    iblBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    iblBindings[0].pImmutableSamplers = nullptr;
    iblBindings[1].binding = 1;
    iblBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    iblBindings[1].descriptorCount = 1;
    iblBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    iblBindings[1].pImmutableSamplers = nullptr;
    iblBindings[2].binding = 2;
    iblBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    iblBindings[2].descriptorCount = 1;
    iblBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    iblBindings[2].pImmutableSamplers = nullptr;


    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo1{};
    descriptorSetLayoutInfo1.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo1.bindingCount = bindings.size();
    descriptorSetLayoutInfo1.pBindings = bindings.data();

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo2{};
    descriptorSetLayoutInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo2.bindingCount = 1;
    descriptorSetLayoutInfo2.pBindings = &sceneUboLayoutBinding;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo3{};
    descriptorSetLayoutInfo3.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo3.bindingCount = iblBindings.size();
    descriptorSetLayoutInfo3.pBindings = iblBindings.data();

    descriptorSetLayouts.resize(3);

    if (vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutInfo1, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
    if (vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutInfo2, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
    if (vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutInfo3, nullptr, &descriptorSetLayouts[2]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = descriptorSetLayouts.size();
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();

    if (vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void Pipeline::create(BRDF brdf) {
    auto vertCode = readFile("build/shaders/" + vert + ".spv");
    auto fragCode = readFile("build/shaders/" + frag + ".spv");

    VkShaderModule vertModule = createShaderModule(context.device, vertCode);
    VkShaderModule fragModule = createShaderModule(context.device, fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";


    VkSpecializationMapEntry entry;
    entry.constantID = 0;
    entry.offset = 0;
    entry.size = sizeof(BRDF);

    VkSpecializationInfo specInfo;
    specInfo.dataSize = sizeof(BRDF);
    specInfo.pData = &brdf;
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &entry;

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";
    fragStage.pSpecializationInfo = &specInfo;

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex); 
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};

    // POSITION
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);

    // NORMAL
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, normal);

    // TANGENT
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[2].offset = offsetof(Vertex, tangent);


    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = (uint32_t)attributes.size();
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
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

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pDynamicState = &dynamic;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &ms;
    info.pColorBlendState = &blend;
    info.pDepthStencilState = &depthStencil;

    info.layout = pipelineLayout;
    info.renderPass = context.renderPass;
    info.subpass = 0;

    if (vkCreateGraphicsPipelines(
        context.device,
        VK_NULL_HANDLE,
        1,
        &info,
        nullptr,
        &pipeline
    ) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline!");
    }
    vkDestroyShaderModule(context.device, vertModule, nullptr);
    vkDestroyShaderModule(context.device, fragModule, nullptr);

}

void Pipeline::recreate(BRDF brdf) {
    vkDeviceWaitIdle(context.device);
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    create(brdf);
    std::cout << "Pipeline recreated" << std::endl;
}

void Pipeline::bind(VkCommandBuffer cmd, const VkExtent2D& extent) const {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
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
}

void Pipeline::destroy() {
    
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    for (auto descriptorSetLayout : descriptorSetLayouts) {
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);
            descriptorSetLayout = VK_NULL_HANDLE;
        }
    }
}