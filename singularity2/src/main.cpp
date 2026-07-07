// Project Singularity — Vulkan compute-raymarched black hole renderer
//
// Architecture (read this before the code — it explains the whole file):
//
//   1. GLFW opens a window and gives us a VkSurfaceKHR to present into.
//   2. vk-bootstrap picks a GPU, creates the VkInstance/VkDevice/VkSwapchainKHR
//      for us (this alone replaces ~400 lines of raw Vulkan boilerplate).
//   3. Rendering is now TWO compute passes instead of one:
//        Pass A (raymarch.comp): raymarches the black hole and writes
//          unclamped linear HDR color into hdrImage (VK_FORMAT_R16G16B16A16_SFLOAT).
//        Pass B (bloom.comp): samples hdrImage (with bilinear filtering),
//          finds bright areas, blurs them into a soft glow, adds that glow
//          back onto the original color, tonemaps, and writes the final
//          presentable LDR image into outputImage (VK_FORMAT_R8G8B8A8_UNORM).
//   4. outputImage gets blitted onto the current swapchain image and presented.
//
// This mirrors a standard real-time bloom pipeline: render HDR -> extract/blur
// bright areas -> composite -> tonemap. It is a genuine two-pass post-process,
// not a raymarch-time lighting hack.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <glm/glm.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

struct PushConstants {
    glm::vec4 resTime;     // xy resolution, z time, w Schwarzschild radius
    glm::vec4 camPos;
    glm::vec4 camForward;
    glm::vec4 camRight;
    glm::vec4 camUp;       // w = disk inner radius (units of Rs)
};

// Kept as four flat floats (not vec2+float+float) to avoid any ambiguity
// between C++ struct packing and GLSL push-constant layout rules.
struct BloomPushConstants {
    float resX;
    float resY;
    float exposure;
    float threshold;
};

static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
static constexpr uint32_t WIDTH = 1280;
static constexpr uint32_t HEIGHT = 720;

static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open shader: " + path);
    size_t size = (size_t)file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

class SingularityApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;

    vkb::Instance vkbInstance;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    vkb::Device vkbDevice;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    vkb::Swapchain vkbSwapchain;
    std::vector<VkImage> swapchainImages;
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;

    // --- Pass A target: linear HDR, written by raymarch.comp, read by bloom.comp ---
    VkImage hdrImage = VK_NULL_HANDLE;
    VkDeviceMemory hdrImageMemory = VK_NULL_HANDLE;
    VkImageView hdrImageView = VK_NULL_HANDLE;
    VkSampler hdrSampler = VK_NULL_HANDLE;

    // --- Pass B target: tonemapped LDR, written by bloom.comp, blitted to swapchain ---
    VkImage outputImage = VK_NULL_HANDLE;
    VkDeviceMemory outputImageMemory = VK_NULL_HANDLE;
    VkImageView outputImageView = VK_NULL_HANDLE;

    // Pass A (raymarch) pipeline objects
    VkDescriptorSetLayout raymarchSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout raymarchPipelineLayout = VK_NULL_HANDLE;
    VkPipeline raymarchPipeline = VK_NULL_HANDLE;
    VkDescriptorSet raymarchSet = VK_NULL_HANDLE;

    // Pass B (bloom) pipeline objects
    VkDescriptorSetLayout bloomSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout bloomPipelineLayout = VK_NULL_HANDLE;
    VkPipeline bloomPipeline = VK_NULL_HANDLE;
    VkDescriptorSet bloomSet = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    size_t currentFrame = 0;

    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastFrameTime;

    // Live-tunable camera state (controlled via keyboard, see processInput())
    float camOrbitRadius = 28.0f;   // distance from black hole, in units of Rs
    float camElevation = 18.0f;     // height above the disk plane, in units of Rs
    float camAzimuth = 0.0f;        // angle around the black hole, radians
    bool autoRotate = true;
    bool spaceWasDown = false;

    // Bloom tuning — feel free to ask for these to be exposed as keyboard
    // controls too if you want to tune them live like the camera.
    float bloomExposure = 0.75f;
    float bloomThreshold = 2.1f; // pixels brighter than this start to bloom

    // -----------------------------------------------------------------
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // no OpenGL context
        window = glfwCreateWindow(WIDTH, HEIGHT, "Project Singularity — Vulkan", nullptr, nullptr);
    }

    // -----------------------------------------------------------------
    void initVulkan() {
        startTime = std::chrono::steady_clock::now();

        vkb::InstanceBuilder builder;
        auto instRet = builder.set_app_name("Project Singularity")
                           .require_api_version(1, 2, 0)
#ifndef NDEBUG
                           .request_validation_layers(true)
                           .use_default_debug_messenger()
#endif
                           .build();
        if (!instRet) throw std::runtime_error("Failed to create Vulkan instance: " + instRet.error().message());
        vkbInstance = instRet.value();

        if (glfwCreateWindowSurface(vkbInstance.instance, window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create window surface");

        VkPhysicalDeviceFeatures features{};
        vkb::PhysicalDeviceSelector selector{vkbInstance};
        auto physRet = selector.set_surface(surface)
                           .set_minimum_version(1, 2)
                           .set_required_features(features)
                           .select();
        if (!physRet) throw std::runtime_error("Failed to select GPU: " + physRet.error().message());

        vkb::DeviceBuilder deviceBuilder{physRet.value()};
        auto devRet = deviceBuilder.build();
        if (!devRet) throw std::runtime_error("Failed to create device: " + devRet.error().message());
        vkbDevice = devRet.value();

        auto computeQueueRet = vkbDevice.get_queue(vkb::QueueType::compute);
        if (!computeQueueRet) {
            std::cerr << "No compute-tagged queue found (" << computeQueueRet.error().message()
                      << "); falling back to the graphics queue, which always supports compute.\n";
            computeQueueRet = vkbDevice.get_queue(vkb::QueueType::graphics);
        }
        if (!computeQueueRet) throw std::runtime_error("Failed to get a usable compute queue: " + computeQueueRet.error().message());
        computeQueue = computeQueueRet.value();

        auto presentQueueRet = vkbDevice.get_queue(vkb::QueueType::present);
        if (!presentQueueRet) throw std::runtime_error("Failed to get present queue: " + presentQueueRet.error().message());
        presentQueue = presentQueueRet.value();

        createSwapchain();
        createCommandPool();          // needed for one-time layout transitions below
        createHdrImage();
        createOutputImage();
        createHdrSampler();

        createRaymarchDescriptorSetLayout();
        createBloomDescriptorSetLayout();
        createRaymarchPipeline();
        createBloomPipeline();

        createDescriptorPool();
        createRaymarchDescriptorSet();
        createBloomDescriptorSet();

        createCommandBuffers();
        createSyncObjects();
    }

    void createSwapchain() {
        vkb::SwapchainBuilder swapchainBuilder{vkbDevice};
        auto swapRet = swapchainBuilder
                           .use_default_format_selection()
                           .set_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                           .set_desired_extent(WIDTH, HEIGHT)
                           .build();
        if (!swapRet) throw std::runtime_error("Failed to create swapchain: " + swapRet.error().message());
        vkbSwapchain = swapRet.value();
        swapchainImages = vkbSwapchain.get_images().value();
        swapchainFormat = vkbSwapchain.image_format;
        swapchainExtent = vkbSwapchain.extent;
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(vkbDevice.physical_device, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        }
        throw std::runtime_error("No suitable memory type found");
    }

    // Generic helper: create an image + memory + view, and leave it in the
    // given initial layout (via a one-time command buffer).
    void createImage(VkFormat format, VkImageUsageFlags usage, VkImage& image,
                      VkDeviceMemory& memory, VkImageView& view, VkImageLayout initialLayout) {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {swapchainExtent.width, swapchainExtent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(vkbDevice.device, &imageInfo, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image");

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(vkbDevice.device, image, &memReq);

        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(vkbDevice.device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate image memory");
        vkBindImageMemory(vkbDevice.device, image, memory, 0);

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(vkbDevice.device, &viewInfo, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image view");

        VkCommandBuffer cmd = beginSingleTimeCommands();
        transitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, initialLayout,
                               0, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        endSingleTimeCommands(cmd);
    }

    void createHdrImage() {
        createImage(VK_FORMAT_R16G16B16A16_SFLOAT,
                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    hdrImage, hdrImageMemory, hdrImageView, VK_IMAGE_LAYOUT_GENERAL);
    }

    void createOutputImage() {
        createImage(VK_FORMAT_R8G8B8A8_UNORM,
                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    outputImage, outputImageMemory, outputImageView, VK_IMAGE_LAYOUT_GENERAL);
    }

    void createHdrSampler() {
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        if (vkCreateSampler(vkbDevice.device, &samplerInfo, nullptr, &hdrSampler) != VK_SUCCESS)
            throw std::runtime_error("Failed to create HDR sampler");
    }

    // -----------------------------------------------------------------
    // Pass A: raymarch descriptor layout / pipeline / set
    void createRaymarchDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        if (vkCreateDescriptorSetLayout(vkbDevice.device, &layoutInfo, nullptr, &raymarchSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create raymarch descriptor set layout");
    }

    void createRaymarchPipeline() {
        auto code = readFile(std::string(SHADER_DIR) + "raymarch.comp.spv");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = code.size();
        moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(vkbDevice.device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS)
            throw std::runtime_error("Failed to create raymarch shader module");

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &raymarchSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(vkbDevice.device, &layoutInfo, nullptr, &raymarchPipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create raymarch pipeline layout");

        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = shaderModule;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = raymarchPipelineLayout;
        if (vkCreateComputePipelines(vkbDevice.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &raymarchPipeline) != VK_SUCCESS)
            throw std::runtime_error("Failed to create raymarch pipeline");

        vkDestroyShaderModule(vkbDevice.device, shaderModule, nullptr);
    }

    void createRaymarchDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &raymarchSetLayout;
        if (vkAllocateDescriptorSets(vkbDevice.device, &allocInfo, &raymarchSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate raymarch descriptor set");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = hdrImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = raymarchSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(vkbDevice.device, 1, &write, 0, nullptr);
    }

    // -----------------------------------------------------------------
    // Pass B: bloom descriptor layout / pipeline / set
    void createBloomDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(vkbDevice.device, &layoutInfo, nullptr, &bloomSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create bloom descriptor set layout");
    }

    void createBloomPipeline() {
        auto code = readFile(std::string(SHADER_DIR) + "bloom.comp.spv");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = code.size();
        moduleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(vkbDevice.device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS)
            throw std::runtime_error("Failed to create bloom shader module");

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(BloomPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &bloomSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(vkbDevice.device, &layoutInfo, nullptr, &bloomPipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create bloom pipeline layout");

        VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = shaderModule;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = bloomPipelineLayout;
        if (vkCreateComputePipelines(vkbDevice.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &bloomPipeline) != VK_SUCCESS)
            throw std::runtime_error("Failed to create bloom pipeline");

        vkDestroyShaderModule(vkbDevice.device, shaderModule, nullptr);
    }

    void createBloomDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &bloomSetLayout;
        if (vkAllocateDescriptorSets(vkbDevice.device, &allocInfo, &bloomSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate bloom descriptor set");

        VkDescriptorImageInfo hdrInfo{};
        hdrInfo.sampler = hdrSampler;
        hdrInfo.imageView = hdrImageView;
        hdrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo outInfo{};
        outInfo.imageView = outputImageView;
        outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet writes[2]{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[0].dstSet = bloomSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &hdrInfo;

        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[1].dstSet = bloomSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo = &outInfo;

        vkUpdateDescriptorSets(vkbDevice.device, 2, writes, 0, nullptr);
    }

    void createDescriptorPool() {
        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2};          // raymarch's hdr write + bloom's output write
        poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}; // bloom's hdr read

        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        poolInfo.maxSets = 2;
        if (vkCreateDescriptorPool(vkbDevice.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool");
    }

    void createCommandPool() {
        auto computeFamilyRet = vkbDevice.get_queue_index(vkb::QueueType::compute);
        if (!computeFamilyRet) {
            computeFamilyRet = vkbDevice.get_queue_index(vkb::QueueType::graphics);
        }
        if (!computeFamilyRet) throw std::runtime_error("Failed to get a usable queue family index: " + computeFamilyRet.error().message());

        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = computeFamilyRet.value();
        if (vkCreateCommandPool(vkbDevice.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create command pool");
    }

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
        if (vkAllocateCommandBuffers(vkbDevice.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate command buffers");
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(vkbDevice.device, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vkbDevice.device, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vkbDevice.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create sync objects");
        }
    }

    // -----------------------------------------------------------------
    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(vkbDevice.device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
        return cmd;
    }

    void endSingleTimeCommands(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(computeQueue);
        vkFreeCommandBuffers(vkbDevice.device, commandPool, 1, &cmd);
    }

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                                VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // -----------------------------------------------------------------
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cmd, &beginInfo);

        // ---------------- Pass A: raymarch into hdrImage (linear HDR) ----------------
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, raymarchPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, raymarchPipelineLayout, 0, 1, &raymarchSet, 0, nullptr);

        float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
        PushConstants pc{};
        pc.resTime = glm::vec4((float)swapchainExtent.width, (float)swapchainExtent.height, t, 1.0f /* Rs */);
        glm::vec3 camPos = glm::vec3(std::cos(camAzimuth) * camOrbitRadius, camElevation, std::sin(camAzimuth) * camOrbitRadius);
        glm::vec3 fwd = glm::normalize(-camPos);
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::cross(right, fwd);
        pc.camPos = glm::vec4(camPos, 0.0f);
        pc.camForward = glm::vec4(fwd, 0.0f);
        pc.camRight = glm::vec4(right, 0.0f);
        pc.camUp = glm::vec4(up, 2.5f /* disk inner radius in units of Rs */);
        vkCmdPushConstants(cmd, raymarchPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);

        uint32_t groupsX = (swapchainExtent.width + 15) / 16;
        uint32_t groupsY = (swapchainExtent.height + 15) / 16;
        vkCmdDispatch(cmd, groupsX, groupsY, 1);

        // Barrier: hdrImage GENERAL (compute write) -> SHADER_READ_ONLY_OPTIMAL (sampled read in pass B)
        transitionImageLayout(cmd, hdrImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        // ---------------- Pass B: bloom, reads hdrImage, writes outputImage (LDR) ----------------
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, bloomPipelineLayout, 0, 1, &bloomSet, 0, nullptr);

        BloomPushConstants bpc{};
        bpc.resX = (float)swapchainExtent.width;
        bpc.resY = (float)swapchainExtent.height;
        bpc.exposure = bloomExposure;
        bpc.threshold = bloomThreshold;
        vkCmdPushConstants(cmd, bloomPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomPushConstants), &bpc);

        vkCmdDispatch(cmd, groupsX, groupsY, 1);

        // Barriers: outputImage GENERAL -> TRANSFER_SRC (for blit), hdrImage back to GENERAL
        // (ready for next frame's raymarch write), swapchain image -> TRANSFER_DST (for blit)
        VkImageMemoryBarrier barriers[3]{};
        barriers[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = outputImage;
        barriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        barriers[1] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].image = hdrImage;
        barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        barriers[2] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barriers[2].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[2].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[2].image = swapchainImages[imageIndex];
        barriers[2].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barriers[2].srcAccessMask = 0;
        barriers[2].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              0, 0, nullptr, 0, nullptr, 3, barriers);

        // Blit outputImage -> swapchain image
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[1] = {(int32_t)swapchainExtent.width, (int32_t)swapchainExtent.height, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[1] = {(int32_t)swapchainExtent.width, (int32_t)swapchainExtent.height, 1};
        vkCmdBlitImage(cmd, outputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_NEAREST);

        // Barriers back: outputImage -> GENERAL (ready for next bloom dispatch), swapchain -> PRESENT_SRC
        VkImageMemoryBarrier postBlit[2]{};
        postBlit[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        postBlit[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        postBlit[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        postBlit[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postBlit[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postBlit[0].image = outputImage;
        postBlit[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        postBlit[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        postBlit[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        postBlit[1] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        postBlit[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        postBlit[1].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        postBlit[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postBlit[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        postBlit[1].image = swapchainImages[imageIndex];
        postBlit[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        postBlit[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        postBlit[1].dstAccessMask = 0;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                              0, 0, nullptr, 0, nullptr, 2, postBlit);

        vkEndCommandBuffer(cmd);
    }

    // -----------------------------------------------------------------
    void mainLoop() {
        lastFrameTime = std::chrono::steady_clock::now();
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - lastFrameTime).count();
            lastFrameTime = now;
            processInput(dt);
            drawFrame();
        }
        vkDeviceWaitIdle(vkbDevice.device);
    }

    // Controls:
    //   W / S       - zoom in / out
    //   Up / Down   - raise / lower camera (elevation above disk plane)
    //   A / D       - manually orbit left / right (disables auto-rotate)
    //   Space       - toggle auto-rotate back on
    void processInput(float dt) {
        const float zoomSpeed = 1.0f;
        const float elevSpeed = 8.0f;
        const float orbitSpeed = 0.6f;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camOrbitRadius *= (1.0f - zoomSpeed * dt);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camOrbitRadius *= (1.0f + zoomSpeed * dt);
        camOrbitRadius = glm::clamp(camOrbitRadius, 3.0f, 200.0f);

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            camElevation += elevSpeed * dt;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            camElevation -= elevSpeed * dt;
        camElevation = glm::clamp(camElevation, -100.0f, 100.0f);

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camAzimuth -= orbitSpeed * dt;
            autoRotate = false;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camAzimuth += orbitSpeed * dt;
            autoRotate = false;
        }

        bool spaceDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (spaceDown && !spaceWasDown) autoRotate = !autoRotate;
        spaceWasDown = spaceDown;

        if (autoRotate) camAzimuth += 0.15f * dt;

        // Bloom tuning: = / - adjust exposure, ] / [ adjust threshold
        const float exposureSpeed = 0.6f;
        const float thresholdSpeed = 0.6f;
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
            bloomExposure += exposureSpeed * dt;
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
            bloomExposure -= exposureSpeed * dt;
        bloomExposure = glm::clamp(bloomExposure, 0.1f, 4.0f);

        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
            bloomThreshold += thresholdSpeed * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
            bloomThreshold -= thresholdSpeed * dt;
        bloomThreshold = glm::clamp(bloomThreshold, 0.1f, 5.0f);
    }

    void drawFrame() {
        vkWaitForFences(vkbDevice.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vkbDevice.device, vkbSwapchain.swapchain, UINT64_MAX,
                                                 imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) return;

        vkResetFences(vkbDevice.device, 1, &inFlightFences[currentFrame]);

        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(computeQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
            throw std::runtime_error("Failed to submit compute command buffer");

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &vkbSwapchain.swapchain;
        presentInfo.pImageIndices = &imageIndex;
        vkQueuePresentKHR(presentQueue, &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // -----------------------------------------------------------------
    void cleanup() {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(vkbDevice.device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(vkbDevice.device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(vkbDevice.device, inFlightFences[i], nullptr);
        }
        vkDestroyCommandPool(vkbDevice.device, commandPool, nullptr);

        vkDestroyPipeline(vkbDevice.device, raymarchPipeline, nullptr);
        vkDestroyPipelineLayout(vkbDevice.device, raymarchPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(vkbDevice.device, raymarchSetLayout, nullptr);

        vkDestroyPipeline(vkbDevice.device, bloomPipeline, nullptr);
        vkDestroyPipelineLayout(vkbDevice.device, bloomPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(vkbDevice.device, bloomSetLayout, nullptr);

        vkDestroyDescriptorPool(vkbDevice.device, descriptorPool, nullptr);

        vkDestroySampler(vkbDevice.device, hdrSampler, nullptr);
        vkDestroyImageView(vkbDevice.device, hdrImageView, nullptr);
        vkDestroyImage(vkbDevice.device, hdrImage, nullptr);
        vkFreeMemory(vkbDevice.device, hdrImageMemory, nullptr);

        vkDestroyImageView(vkbDevice.device, outputImageView, nullptr);
        vkDestroyImage(vkbDevice.device, outputImage, nullptr);
        vkFreeMemory(vkbDevice.device, outputImageMemory, nullptr);

        vkb::destroy_swapchain(vkbSwapchain);
        vkb::destroy_device(vkbDevice);
        vkb::destroy_surface(vkbInstance, surface);
        vkb::destroy_instance(vkbInstance);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    SingularityApp app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
