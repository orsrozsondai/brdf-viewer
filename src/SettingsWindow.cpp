#include "SettingsWindow.hpp"
#include "Camera.hpp"
#include "Config.hpp"
#include "Scene.hpp"
#include "UniformBufferObjects.hpp"
#include "Object.hpp"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "RenderContext.hpp"
#include <GLFW/glfw3.h>
#include <cfloat>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

SettingsWindow::SettingsWindow(const RenderContext& context) : context(context) {
    init();
}

void SettingsWindow::init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    io = &ImGui::GetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io->IniFilename = nullptr;


    ImGui_ImplGlfw_InitForVulkan(context.window, true);

    

    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * 11;
    pool_info.poolSizeCount = 11;
    pool_info.pPoolSizes = pool_sizes;
    
    if (vkCreateDescriptorPool(context.device, &pool_info, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }

    initStyle();
    
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = context.instance;
    init_info.PhysicalDevice = context.physicalDevice;
    init_info.Device = context.device;
    init_info.Queue = context.graphicsQueue;
    init_info.DescriptorPool = descriptorPool;
    init_info.MinImageCount = context.imageCount;
    init_info.ImageCount = context.imageCount;
    init_info.PipelineInfoMain.RenderPass = context.renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = context.samples;
    ImGui_ImplVulkan_Init(&init_info);

}

void SettingsWindow::initStyle() {
    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 3;
    config.PixelSnapH = false;
    
    io->Fonts->AddFontFromFileTTF("res/fonts/OpenSans-B9K8.ttf", 18, &config);

    ImGuiStyle& style = ImGui::GetStyle();
    style.ItemSpacing.y = 1;

    // float xscale, yscale;
    // glfwGetWindowContentScale(context.window, &xscale, &yscale);
    // float dpiScale = (xscale + yscale) * 0.5f;
    // io->FontGlobalScale = dpiScale;
}

void SettingsWindow::draw(VkCommandBuffer cmd) {
    if (!visible) return;
    if (!scene) return;
    ImGui_ImplVulkan_RenderDrawData(
        ImGui::GetDrawData(),
        cmd
    );
}

static void imguiTooltip(const char* s) {
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", s);
    }
}

void SettingsWindow::update() {
    if (!visible) return;
    if (!scene) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    int width, height;
    glfwGetWindowSize(context.window, &width, &height);
    ImGui::SetNextWindowSize({300, (float)height});
    ImGui::SetNextWindowPos({(float)width-300, 0});

    ImGui::Begin("Settings",nullptr,ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

    MaterialUBO* materialUbo = scene->selectedObject()->ubo();
    SceneUBO* sceneUBO = scene->ubo();
    bool interpolatedObject = scene->isObjectInterpolated();
    static int objc = 3;
    static float objd = 2;
    static const char* lightTypes[] = {"directional", "positional"};
    static int type = (int)sceneUBO->lightPos.w;
    static glm::vec2 lightDir(0,0);
    static glm::vec3 lightColor(1,1,1);
    static float lightIntensity = 1;
    static const char* BRDFNames[] = {"None", "Lambert", "Oren-Nayar", "Burley", "None", "Blinn-Phong", "Cook-Torrance", "Ward", "Disney"};
    static int diffuse = 0, specular = 0;
    static int cameraFov = 45;
    static int meshIndex = 0;
    static const auto meshNames = scene->getMeshNames();
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushItemWidth(-FLT_MIN);

    ImGui::SeparatorText("Scene");
    ImGui::Text("Object count:");
    if (ImGui::SliderInt("##obj_count", &objc, 1, MAX_OBJECT_COUNT)) {
        scene->setObjectCount(objc);
    }
    ImGui::Text("Mesh:");
    if (ImGui::Combo("##mesh", &meshIndex, meshNames.data(), meshNames.size())) {
        scene->setMeshIndex(meshIndex);
    }
    ImGui::Text("Object distance:");
    if (ImGui::SliderFloat("##obj_dist", &objd, 1.f, 10.f)) {
        scene->setObjectDistance(objd);
    }
    ImGui::Text("Camera FOV:");
    if (ImGui::SliderInt("##fov", &cameraFov, 30, 90)) {
        scene->getCamera()->setFov((float)cameraFov);
    }
    ImGui::Text("BRDF:");
    ImGui::PopItemWidth();
    if (ImGui::Combo("Diffuse", &diffuse, BRDFNames, 4)) {
        sceneUBO->brdf = (1 << diffuse) | (1 << (specular + 4));
        scene->reloadShaders();
    }
    if (ImGui::Combo("Specular", &specular, &BRDFNames[4], 5)) {
        sceneUBO->brdf = (1 << diffuse) | (1 << (specular + 4));
        scene->reloadShaders();
    }


    ImGui::PushItemWidth(-FLT_MIN);
    static bool changed_param = false;

    ImGui::SeparatorText("Material");
    
    ImGui::PushItemWidth(- ImGui::GetFrameHeight() - style.FramePadding.x);


    ImGui::BeginDisabled(interpolatedObject & interpolatedParameters[ALBEDO]);
    changed_param = false;
    ImGui::Text("Color:");
    changed_param = changed_param | ImGui::ColorEdit3("##albedo", glm::value_ptr(materialUbo->albedo));
    ImGui::SameLine(0, style.FramePadding.x);
    ImGui::EndDisabled();
    ImGui::Checkbox("##albedo_lerp", &interpolatedParameters[ALBEDO]);
    imguiTooltip("Interpolate");

    ImGui::BeginDisabled(interpolatedObject & interpolatedParameters[ROUGHNESS]);
    ImGui::Text("Roughness:");
    changed_param = changed_param | ImGui::SliderFloat("##roughness", &materialUbo->roughness, 0.0f, 1.0f);
    ImGui::SameLine(0, style.FramePadding.x);
    ImGui::EndDisabled();
    ImGui::Checkbox("##roughness_lerp", &interpolatedParameters[ROUGHNESS]);
    imguiTooltip("Interpolate");
    
    ImGui::BeginDisabled(interpolatedObject & interpolatedParameters[METALLIC]);
    ImGui::Text("Metallic:");
    changed_param = changed_param | ImGui::SliderFloat("##metallic", &materialUbo->metallic, 0.0f, 1.0f);
    ImGui::SameLine(0, style.FramePadding.x);
    ImGui::EndDisabled();
    ImGui::Checkbox("##metallic_lerp", &interpolatedParameters[METALLIC]);
    imguiTooltip("Interpolate");

    ImGui::BeginDisabled(!(sceneUBO->brdf & BRDF_SPECULAR_DISNEY));

    ImGui::BeginDisabled(interpolatedObject & interpolatedParameters[SHEEN]);
    ImGui::Text("Sheen:");
    changed_param = changed_param | ImGui::SliderFloat("##sheen", &materialUbo->sheen, 0.0f, 1.0f);
    ImGui::SameLine(0, style.FramePadding.x);
    ImGui::EndDisabled();
    ImGui::Checkbox("##sheen_lerp", &interpolatedParameters[SHEEN]);
    imguiTooltip("Interpolate");

    ImGui::BeginDisabled(interpolatedObject & interpolatedParameters[SHEEN_TINT]);
    ImGui::Text("Sheen Tint:");
    changed_param = changed_param | ImGui::SliderFloat("##sheen_tint", &materialUbo->sheenTint, 0.0f, 1.0f);
    ImGui::SameLine(0, style.FramePadding.x);
    ImGui::EndDisabled();
    ImGui::Checkbox("##sheen_tint_lerp", &interpolatedParameters[SHEEN_TINT]);
    imguiTooltip("Interpolate");

    ImGui::BeginDisabled(interpolatedObject & interpolatedParameters[CLEARCOAT]);
    ImGui::Text("Clearcoat:");
    changed_param = changed_param | ImGui::SliderFloat("##clearcoat", &materialUbo->clearcoat, 0.0f, 1.0f);
    ImGui::SameLine(0, style.FramePadding.x);
    ImGui::EndDisabled();
    ImGui::Checkbox("##clearcoat_lerp", &interpolatedParameters[CLEARCOAT]);
    imguiTooltip("Interpolate");

    ImGui::BeginDisabled(interpolatedObject & interpolatedParameters[CLEARCOAT_GLOSS]);
    ImGui::Text("Clearcoat Gloss:");
    changed_param = changed_param | ImGui::SliderFloat("##clearcoat_gloss", &materialUbo->clearcoatGloss, 0.0f, 1.0f);
    ImGui::SameLine(0, style.FramePadding.x);
    ImGui::EndDisabled();
    ImGui::Checkbox("##clearcoat_gloss_lerp", &interpolatedParameters[CLEARCOAT_GLOSS]);
    imguiTooltip("Interpolate");

    ImGui::EndDisabled();

    if (ImGui::Button("Apply to all")) {
        scene->applySettingsToAll();
    }

    if (changed_param) {
        for (size_t i = 0; i < interpolatedParameters.size(); i++) {
            if (interpolatedParameters[i]) scene->interpolate((MaterialParameters)i);
        }
    }
    ImGui::PopItemWidth();
    
    

    ImGui::SeparatorText("Light");

    ImGui::Checkbox("Use IBL", (bool*) &sceneUBO->ibl);

    ImGui::BeginDisabled(sceneUBO->ibl);

    ImGui::Text("Type:");
    if (ImGui::Combo("##light_type", &type, lightTypes, IM_ARRAYSIZE(lightTypes))) {
        sceneUBO->lightPos.w = (float)type;
    }
    ImGui::Text("Color:");
    if (ImGui::ColorEdit3("##light_color", glm::value_ptr(lightColor))) {
        sceneUBO->lightColor = lightColor * lightIntensity;
    }
    ImGui::Text("Intensity:");
    if (ImGui::SliderFloat("##light_intensity", &lightIntensity, 0, 10)) {
        sceneUBO->lightColor = lightColor * lightIntensity;
    }
    if (type) {
        ImGui::Text("Position (x,y,z):");
        ImGui::SliderFloat3("##light_pos", glm::value_ptr(sceneUBO->lightPos), -100.0f, 100.0f, "%.0f");
    }
    else {
        ImGui::Text("Direction (Azimuth/Inclination):");
        float itemWidth = ImGui::CalcItemWidth() - style.FramePadding.x;
        ImGui::PushItemWidth(itemWidth/2.0);
        if (ImGui::SliderFloat("##light_dir_azimuth", &lightDir.x, 0.0f, 360.0f, "%.0f")) {
            float phi = glm::radians(lightDir.y);
            float theta = glm::radians(lightDir.x);
            sceneUBO->lightPos.z = -sin(phi) * cos(theta);
            sceneUBO->lightPos.y = -cos(phi);
            sceneUBO->lightPos.x = sin(phi) * sin(theta);
        };
        ImGui::SameLine(0,style.FramePadding.x);
        if (ImGui::SliderFloat("##light_dir_inclination", &lightDir.y, 0.0f, 180.0f, "%.0f")) {
            float phi = glm::radians(lightDir.y);
            float theta = glm::radians(lightDir.x);
            sceneUBO->lightPos.z = -sin(phi) * cos(theta);
            sceneUBO->lightPos.y = -cos(phi);
            sceneUBO->lightPos.x = sin(phi) * sin(theta);
        };
        ImGui::PopItemWidth();
    }
    ImGui::Text("Ambient:");
    ImGui::ColorEdit3("##ambientlight", glm::value_ptr(sceneUBO->ambientLight));

    ImGui::EndDisabled();

    ImGui::SeparatorText("Post processing");
    ImGui::Checkbox("Tone Mapping", (bool*)&sceneUBO->toneMapping);
    ImGui::BeginDisabled(!sceneUBO->toneMapping);
    ImGui::Text("Exposure:");
    ImGui::SliderFloat("##exposure", &sceneUBO->exposure, 0, 2);
    ImGui::EndDisabled();

    float textHeight = ImGui::GetTextLineHeightWithSpacing();

    ImGui::SetCursorPosY(fmax(ImGui::GetCursorPosY(),height - textHeight - ImGui::GetStyle().WindowPadding.y));

    ImGui::Text("Window size: %dx%d, FPS: %6.2f",width, height, io->Framerate);
    ImGui::PopItemWidth();

    ImGui::End();

    ImGui::Render();
}

void SettingsWindow::setControlledObject(Object* pObject) {
    object = pObject;
}

SettingsWindow::~SettingsWindow() {
    vkDeviceWaitIdle(context.device);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);
}

void SettingsWindow::toggle() {
    visible = !visible;
}

void SettingsWindow::setScene(Scene* pScene) {
    scene = pScene;
}