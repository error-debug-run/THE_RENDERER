#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <optional>
#include <set>
#include <array>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};
        attrs[0].binding = 0; attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset = offsetof(Vertex, pos);
        attrs[1].binding = 0; attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset = offsetof(Vertex, color);
        return attrs;
    }
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
    {{ 0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*)
{
    std::cerr << "[Validation] " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func) func(instance, debugMessenger, pAllocator);
}

bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());
    for (const char* name : validationLayers) {
        bool found = false;
        for (const auto& props : available)
            if (strcmp(name, props.layerName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + filename);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    if (code.empty() || code.size() % 4 != 0) throw std::runtime_error("Invalid SPIR-V");
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");
    return module;
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) indices.presentFamily = i;
        if (indices.isComplete()) break;
    }
    return indices;
}

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails d;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &d.capabilities);
    uint32_t fc; vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fc, nullptr);
    if (fc) { d.formats.resize(fc); vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fc, d.formats.data()); }
    uint32_t pc; vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &pc, nullptr);
    if (pc) { d.presentModes.resize(pc); vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &pc, d.presentModes.data()); }
    return d;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : available) required.erase(ext.extensionName);
    return required.empty();
}

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    auto indices = findQueueFamilies(device, surface);
    bool extOk = checkDeviceExtensionSupport(device);
    bool swapOk = false;
    if (extOk) { auto sc = querySwapChainSupport(device, surface); swapOk = !sc.formats.empty() && !sc.presentModes.empty(); }
    return indices.isComplete() && extOk && swapOk;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
    return formats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& cap, GLFWwindow* window) {
    if (cap.currentExtent.width != UINT32_MAX) return cap.currentExtent;
    int w, h; glfwGetFramebufferSize(window, &w, &h);
    return {
        std::clamp((uint32_t)w, cap.minImageExtent.width,  cap.maxImageExtent.width),
        std::clamp((uint32_t)h, cap.minImageExtent.height, cap.maxImageExtent.height)
    };
}

int main() {
    try {
        // ─── GLFW ─────────────────────────────────────────────────────────────────
        if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        GLFWwindow* window = glfwCreateWindow(800, 600, "THE_RENDERER", nullptr, nullptr);
        if (!window) throw std::runtime_error("Failed to create window");

        // ─── Instance ─────────────────────────────────────────────────────────────
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
        std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);
        if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;
        instanceInfo.enabledExtensionCount = (uint32_t)extensions.size();
        instanceInfo.ppEnabledExtensionNames = extensions.data();
        if (enableValidationLayers) {
            instanceInfo.enabledLayerCount = (uint32_t)validationLayers.size();
            instanceInfo.ppEnabledLayerNames = validationLayers.data();
            instanceInfo.pNext = &debugCreateInfo;
        }

        VkInstance instance;
        if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan instance");

        VkDebugUtilsMessengerEXT debugMessenger{};
        if (enableValidationLayers)
            CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger);

        std::cout << "Vulkan instance created | Validation: " << (enableValidationLayers ? "ON" : "OFF") << std::endl;

        // ─── Surface ──────────────────────────────────────────────────────────────
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create window surface");

        // ─── Physical Device ──────────────────────────────────────────────────────
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (!deviceCount) throw std::runtime_error("No Vulkan GPU found");
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        for (const auto& pd : physicalDevices)
            if (isDeviceSuitable(pd, surface)) { physicalDevice = pd; break; }
        if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No suitable GPU");

        VkPhysicalDeviceProperties gpuProps;
        vkGetPhysicalDeviceProperties(physicalDevice, &gpuProps);
        std::cout << "GPU: " << gpuProps.deviceName << std::endl;

        // ─── Logical Device ───────────────────────────────────────────────────────
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
        std::vector<VkDeviceQueueCreateInfo> queueCIs;
        float queuePriority = 1.0f;
        for (uint32_t qf : std::set<uint32_t>{ indices.graphicsFamily.value(), indices.presentFamily.value() }) {
            VkDeviceQueueCreateInfo qi{};
            qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qi.queueFamilyIndex = qf; qi.queueCount = 1; qi.pQueuePriorities = &queuePriority;
            queueCIs.push_back(qi);
        }
        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo deviceCI{};
        deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCI.queueCreateInfoCount = (uint32_t)queueCIs.size();
        deviceCI.pQueueCreateInfos = queueCIs.data();
        deviceCI.pEnabledFeatures = &deviceFeatures;
        deviceCI.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceCI.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device;
        if (vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device) != VK_SUCCESS)
            throw std::runtime_error("Failed to create logical device");

        VkQueue graphicsQueue, presentQueue;
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
        std::cout << "Logical device created" << std::endl;

        // ─── Swap Chain ───────────────────────────────────────────────────────────
        SwapChainSupportDetails scSupport = querySwapChainSupport(physicalDevice, surface);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(scSupport.formats);
        VkPresentModeKHR   presentMode = chooseSwapPresentMode(scSupport.presentModes);
        VkExtent2D         extent = chooseSwapExtent(scSupport.capabilities, window);

        uint32_t imageCount = scSupport.capabilities.minImageCount + 1;
        if (scSupport.capabilities.maxImageCount > 0 && imageCount > scSupport.capabilities.maxImageCount)
            imageCount = scSupport.capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR scCI{};
        scCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scCI.surface = surface;
        scCI.minImageCount = imageCount;
        scCI.imageFormat = surfaceFormat.format;
        scCI.imageColorSpace = surfaceFormat.colorSpace;
        scCI.imageExtent = extent;
        scCI.imageArrayLayers = 1;
        scCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        uint32_t qfArr[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
        if (indices.graphicsFamily != indices.presentFamily) {
            scCI.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            scCI.queueFamilyIndexCount = 2; scCI.pQueueFamilyIndices = qfArr;
        }
        else { scCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
        scCI.preTransform = scSupport.capabilities.currentTransform;
        scCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scCI.presentMode = presentMode;
        scCI.clipped = VK_TRUE;

        VkSwapchainKHR swapchain;
        if (vkCreateSwapchainKHR(device, &scCI, nullptr, &swapchain) != VK_SUCCESS)
            throw std::runtime_error("Failed to create swap chain");
        std::cout << "Swap chain: " << extent.width << "x" << extent.height << " | " << (presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "Mailbox" : "FIFO") << std::endl;

        // ─── Images + Image Views ─────────────────────────────────────────────────
        uint32_t swapImageCount;
        vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, nullptr);
        std::vector<VkImage> swapImages(swapImageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, swapImages.data());

        std::vector<VkImageView> swapImageViews(swapImageCount);
        for (size_t i = 0; i < swapImageCount; i++) {
            VkImageViewCreateInfo vi{};
            vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vi.image = swapImages[i];
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = surfaceFormat.format;
            vi.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vi.subresourceRange.baseMipLevel = 0; vi.subresourceRange.levelCount = 1;
            vi.subresourceRange.baseArrayLayer = 0; vi.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device, &vi, nullptr, &swapImageViews[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create image view");
        }
        std::cout << "Image views: " << swapImageCount << std::endl;

        uint32_t framesInFlight = swapImageCount;

        // ─── Render Pass ──────────────────────────────────────────────────────────────
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = surfaceFormat.format;
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
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
        VkRenderPassCreateInfo rpCI{};
        rpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpCI.attachmentCount = (uint32_t)attachments.size();
        rpCI.pAttachments = attachments.data();
        rpCI.subpassCount = 1; rpCI.pSubpasses = &subpass;
        rpCI.dependencyCount = 1; rpCI.pDependencies = &dep;

        VkRenderPass renderPass;
        if (vkCreateRenderPass(device, &rpCI, nullptr, &renderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create render pass");
        std::cout << "Render pass created" << std::endl;
        // ─── VMA Allocator ────────────────────────────────────────────────────────
        VmaAllocatorCreateInfo allocatorCI{};
        allocatorCI.physicalDevice = physicalDevice;
        allocatorCI.device = device;
        allocatorCI.instance = instance;
        VmaAllocator vmaAllocator;
        vmaCreateAllocator(&allocatorCI, &vmaAllocator);

        // ─── Vertex Buffer ────────────────────────────────────────────────────────
        VkBufferCreateInfo vbCI{};
        vbCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vbCI.size = sizeof(vertices[0]) * vertices.size();
        vbCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vbCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vbAllocCI{};
        vbAllocCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VkBuffer      vertexBuffer;
        VmaAllocation vertexAllocation;
        vmaCreateBuffer(vmaAllocator, &vbCI, &vbAllocCI, &vertexBuffer, &vertexAllocation, nullptr);

        void* vbData;
        vmaMapMemory(vmaAllocator, vertexAllocation, &vbData);
        memcpy(vbData, vertices.data(), (size_t)vbCI.size);
        vmaUnmapMemory(vmaAllocator, vertexAllocation);
        std::cout << "Vertex buffer created" << std::endl;

        // ─── Uniform Buffers ──────────────────────────────────────────────────────
        std::vector<VkBuffer>      uniformBuffers(framesInFlight);
        std::vector<VmaAllocation> uniformAllocations(framesInFlight);
        std::vector<void*>         uniformMapped(framesInFlight);

        VkBufferCreateInfo ubCI{};
        ubCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ubCI.size = sizeof(UniformBufferObject);
        ubCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        ubCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo ubAllocCI{};
        ubAllocCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        ubAllocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        for (int i = 0; i < framesInFlight; i++) {
            VmaAllocationInfo allocOut{};
            vmaCreateBuffer(vmaAllocator, &ubCI, &ubAllocCI, &uniformBuffers[i], &uniformAllocations[i], &allocOut);
            uniformMapped[i] = allocOut.pMappedData;
        }
        std::cout << "Uniform buffers created" << std::endl;

        // ─── Depth Buffer ─────────────────────────────────────────────────────────────
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = depthFormat;
        depthImageInfo.extent = { extent.width, extent.height, 1 };
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo depthAllocInfo{};
        depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkImage       depthImage;
        VmaAllocation depthAllocation;
        vmaCreateImage(vmaAllocator, &depthImageInfo, &depthAllocInfo,
            &depthImage, &depthAllocation, nullptr);

        VkImageViewCreateInfo depthViewInfo{};
        depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthViewInfo.image = depthImage;
        depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthViewInfo.format = depthFormat;
        depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthViewInfo.subresourceRange.baseMipLevel = 0;
        depthViewInfo.subresourceRange.levelCount = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount = 1;

        VkImageView depthImageView;
        if (vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView) != VK_SUCCESS)
            throw std::runtime_error("Failed to create depth image view");

        std::cout << "Depth buffer created" << std::endl;

        // ─── Framebuffers ─────────────────────────────────────────────────────────
        std::vector<VkFramebuffer> framebuffers(swapImageCount);
        for (size_t i = 0; i < swapImageCount; i++) {
            std::array<VkImageView, 2> attachments = { swapImageViews[i], depthImageView };
            VkFramebufferCreateInfo fbCI{};
            fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbCI.renderPass = renderPass;
            fbCI.attachmentCount = (uint32_t)attachments.size();
            fbCI.pAttachments = attachments.data();
            fbCI.width = extent.width; fbCI.height = extent.height; fbCI.layers = 1;
            if (vkCreateFramebuffer(device, &fbCI, nullptr, &framebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create framebuffer");
        }
        std::cout << "Framebuffers: " << swapImageCount << std::endl;

        // ─── Descriptor Set Layout ────────────────────────────────────────────────
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo dslCI{};
        dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslCI.bindingCount = 1; dslCI.pBindings = &uboBinding;

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(device, &dslCI, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor set layout");

        // ─── Shaders ──────────────────────────────────────────────────────────────
        auto vertCode = readFile("triangle.vert.spv");
        auto fragCode = readFile("triangle.frag.spv");
        VkShaderModule vertModule = createShaderModule(device, vertCode);
        VkShaderModule fragModule = createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertModule; vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragModule; fragStage.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

        // ─── Pipeline ─────────────────────────────────────────────────────────────
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
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1; colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineLayoutCreateInfo pipelineLayoutCI{};
        pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCI.setLayoutCount = 1;
        pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout");

        VkGraphicsPipelineCreateInfo pipelineCI{};
        pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCI.stageCount = 2; pipelineCI.pStages = shaderStages;
        pipelineCI.pVertexInputState = &vertexInput;
        pipelineCI.pInputAssemblyState = &inputAssembly;
        pipelineCI.pViewportState = &viewportState;
        pipelineCI.pRasterizationState = &rasterizer;
        pipelineCI.pMultisampleState = &multisampling;
        pipelineCI.pColorBlendState = &colorBlending;
        pipelineCI.layout = pipelineLayout;
        pipelineCI.renderPass = renderPass;
        pipelineCI.pDepthStencilState = &depthStencil;
        pipelineCI.subpass = 0;

        VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &graphicsPipeline) != VK_SUCCESS)
            throw std::runtime_error("Failed to create graphics pipeline");

        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        std::cout << "Graphics pipeline created" << std::endl;

        // ─── Command Pool + Buffers ───────────────────────────────────────────────
        VkCommandPoolCreateInfo poolCI{};
        poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCI.queueFamilyIndex = indices.graphicsFamily.value();

        VkCommandPool commandPool;
        if (vkCreateCommandPool(device, &poolCI, nullptr, &commandPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create command pool");

        std::vector<VkCommandBuffer> commandBuffers(framesInFlight);
        VkCommandBufferAllocateInfo cbAllocInfo{};
        cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbAllocInfo.commandPool = commandPool;
        cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAllocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
        if (vkAllocateCommandBuffers(device, &cbAllocInfo, commandBuffers.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate command buffers");

        

        // ─── Descriptor Pool ──────────────────────────────────────────────────────
        VkDescriptorPoolSize dpSize{};
        dpSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        dpSize.descriptorCount = (uint32_t)framesInFlight;

        VkDescriptorPoolCreateInfo dpCI{};
        dpCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpCI.poolSizeCount = 1; dpCI.pPoolSizes = &dpSize;
        dpCI.maxSets = (uint32_t)framesInFlight;

        VkDescriptorPool descriptorPool;
        if (vkCreateDescriptorPool(device, &dpCI, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool");



        // ─── Descriptor Sets ──────────────────────────────────────────────────────
        std::vector<VkDescriptorSetLayout> dsLayouts(framesInFlight, descriptorSetLayout);
        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.descriptorPool = descriptorPool;
        dsAllocInfo.descriptorSetCount = (uint32_t)framesInFlight;
        dsAllocInfo.pSetLayouts = dsLayouts.data();

        std::vector<VkDescriptorSet> descriptorSets(framesInFlight);
        if (vkAllocateDescriptorSets(device, &dsAllocInfo, descriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate descriptor sets");

        for (int i = 0; i < framesInFlight; i++) {
            VkDescriptorBufferInfo dbi{};
            dbi.buffer = uniformBuffers[i]; dbi.offset = 0; dbi.range = sizeof(UniformBufferObject);
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSets[i]; write.dstBinding = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1; write.pBufferInfo = &dbi;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }
        std::cout << "Descriptor sets created" << std::endl;

        // ─── Sync Objects ─────────────────────────────────────────────────────────
        std::vector<VkSemaphore> imageAvailableSemaphores(swapImageCount);
        std::vector<VkSemaphore> renderFinishedSemaphores(swapImageCount);
        std::vector<VkFence>     inFlightFences(framesInFlight);

        VkSemaphoreCreateInfo semCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo     fenCI{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < swapImageCount; i++) {
            if (vkCreateSemaphore(device, &semCI, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semCI, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create semaphores");
        }
        for (int i = 0; i < framesInFlight; i++) {
            if (vkCreateFence(device, &fenCI, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create fences");
        }
        std::cout << "Sync objects created" << std::endl;
        std::cout << "Entering render loop..." << std::endl;



        // ═════════════════════════════════════════════════════════════════════════
        //  RENDER LOOP
        // ═════════════════════════════════════════════════════════════════════════
        std::vector<VkSemaphore> acquireSemaphores(framesInFlight);
        for (int i = 0; i < framesInFlight; i++)
            vkCreateSemaphore(device, &semCI, nullptr, &acquireSemaphores[i]);

        static auto startTime = std::chrono::high_resolution_clock::now();
        uint32_t currentFrame = 0;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // ── Update UBO ──────────────────────────────────────────────────────
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float>(currentTime - startTime).count();

            UniformBufferObject ubo{};
            ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 5.0f));
            ubo.view = glm::lookAt(
                glm::vec3(0.0f, 0.0f, 2.0f),  // camera straight ahead
                glm::vec3(0.0f, 0.0f, 0.0f),  // looking at origin
                glm::vec3(0.0f, 1.0f, 0.0f)   // up is Y
            );
            ubo.proj = glm::perspective(glm::radians(45.0f), (float)extent.width / extent.height, 0.1f, 10.0f);
            ubo.proj[1][1] *= -1;
            memcpy(uniformMapped[currentFrame], &ubo, sizeof(ubo));

            // ── Frame sync ──────────────────────────────────────────────────────
            vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

            uint32_t imageIndex;
            vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                acquireSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

            vkResetFences(device, 1, &inFlightFences[currentFrame]);

            // ── Record commands ─────────────────────────────────────────────────
            VkCommandBuffer cmd = commandBuffers[currentFrame];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            vkBeginCommandBuffer(cmd, &beginInfo);

            VkClearValue clearColor = { {{0.01f, 0.01f, 0.01f, 1.0f}} };
            VkRenderPassBeginInfo rpBegin{};
            rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBegin.renderPass = renderPass;
            rpBegin.framebuffer = framebuffers[imageIndex];
            rpBegin.renderArea.offset = { 0, 0 };
            rpBegin.renderArea.extent = extent;
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = { {0.01f, 0.01f, 0.01f, 1.0f} };
            clearValues[1].depthStencil = { 1.0f, 0 };

            rpBegin.clearValueCount = (uint32_t)clearValues.size();
            rpBegin.pClearValues = clearValues.data();;

            vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
            VkBuffer     vbufs[] = { vertexBuffer };
            VkDeviceSize voffsets[] = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, voffsets);
            vkCmdDraw(cmd, (uint32_t)vertices.size(), 1, 0, 0);
            vkCmdEndRenderPass(cmd);
            vkEndCommandBuffer(cmd);

            // ── Submit ──────────────────────────────────────────────────────────
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

            // ── Present ─────────────────────────────────────────────────────────
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1; presentInfo.pWaitSemaphores = signalSems;
            presentInfo.swapchainCount = 1; presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;
            vkQueuePresentKHR(graphicsQueue, &presentInfo);

            currentFrame = (currentFrame + 1) % framesInFlight;
        }

        vkDeviceWaitIdle(device);

        // ─── Cleanup (reverse order) ──────────────────────────────────────────────
        vkDestroyImageView(device, depthImageView, nullptr);
        vmaDestroyImage(vmaAllocator, depthImage, depthAllocation);
        for (int i = 0; i < framesInFlight; i++){
            vkDestroySemaphore(device, acquireSemaphores[i], nullptr);
        }
        for (size_t i = 0; i < swapImageCount; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        }
        for (int i = 0; i < framesInFlight; i++) {
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        for (int i = 0; i < framesInFlight; i++)
            vmaDestroyBuffer(vmaAllocator, uniformBuffers[i], uniformAllocations[i]);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vmaDestroyBuffer(vmaAllocator, vertexBuffer, vertexAllocation);
        vmaDestroyAllocator(vmaAllocator);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        for (auto fb : framebuffers)   vkDestroyFramebuffer(device, fb, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        for (auto iv : swapImageViews) vkDestroyImageView(device, iv, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyDevice(device, nullptr);
        if (enableValidationLayers)
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        system("pause");
        return -1;
    }
}