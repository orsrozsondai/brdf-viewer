#include "App.hpp"
#include "Pipeline.hpp"
#include "Scene.hpp"
#include "SettingsWindow.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <sys/types.h>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include "Config.hpp"
#include "helpers.hpp"


void App::initInstance(const char* appName) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = count;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = layers;


    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

}
void App::initGLFW(const char* appName, int width, int height) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (width <= 0 || height <= 0) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        width = mode->width;
        height = mode->height;
    }
    else {
        monitor = nullptr;
    }
    window = glfwCreateWindow(width, height, appName, monitor, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);
    dragCursor = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
}

void App::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void App::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    
    App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
    app->handleKey(key, scancode, action, mods);
}

void App::handleKey(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (mods & GLFW_MOD_CONTROL) {
            if (key == GLFW_KEY_R) scene->reloadShaders();
            if (key == GLFW_KEY_M) settingsWindow->toggle();
        }
        if (ImGui::GetCurrentContext()) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantCaptureKeyboard)
                return;
        }
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
            scene->selectObject(key-GLFW_KEY_0);
        }
        if (key == GLFW_KEY_LEFT) scene->cycleSelected(-1);
        if (key == GLFW_KEY_RIGHT) scene->cycleSelected(1);
        
    }
}

void App::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
            return;
    }
    App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
    app->handleMouseInput(xpos, ypos);
} 

void App::handleMouseInput(double xpos, double ypos) {
    if (!rotatingCamera)
    {
        firstMouse = true;
        return;
    }
    if (firstMouse) {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = (float)ypos - lastY;

    lastX = (float)xpos;
    lastY = (float)ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;
    camera->rotate(xoffset, yoffset);
}

void App::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
            return;
    }
    App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
    app->handleScroll(xoffset, yoffset);
}
void App::handleScroll(double xoffset, double yoffset) {
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)

        scene->cycleSelected(yoffset);
    
    else camera->zoom(float(yoffset)*0.5f);
}
void App::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
            return;
    }
    App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
    app->handleMouseButton(button, action);
}


void App::handleMouseButton(int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            rotatingCamera = true;
            glfwSetCursor(window, dragCursor);
            
        }
        else if (action == GLFW_RELEASE)
        {
            rotatingCamera = false;
            glfwSetCursor(window, nullptr);
        }
    }
}

void App::initSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void App::selectDevice() {
    physicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("no GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    physicalDevice = devices[0];

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        nullptr
    );

    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        families.data()
    );

    graphicsFamily = -1;
    presentFamily = -1;

    for (uint32_t i = 0; i < queueFamilyCount; i++) {

        bool graphics = families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        bool compute = families[i].queueFlags & VK_QUEUE_COMPUTE_BIT;

        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            physicalDevice,
            i,
            surface,
            &present
        );

        if (graphics && compute && present) {
            graphicsFamily = i;
            presentFamily  = i;
            break;
        }

        if (graphics && graphicsFamily == -1)
            graphicsFamily = i;

        if (present && presentFamily == -1)
            presentFamily = i;
    }

    if (graphicsFamily == -1 || presentFamily == -1) {
        throw std::runtime_error("required queue families not found!");
    }
    std::vector<uint32_t> uniqueFamilies;

    uniqueFamilies.push_back(graphicsFamily);
    
    if (presentFamily != graphicsFamily) {
        uniqueFamilies.push_back(presentFamily);
    }

    // create logical device
    float priority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    

    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo q{};
        q.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        q.queueFamilyIndex = family;
        q.queueCount = 1;
        q.pQueuePriorities = &priority;

        queueInfos.push_back(q);
    }
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // sampler anisotropy
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

    if (!supportedFeatures.samplerAnisotropy) {
        std::cout << "Anisotropic filtering is not supported" << std::endl;
    }
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = supportedFeatures.samplerAnisotropy;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueInfos.size();
    createInfo.pQueueCreateInfos = queueInfos.data();

    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    createInfo.pEnabledFeatures = &deviceFeatures;


    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }
    // get queues

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);

    
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);

    std::cout << "Selected GPU: " << props.deviceName << std::endl;

}

VkSampleCountFlagBits App::getMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

void App::createSwapchain() {
    struct SwapchainSupport {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapchainSupport support;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice, surface, &support.capabilities
    );

    uint32_t count;

    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, surface, &count, nullptr
    );
    support.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, surface, &count, support.formats.data()
    );

    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, surface, &count, nullptr
    );
    support.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, surface, &count, support.presentModes.data()
    );

    // choose format
    VkSurfaceFormatKHR format = support.formats[0];
    for (const auto& f : support.formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = f;
            break;
        }
    }
    swapchainFormat = format.format;
    // choose present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync
    // for (auto m : support.presentModes) {
    //     if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
    //         presentMode = m; // triple buffering
    //         break;
    //     }

    // }

    // choose extent
    VkExtent2D extent;
    if (support.capabilities.currentExtent.width != UINT32_MAX) {
        extent = support.capabilities.currentExtent;
    }
    else {

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        extent = {
            (uint32_t)w,
            (uint32_t)h
        };

        extent.width = std::clamp(
            extent.width,
            support.capabilities.minImageExtent.width,
            support.capabilities.maxImageExtent.width
        );
        extent.height = std::clamp(
            extent.height,
            support.capabilities.minImageExtent.height,
            support.capabilities.maxImageExtent.height
        );
    }
    swapchainExtent = extent;


    imageCount = support.capabilities.minImageCount + 1;

    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }    


    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;

    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilies[] = {
        (uint32_t)graphicsFamily,
        (uint32_t)presentFamily
    };

    if (graphicsFamily != presentFamily) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = queueFamilies;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;



    if (vkCreateSwapchainKHR(device, &info, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swapchain!");
    }

}

void App::createImageViews() {

    imageViews = std::vector<VkImageView>(imageCount);
    std::vector<VkImage> images(imageCount);

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());
    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        info.image = images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = swapchainFormat;

        info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &info, nullptr, &imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }
    }

}

void App::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat;
    colorAttachment.samples = msaaSamples;

    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription resolveAttachment{};
    resolveAttachment.format = swapchainFormat;
    resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference resolveRef{};
    resolveRef.attachment = 2;
    resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;
    subpass.pResolveAttachments = &resolveRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;

    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments{};
    attachments[0] = colorAttachment;
    attachments[1] = depthAttachment;
    attachments[2] = resolveAttachment;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    info.attachmentCount = (uint32_t)attachments.size();
    info.pAttachments = attachments.data();

    info.subpassCount = 1;
    info.pSubpasses = &subpass;

    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &info, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

}

void App::createColorResources() {
    colorImages.resize(imageCount);
    colorImageMemories.resize(imageCount);
    colorImageViews.resize(imageCount);

    for (size_t i = 0; i < imageCount; i++) {

        createImage(
            device,
            physicalDevice,
            swapchainExtent.width,
            swapchainExtent.height,
            msaaSamples,
            swapchainFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            colorImages[i],
            colorImageMemories[i]
        );

        
        
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = colorImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat;

        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &colorImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }
    }
}

void App::createDepthResources() {
    std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkFormat selFormat;
    bool foundFormat = false;
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            selFormat = format;
            foundFormat = true;
            break;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            selFormat = format;
            foundFormat = true;
            break;
        }
    }

    if (!foundFormat) throw std::runtime_error("failed to find supported format!");

    depthImages = std::vector<VkImage>(imageCount);
    depthImageMemories = std::vector<VkDeviceMemory>(imageCount);
    depthImageViews = std::vector<VkImageView>(imageCount);

    for (size_t i = 0; i < imageCount; i++) {

        createImage(
            device,
            physicalDevice,
            swapchainExtent.width,
            swapchainExtent.height,
            msaaSamples,
            selFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            depthImages[i],
            depthImageMemories[i]
        );


        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = selFormat;

        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }
    }

}

void App::createFramebuffers() {
    framebuffers = std::vector<VkFramebuffer>(imageViews.size());
    for (size_t i = 0; i < imageViews.size(); i++) {
        VkImageView attachments[] = {
            colorImageViews[i],
            depthImageViews[i],
            imageViews[i]
        };

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

        info.renderPass = renderPass;
        info.attachmentCount = 3;
        info.pAttachments = attachments;

        info.width  = swapchainExtent.width;
        info.height = swapchainExtent.height;
        info.layers = 1;

        if (vkCreateFramebuffer(device, &info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}


void App::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

}

void App::createCommandBuffers() {
    commandBuffers = std::vector<VkCommandBuffer>(framebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}
void App::recordCommands() {

    for (size_t i = 0; i < commandBuffers.size(); i++) {
        recordCommandBuffer(commandBuffers[i], i);
        
    }

}
void App::recordCommandBuffer(VkCommandBuffer cmd, int imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { { bgcolor.x, bgcolor.y, bgcolor.z, 1.0}};
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderInfo.renderPass = renderPass;
    renderInfo.framebuffer = framebuffers[imageIndex];
    renderInfo.renderArea.offset = {0, 0};
    renderInfo.renderArea.extent = swapchainExtent;
    renderInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderInfo.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(
        cmd,
        &renderInfo,
        VK_SUBPASS_CONTENTS_INLINE
    );
    
    // scene->getPipeline()->bind(cmd, swapchainExtent);
    
    scene->draw(cmd, swapchainExtent, imageIndex);
    
    if (settingsWindow != nullptr) {
        settingsWindow->draw((cmd));
    }
    vkCmdEndRenderPass(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void App::createSyncObjects() {

    imageAvailable = std::vector<VkSemaphore>(imageCount);
    renderFinished = std::vector<VkSemaphore>(imageCount);
    inFlightFences = std::vector<VkFence>(imageCount);
    imagesInFlight = std::vector<VkFence>(imageCount);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < (int)imageCount; i++) {
        vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailable[i]);
        vkCreateSemaphore(device, &semInfo, nullptr, &renderFinished[i]);
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
    }
    for (size_t i = 0; i < (size_t)imageCount; i++) {
        imagesInFlight[i] = VK_NULL_HANDLE;
    }
    

}

void App::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = imageCount * MAX_OBJECT_COUNT * 4 + imageCount;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 3;


    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = imageCount * MAX_OBJECT_COUNT*2 + imageCount;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to creater descriptor pool!");
    }
}

App::App(const char* appName, const glm::vec2& windowSize) : framebufferResized(false) {
    initGLFW(appName, windowSize.x, windowSize.y);
    initInstance(appName);
    initSurface();
    selectDevice();
    // check samplecount support
    VkSampleCountFlagBits max = getMaxUsableSampleCount();
    if (max < msaaSamples)
        msaaSamples = max;
    std::cout << "Selected msaa sample count: " << msaaSamples << std::endl;
    createSwapchain();
    createImageViews();
    createRenderPass();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
    createDescriptorPool();
    
    this->settingsWindow = new SettingsWindow(getRenderContext());
}
void App::destroy() {
    if (dragCursor) glfwDestroyCursor(dragCursor);
    vkDeviceWaitIdle(device);
    delete settingsWindow;
    scene->destroy();
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    for (int i = 0; i < (int)imageCount; i++) {
        vkDestroySemaphore(device, imageAvailable[i], nullptr);
        vkDestroySemaphore(device, renderFinished[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }
    vkDestroyCommandPool(device, commandPool, nullptr);
    for (VkFramebuffer fb : framebuffers) {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (VkImageView view : imageViews) {
        vkDestroyImageView(device, view, nullptr);
    }
    for (size_t i = 0; i < imageCount; i++) {
        vkDestroyImageView(device, depthImageViews[i], nullptr);
        vkDestroyImage(device, depthImages[i], nullptr);
        vkFreeMemory(device, depthImageMemories[i], nullptr);
    }
    for (size_t i = 0; i < colorImages.size(); i++) {
        vkDestroyImageView(device, colorImageViews[i], nullptr);
        vkDestroyImage(device, colorImages[i], nullptr);
        vkFreeMemory(device, colorImageMemories[i], nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);

    glfwTerminate();

}
void App::run() {
    glfwShowWindow(window);
    
    while (!glfwWindowShouldClose(window)) {


        glfwPollEvents();
        
        if (settingsWindow != nullptr) {
            settingsWindow->update();
        }
        
        scene->update();

        

        drawFrame();
    }

}

void App::drawFrame() {
    VkResult result;

    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    result = vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX,
        imageAvailable[currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailable[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = { renderFinished[currentFrame] };

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(
        graphicsQueue,
        1,
        &submitInfo,
        inFlightFences[currentFrame]
    );

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
    }

    currentFrame = (currentFrame + 1) % imageCount;

}

void App::recreateSwapchain() {
    int width = 0, height = 0;

    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    for (auto fb : framebuffers)
        vkDestroyFramebuffer(device, fb, nullptr);

    for (auto view : imageViews)
        vkDestroyImageView(device, view, nullptr);

    for (size_t i = 0; i < imageCount; i++) {
        vkDestroyImageView(device, depthImageViews[i], nullptr);
        vkDestroyImage(device, depthImages[i], nullptr);
        vkFreeMemory(device, depthImageMemories[i], nullptr);
    }
    for (size_t i = 0; i < colorImages.size(); i++) {
        vkDestroyImageView(device, colorImageViews[i], nullptr);
        vkDestroyImage(device, colorImages[i], nullptr);
        vkFreeMemory(device, colorImageMemories[i], nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain, nullptr);


    createSwapchain();
    createImageViews();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    recordCommands();
    camera->setAspectRatio((float)swapchainExtent.width/(float)swapchainExtent.height);

}

RenderContext App::getRenderContext() const {
    return RenderContext{
        instance,
        window,
        device,
        physicalDevice,
        renderPass,
        msaaSamples,
        graphicsQueue,
        commandPool, 
        imageCount,
        descriptorPool
    };
}

VkExtent2D App::getSwapchainExtent() const {
    return swapchainExtent;
}

VkQueue App::getGraphicsQueue() const {
    return graphicsQueue;
}

void App::setScene(Scene* pScene) {
    scene = pScene;
    settingsWindow->setScene(pScene);
}

void App::setCamera(Camera* pCamera) {
    camera = pCamera;
}

Camera* App::getCamera() const {
    return camera;
}

void App::addSettingsWindow(SettingsWindow* pSettingsWindow) {
    settingsWindow = pSettingsWindow;
}