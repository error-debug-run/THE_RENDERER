#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "Renderer.h"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ─── Constants ────────────────────────────────────────────────────────────────
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

const std::vector<Vertex> vertices = {
    {{ 0.0f,  0.667f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.333f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, -0.333f, 0.0f}, {0.0f, 0.0f, 1.0f}}
};

// ─── Debug Messenger ──────────────────────────────────────────────────────────
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*)
{
    std::cerr << "[Validation] " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCI,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) return func(instance, pCI, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func) func(instance, messenger, pAllocator);
}

// ═════════════════════════════════════════════════════════════════════════════
//  PUBLIC
// ═════════════════════════════════════════════════════════════════════════════
void Renderer::init(GLFWwindow* win) {
    window = win;
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createVMAAllocator();
    createVertexBuffer();
    createUniformBuffers();
    createDepthBuffer();
    createFramebuffers();
    createDescriptorSetLayout();
    createPipeline();
    createCommandPool();
    createCommandBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createSyncObjects();
    std::cout << "Renderer initialized" << std::endl;
}

void Renderer::drawFrame(const Scene& scene) {
    float aspect = (float)extent.width / (float)extent.height;

    // ── build vertices from scene entities ──
    std::vector<Vertex> verts;
    verts.reserve(scene.entities.size());
    for (auto& e : scene.entities) {
        Vertex v;
        v.pos = e.position;
        v.color = glm::vec3(1.0f, 1.0f, 1.0f);  // white for now
        verts.push_back(v);
    }

    // upload to vertex buffer
    if (!verts.empty()) {
        void* data;
        vmaMapMemory(vmaAllocator, vertexAllocation, &data);
        memcpy(data, verts.data(), sizeof(Vertex) * verts.size());
        vmaUnmapMemory(vmaAllocator, vertexAllocation);
    }

    // ── UBO — model is identity, camera does the work ──
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = scene.camera.view(aspect);
    ubo.proj = scene.camera.projection(aspect);
    ubo.proj[1][1] *= -1;
    memcpy(uniformMapped[currentFrame], &ubo, sizeof(ubo));

    // ── Frame sync ──────────────────────────────────────────────────────────
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
        acquireSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // ── Record ──────────────────────────────────────────────────────────────
    VkCommandBuffer cmd = commandBuffers[currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &beginInfo);

    std::array< VkClearValue, 2 > clearValues{};
    clearValues[0].color = { {0.01f, 0.01f, 0.01f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = renderPass;
    rpBegin.framebuffer = framebuffers[imageIndex];
    rpBegin.renderArea.offset = { 0, 0 };
    rpBegin.renderArea.extent = extent;
    rpBegin.clearValueCount = (uint32_t)clearValues.size();
    rpBegin.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
    VkBuffer     vbufs[] = { vertexBuffer };
    VkDeviceSize voffsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, voffsets);
    vkCmdDraw(cmd, (uint32_t)verts.size(), 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // ── Submit ──────────────────────────────────────────────────────────────
    VkSemaphore waitSems[] = { acquireSemaphores[currentFrame] };
    VkSemaphore signalSems[] = { renderFinishedSemaphores[imageIndex] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1; submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1; submitInfo.pSignalSemaphores = signalSems;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);

    // ── Present ─────────────────────────────────────────────────────────────
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1; presentInfo.pWaitSemaphores = signalSems;
    presentInfo.swapchainCount = 1; presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    vkQueuePresentKHR(graphicsQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % framesInFlight;
}

void Renderer::cleanup() {
    vkDeviceWaitIdle(device);

    vkDestroyImageView(device, depthImageView, nullptr);
    vmaDestroyImage(vmaAllocator, depthImage, depthAllocation);

    for (int i = 0; i < (int)framesInFlight; i++)
        vkDestroySemaphore(device, acquireSemaphores[i], nullptr);
    for (size_t i = 0; i < swapImageCount; i++)
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
    for (int i = 0; i < (int)framesInFlight; i++)
        vkDestroyFence(device, inFlightFences[i], nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    for (int i = 0; i < (int)framesInFlight; i++)
        vmaDestroyBuffer(vmaAllocator, uniformBuffers[i], uniformAllocations[i]);

    vkDestroyCommandPool(device, commandPool, nullptr);
    vmaDestroyBuffer(vmaAllocator, vertexBuffer, vertexAllocation);
    vmaDestroyAllocator(vmaAllocator);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    for (auto fb : framebuffers)     vkDestroyFramebuffer(device, fb, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (auto iv : swapImageViews)   vkDestroyImageView(device, iv, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    if (enableValidationLayers)
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    vkDestroyInstance(instance, nullptr);
}

// ═════════════════════════════════════════════════════════════════════════════
//  PRIVATE — INIT
// ═════════════════════════════════════════════════════════════════════════════
void Renderer::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport())
        throw std::runtime_error("Validation layers not available");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "THE_RENDERER";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "CAP";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector< const char*> extensions(glfwExts, glfwExts + glfwExtCount);
    if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkDebugUtilsMessengerCreateInfoEXT debugCI{};
    debugCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCI.pfnUserCallback = debugCallback;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = (uint32_t)extensions.size();
    ci.ppEnabledExtensionNames = extensions.data();
    if (enableValidationLayers) {
        ci.enabledLayerCount = (uint32_t)validationLayers.size();
        ci.ppEnabledLayerNames = validationLayers.data();
        ci.pNext = &debugCI;
    }

    if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance");

    if (enableValidationLayers)
        CreateDebugUtilsMessengerEXT(instance, &debugCI, nullptr, &debugMessenger);

    std::cout << "Vulkan instance created | Validation: " << (enableValidationLayers ? "ON" : "OFF") << std::endl;
}

void Renderer::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
}

void Renderer::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (!count) throw std::runtime_error("No Vulkan GPU found");
    std::vector< VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    for (const auto& pd : devices)
        if (isDeviceSuitable(pd)) { physicalDevice = pd; break; }
    if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No suitable GPU");

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "GPU: " << props.deviceName << std::endl;
}

void Renderer::createLogicalDevice() {
    queueIndices = findQueueFamilies(physicalDevice);
    std::vector< VkDeviceQueueCreateInfo> queueCIs;
    float priority = 1.0f;
    for (uint32_t qf : std::set< uint32_t>{ queueIndices.graphicsFamily.value(), queueIndices.presentFamily.value() }) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = qf; qi.queueCount = 1; qi.pQueuePriorities = &priority;
        queueCIs.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};
	features.largePoints = VK_TRUE;
    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = (uint32_t)queueCIs.size();
    ci.pQueueCreateInfos = queueCIs.data();
    ci.pEnabledFeatures = &features;
    ci.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    ci.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &ci, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device");

    vkGetDeviceQueue(device, queueIndices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, queueIndices.presentFamily.value(), 0, &presentQueue);
    std::cout << "Logical device created" << std::endl;
}

void Renderer::createSwapChain() {
    SwapChainSupportDetails sc = querySwapChainSupport(physicalDevice);
    swapSurfaceFormat = chooseSwapSurfaceFormat(sc.formats);
    VkPresentModeKHR mode = chooseSwapPresentMode(sc.presentModes);
    extent = chooseSwapExtent(sc.capabilities);

    uint32_t imgCount = sc.capabilities.minImageCount + 1;
    if (sc.capabilities.maxImageCount > 0 && imgCount > sc.capabilities.maxImageCount)
        imgCount = sc.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surface;
    ci.minImageCount = imgCount;
    ci.imageFormat = swapSurfaceFormat.format;
    ci.imageColorSpace = swapSurfaceFormat.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    uint32_t qfArr[] = { queueIndices.graphicsFamily.value(), queueIndices.presentFamily.value() };
    if (queueIndices.graphicsFamily != queueIndices.presentFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2; ci.pQueueFamilyIndices = qfArr;
    }
    else { ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
    ci.preTransform = sc.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = mode;
    ci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swap chain");

    vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, nullptr);
    swapImages.resize(swapImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, swapImages.data());
    framesInFlight = swapImageCount;

    std::cout << "Swap chain: " << extent.width << "x" << extent.height
        << " | " << (mode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO") << std::endl;
}

void Renderer::createImageViews() {
    swapImageViews.resize(swapImageCount);
    for (size_t i = 0; i < swapImageCount; i++) {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = swapImages[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = swapSurfaceFormat.format;
        vi.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.baseMipLevel = 0; vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.baseArrayLayer = 0; vi.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &vi, nullptr, &swapImageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image view");
    }
    std::cout << "Image views: " << swapImageCount << std::endl;
}

void Renderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapSurfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array< VkAttachmentDescription, 2 > attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = (uint32_t)attachments.size(); ci.pAttachments = attachments.data();
    ci.subpassCount = 1;                             ci.pSubpasses = &subpass;
    ci.dependencyCount = 1;                             ci.pDependencies = &dep;

    if (vkCreateRenderPass(device, &ci, nullptr, &renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
    std::cout << "Render pass created" << std::endl;
}

void Renderer::createVMAAllocator() {
    VmaAllocatorCreateInfo ci{};
    ci.physicalDevice = physicalDevice;
    ci.device = device;
    ci.instance = instance;
    vmaCreateAllocator(&ci, &vmaAllocator);
}

void Renderer::createVertexBuffer() {
    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = sizeof(vertices[0]) * MAX_ENTITIES;
    ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    vmaCreateBuffer(vmaAllocator, &ci, &aci, &vertexBuffer, &vertexAllocation, nullptr);

    void* data;
    vmaMapMemory(vmaAllocator, vertexAllocation, &data);
    memcpy(data, vertices.data(), (size_t)ci.size);
    vmaUnmapMemory(vmaAllocator, vertexAllocation);
    std::cout << "Vertex buffer created" << std::endl;
}

void Renderer::createUniformBuffers() {
    uniformBuffers.resize(framesInFlight);
    uniformAllocations.resize(framesInFlight);
    uniformMapped.resize(framesInFlight);

    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = sizeof(UniformBufferObject);
    ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    for (uint32_t i = 0; i < framesInFlight; i++) {
        VmaAllocationInfo info{};
        vmaCreateBuffer(vmaAllocator, &ci, &aci, &uniformBuffers[i], &uniformAllocations[i], &info);
        uniformMapped[i] = info.pMappedData;
    }
    std::cout << "Uniform buffers created" << std::endl;
}

void Renderer::createDepthBuffer() {
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = VK_FORMAT_D32_SFLOAT;
    ci.extent = { extent.width, extent.height, 1 };
    ci.mipLevels = 1; ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(vmaAllocator, &ci, &aci, &depthImage, &depthAllocation, nullptr);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = depthImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_D32_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vi.subresourceRange.baseMipLevel = 0; vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.baseArrayLayer = 0; vi.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &vi, nullptr, &depthImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image view");
    std::cout << "Depth buffer created" << std::endl;
}

void Renderer::createFramebuffers() {
    framebuffers.resize(swapImageCount);
    for (size_t i = 0; i < swapImageCount; i++) {
        std::array< VkImageView, 2 > atts = { swapImageViews[i], depthImageView };
        VkFramebufferCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass = renderPass;
        ci.attachmentCount = (uint32_t)atts.size(); ci.pAttachments = atts.data();
        ci.width = extent.width; ci.height = extent.height; ci.layers = 1;
        if (vkCreateFramebuffer(device, &ci, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");
    }
    std::cout << "Framebuffers: " << swapImageCount << std::endl;
}

void Renderer::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1; ci.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(device, &ci, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout");
}

void Renderer::createPipeline() {
    auto vertCode = readFile("triangle.vert.spv");
    auto fragCode = readFile("triangle.frag.spv");
    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule; vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule; fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    auto bindingDesc = Vertex::getBindingDescription();
    auto attributeDesc = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = (uint32_t)attributeDesc.size();
    vertexInput.pVertexAttributeDescriptions = attributeDesc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkViewport viewport{};
    viewport.x = 0; viewport.y = 0;
    viewport.width = (float)extent.width; viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
    VkRect2D scissor{ {0,0}, extent };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1; viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1; viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAtt{};
    colorBlendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAtt.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1; colorBlending.pAttachments = &colorBlendAtt;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = 1; layoutCI.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &layoutCI, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount = 2; pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pRasterizationState = &rasterizer;
    pipelineCI.pMultisampleState = &multisampling;
    pipelineCI.pColorBlendState = &colorBlending;
    pipelineCI.pDepthStencilState = &depthStencil;
    pipelineCI.layout = pipelineLayout;
    pipelineCI.renderPass = renderPass;
    pipelineCI.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &graphicsPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
    std::cout << "Graphics pipeline created" << std::endl;
}

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = queueIndices.graphicsFamily.value();
    if (vkCreateCommandPool(device, &ci, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(framesInFlight);
    VkCommandBufferAllocateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ci.commandPool = commandPool;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ci.commandBufferCount = framesInFlight;
    if (vkAllocateCommandBuffers(device, &ci, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}

void Renderer::createDescriptorPool() {
    VkDescriptorPoolSize size{};
    size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    size.descriptorCount = framesInFlight;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = 1; ci.pPoolSizes = &size;
    ci.maxSets = framesInFlight;

    if (vkCreateDescriptorPool(device, &ci, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool");
}

void Renderer::createDescriptorSets() {
    std::vector< VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayout);
    VkDescriptorSetAllocateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ci.descriptorPool = descriptorPool;
    ci.descriptorSetCount = framesInFlight;
    ci.pSetLayouts = layouts.data();

    descriptorSets.resize(framesInFlight);
    if (vkAllocateDescriptorSets(device, &ci, descriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor sets");

    for (uint32_t i = 0; i < framesInFlight; i++) {
        VkDescriptorBufferInfo buf{};
        buf.buffer = uniformBuffers[i]; buf.offset = 0; buf.range = sizeof(UniformBufferObject);
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descriptorSets[i]; w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1; w.pBufferInfo = &buf;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }
    std::cout << "Descriptor sets created" << std::endl;
}

void Renderer::createSyncObjects() {
    acquireSemaphores.resize(framesInFlight);
    renderFinishedSemaphores.resize(swapImageCount);
    inFlightFences.resize(framesInFlight);

    VkSemaphoreCreateInfo semCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo     fenCI{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < framesInFlight; i++) {
        if (vkCreateSemaphore(device, &semCI, nullptr, &acquireSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenCI, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create sync objects");
    }
    for (uint32_t i = 0; i < swapImageCount; i++) {
        if (vkCreateSemaphore(device, &semCI, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create render finished semaphore");
    }
    std::cout << "Sync objects created" << std::endl;
}

// ═════════════════════════════════════════════════════════════════════════════
//  PRIVATE — UTILITIES
// ═════════════════════════════════════════════════════════════════════════════
bool Renderer::checkValidationLayerSupport() {
    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector< VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());
    for (const char* name : validationLayers) {
        bool found = false;
        for (const auto& p : available)
            if (strcmp(name, p.layerName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice dev) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector< VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());
    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
        if (present) indices.presentFamily = i;
        if (indices.isComplete()) break;
    }
    return indices;
}

SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice dev) {
    SwapChainSupportDetails d;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &d.capabilities);
    uint32_t fc; vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fc, nullptr);
    if (fc) { d.formats.resize(fc); vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fc, d.formats.data()); }
    uint32_t pc; vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pc, nullptr);
    if (pc) { d.presentModes.resize(pc); vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &pc, d.presentModes.data()); }
    return d;
}

bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice dev) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector< VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, available.data());
    std::set< std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : available) required.erase(ext.extensionName);
    return required.empty();
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice dev) {
    auto idx = findQueueFamilies(dev);
    bool extOk = checkDeviceExtensionSupport(dev);
    bool swapOk = false;
    if (extOk) { auto sc = querySwapChainSupport(dev); swapOk =! sc.formats.empty() && !sc.presentModes.empty(); }
    return idx.isComplete() && extOk && swapOk;
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector< VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
    return formats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector< VkPresentModeKHR>& modes) {
    for (const auto& m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& cap) {
    if (cap.currentExtent.width != UINT32_MAX) return cap.currentExtent;
    int w, h; glfwGetFramebufferSize(window, &w, &h);
    return {
        std::clamp((uint32_t)w, cap.minImageExtent.width,  cap.maxImageExtent.width),
        std::clamp((uint32_t)h, cap.minImageExtent.height, cap.maxImageExtent.height)
    };
}

VkShaderModule Renderer::createShaderModule(const std::vector< char>& code) {
    if (code.empty() || code.size() % 4 != 0) throw std::runtime_error("Invalid SPIR-V");
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast <const uint32_t*> (code.data());
    VkShaderModule module;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");
    return module;
}

std::vector< char> Renderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open: " + filename);
    size_t size = (size_t)file.tellg();
    std::vector< char> buf(size);
    file.seekg(0);
    file.read(buf.data(), size);
    return buf;
}