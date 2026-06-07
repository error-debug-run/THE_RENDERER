#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vk_mem_alloc.h>

#include <vector>
#include <optional>
#include <string>
#include <array>
#include <set>

// ─── Vertex ───────────────────────────────────────────────────────────────────
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

    static std::array< VkVertexInputAttributeDescription, 2 > getAttributeDescriptions() {
        std::array< VkVertexInputAttributeDescription, 2 > attrs{};
        attrs[0].binding = 0; attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset = offsetof(Vertex, pos);
        attrs[1].binding = 0; attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset = offsetof(Vertex, color);
        return attrs;
    }
};

// ─── UBO ──────────────────────────────────────────────────────────────────────
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// ─── Queue Families ───────────────────────────────────────────────────────────
struct QueueFamilyIndices {
    std::optional< uint32_t> graphicsFamily;
    std::optional< uint32_t> presentFamily;
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

// ─── Swap Chain Support ───────────────────────────────────────────────────────
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities{};
    std::vector< VkSurfaceFormatKHR> formats;
    std::vector< VkPresentModeKHR>   presentModes;
};

// ─── Renderer ─────────────────────────────────────────────────────────────────
class Renderer {
public:
    void init(GLFWwindow* window);
    void drawFrame(float time);
    void cleanup();

private:
    GLFWwindow* window = nullptr;

    // Core Vulkan
    VkInstance               instance;
    VkDebugUtilsMessengerEXT debugMessenger{};
    VkSurfaceKHR             surface;
    VkPhysicalDevice         physicalDevice = VK_NULL_HANDLE;
    VkDevice                 device;
    VkQueue                  graphicsQueue;
    VkQueue                  presentQueue;
    QueueFamilyIndices       queueIndices;

    // Swap chain
    VkSwapchainKHR             swapchain;
    std::vector< VkImage>       swapImages;
    std::vector< VkImageView>   swapImageViews;
    VkSurfaceFormatKHR         swapSurfaceFormat;
    VkExtent2D                 extent;
    uint32_t                   swapImageCount = 0;
    uint32_t                   framesInFlight = 0;

    // Render pass + pipeline
    VkRenderPass          renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout      pipelineLayout;
    VkPipeline            graphicsPipeline;

    // Framebuffers
    std::vector< VkFramebuffer> framebuffers;

    // Depth
    VkImage       depthImage;
    VmaAllocation depthAllocation;
    VkImageView   depthImageView;

    // Commands
    VkCommandPool                commandPool;
    std::vector< VkCommandBuffer> commandBuffers;

    // VMA
    VmaAllocator vmaAllocator;

    // Buffers
    VkBuffer      vertexBuffer;
    VmaAllocation vertexAllocation;

    std::vector< VkBuffer>      uniformBuffers;
    std::vector< VmaAllocation> uniformAllocations;
    std::vector< void*>         uniformMapped;

    // Descriptors
    VkDescriptorPool             descriptorPool;
    std::vector< VkDescriptorSet> descriptorSets;

    // Sync
    std::vector< VkSemaphore> acquireSemaphores;
    std::vector< VkSemaphore> renderFinishedSemaphores;
    std::vector< VkFence>     inFlightFences;

    uint32_t currentFrame = 0;

    // Init helpers
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createVMAAllocator();
    void createVertexBuffer();
    void createUniformBuffers();
    void createDepthBuffer();
    void createFramebuffers();
    void createDescriptorSetLayout();
    void createPipeline();
    void createCommandPool();
    void createCommandBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createSyncObjects();

    // Utilities
    bool                    checkValidationLayerSupport() ;
    QueueFamilyIndices      findQueueFamilies(VkPhysicalDevice dev);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev);
    bool                    isDeviceSuitable(VkPhysicalDevice dev) ;
    bool                    checkDeviceExtensionSupport(VkPhysicalDevice dev) ;
    VkSurfaceFormatKHR      chooseSwapSurfaceFormat(const std::vector< VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR        chooseSwapPresentMode(const std::vector< VkPresentModeKHR>& modes);
    VkExtent2D              chooseSwapExtent(const VkSurfaceCapabilitiesKHR& cap);
    VkShaderModule          createShaderModule(const std::vector< char>& code);
    std::vector< char>       readFile(const std::string& filename);
};