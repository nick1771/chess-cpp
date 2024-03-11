#include "VulkanGraphicsDevice.h"
#include "VulkanMapping.h"
#include "VulkanCommandBufferHelper.h"
#include "VulkanSurface.h"

#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Windowing/Window.h"

#include <print>
#include <ranges>

namespace Pandora::Implementation {

    constexpr auto RenderTargetFormat = vk::Format::eR16G16B16A16Sfloat;
    constexpr auto StagingFrameIndex = 5;

#ifdef DEBUG_ENABLED
    constexpr auto IsDebugModeEnabled = true;
#else
    constexpr auto IsDebugModeEnabled = false;
#endif

    static constexpr auto getRequiredInstanceExtensions() {
        if constexpr (IsDebugModeEnabled) {
            return std::array{ "VK_KHR_win32_surface", "VK_KHR_surface", "VK_EXT_debug_utils" };
        } else {
            return std::array{ "VK_KHR_win32_surface", "VK_KHR_surface" };
        }
    }

    static constexpr auto getRequiredInstanceLayers() {
        if constexpr (IsDebugModeEnabled) {
            return std::array{ "VK_LAYER_KHRONOS_validation" };
        } else {
            return std::array<const char*, 0>{};
        }
    }

    constexpr std::array<const char*, 1> RequiredDeviceExtensions = {
        "VK_KHR_swapchain",
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData
    ) {
        const auto severityFlag = static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity);
        const auto typeFlag = static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageType);

        const auto severity = vk::to_string(severityFlag);
        const auto type = vk::to_string(typeFlag);

        std::println("{} {}: {}", severity, type, callbackData->pMessage);

        return vk::False;
    }

    constexpr auto DebugSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

    constexpr auto DebugMessageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

    static constexpr auto getDebugUtilsMessengerCreateInfo() {
        auto debugUtilsCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{};
        debugUtilsCreateInfo.messageSeverity = DebugSeverity;
        debugUtilsCreateInfo.messageType = DebugMessageType;
        debugUtilsCreateInfo.pfnUserCallback = debugUtilsCallback;

        return debugUtilsCreateInfo;
    }

    struct VulkanInstance {
        vk::Instance instance{};
        vk::DebugUtilsMessengerEXT debugUtilsMessenger{};
        VulkanExtensionDispatch extensionDispatch{};
    };

    static VulkanInstance createVulkanInstance() {
        auto applicationInfo = vk::ApplicationInfo{};
        applicationInfo.pApplicationName = "Vulkan Application";
        applicationInfo.pEngineName = "Vulkan Engine";
        applicationInfo.applicationVersion = vk::makeApiVersion(1, 0, 0, 0);
        applicationInfo.engineVersion = vk::makeApiVersion(1, 0, 0, 0);
        applicationInfo.apiVersion = vk::ApiVersion13;

        auto requiredInstanceExtensions = getRequiredInstanceExtensions();
        auto requiredInstanceLayers = getRequiredInstanceLayers();

        auto instanceCreateInfo = vk::InstanceCreateInfo{};
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();
        instanceCreateInfo.enabledExtensionCount = static_cast<u32>(requiredInstanceExtensions.size());
        instanceCreateInfo.ppEnabledLayerNames = requiredInstanceLayers.data();
        instanceCreateInfo.enabledLayerCount = static_cast<u32>(requiredInstanceLayers.size());

        if (IsDebugModeEnabled) {
            auto debugUtilsMessengerCreateInfo = getDebugUtilsMessengerCreateInfo();
            instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;
        }

        const auto instance = vk::createInstance(instanceCreateInfo);
        const auto extensionDispatch = VulkanExtensionDispatch{ instance };

        auto debugUtilsMessenger = vk::DebugUtilsMessengerEXT{};
        if (IsDebugModeEnabled) {
            auto debugUtilsMessengerCreateInfo = getDebugUtilsMessengerCreateInfo();
            debugUtilsMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfo, nullptr, extensionDispatch);
        }

        return VulkanInstance{
            instance,
            debugUtilsMessenger,
            extensionDispatch
        };
    }

    struct VulkanDevice {
        vk::PhysicalDevice physicalDevice{};
        vk::Device logicalDevice{};
        vk::Queue graphicsQueue{};
        u32 graphicsQueueIndex{};
    };

    static void selectPhysicalDevice(VulkanDevice& device, vk::Instance instance) {
        const auto physicalDevices = instance.enumeratePhysicalDevices();

        if (physicalDevices.size() == 1) {
            device.physicalDevice = physicalDevices.front();
            return;
        }

        for (const auto& physicalDevice : physicalDevices) {
            const auto properties = physicalDevice.getProperties();
            if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                device.physicalDevice = physicalDevice;
                return;
            }
        }

        throw std::runtime_error("No physical device could be selected");
    }

    static void selectGraphicsQueueIndex(VulkanDevice& device, vk::SurfaceKHR surface) {
        const auto queueProperties = device.physicalDevice.getQueueFamilyProperties();
        for (auto index = 0; index < queueProperties.size(); index++) {
            const auto& queueProperty = queueProperties[index];

            const auto isGraphicsSupported = queueProperty.queueFlags & vk::QueueFlagBits::eGraphics;
            const auto isPresentationSupported = device.physicalDevice.getSurfaceSupportKHR(index, surface);

            if (isGraphicsSupported && isPresentationSupported) {
                device.graphicsQueueIndex = index;
                return;
            }
        }

        throw std::runtime_error("No graphics queue could be selected");
    }

    static void createLogicalDeviceAndQueue(VulkanDevice& device) {
        auto vulkan13Features = vk::PhysicalDeviceVulkan13Features{};
        vulkan13Features.dynamicRendering = true;
        vulkan13Features.synchronization2 = true;

        auto vulkan12Features = vk::PhysicalDeviceVulkan12Features{};
        vulkan12Features.bufferDeviceAddress = true;
        vulkan12Features.descriptorIndexing = true;
        vulkan12Features.pNext = &vulkan13Features;

        const auto queuePriority = 1.f;

        auto queueCreateInfo = vk::DeviceQueueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = device.graphicsQueueIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        auto deviceCreateInfo = vk::DeviceCreateInfo{};
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.ppEnabledExtensionNames = RequiredDeviceExtensions.data();
        deviceCreateInfo.enabledExtensionCount = static_cast<u32>(RequiredDeviceExtensions.size());
        deviceCreateInfo.pNext = &vulkan12Features;

        device.logicalDevice = device.physicalDevice.createDevice(deviceCreateInfo);
        device.graphicsQueue = device.logicalDevice.getQueue(device.graphicsQueueIndex, 0);
    }

    static VulkanDevice createVulkanDevice(vk::Instance instance, vk::SurfaceKHR surface) {
        auto device = VulkanDevice{};

        selectPhysicalDevice(device, instance);
        selectGraphicsQueueIndex(device, surface);
        createLogicalDeviceAndQueue(device);

        return device;
    }

    struct VulkanSwapchain {
        vk::SwapchainKHR swapchain{};
        std::vector<vk::Image> images{};
        std::vector<vk::UniqueImageView> views{};
    };

    static u32 getMinimumSurfaceImageCount(const vk::SurfaceCapabilitiesKHR& capabilities) {
        u32 minimumImageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && minimumImageCount > capabilities.maxImageCount) {
            minimumImageCount = capabilities.maxImageCount;
        }

        return minimumImageCount;
    }

    static vk::SurfaceFormatKHR getSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& surfaceFormats) {
        for (const auto& format : surfaceFormats) {
            const auto isValidFormat = format.format == vk::Format::eB8G8R8A8Unorm;
            const auto isValidColorspace = format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;

            if (isValidFormat && isValidColorspace) {
                return format;
            }
        }

        throw std::runtime_error("Unsuported surface formats");
    }

    static auto getSurfaceSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D extent) {
        if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
            return capabilities.currentExtent;
        }

        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }

    static VulkanSwapchain createVulkanSwapchain(const VulkanDevice& device, vk::SurfaceKHR surface, u32 width, u32 height) {
        const auto surfaceFormat = getSurfaceFormat(device.physicalDevice.getSurfaceFormatsKHR(surface));
        const auto surfaceCapabilities = device.physicalDevice.getSurfaceCapabilitiesKHR(surface);
        const auto surfaceMinimumImageCount = getMinimumSurfaceImageCount(surfaceCapabilities);
        const auto surfaceExtent = getSurfaceSwapExtent(surfaceCapabilities, vk::Extent2D{ width, height });

        auto swapchainCreateInfo = vk::SwapchainCreateInfoKHR{};
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = surfaceMinimumImageCount;
        swapchainCreateInfo.imageFormat = surfaceFormat.format;
        swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent = surfaceExtent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment;
        swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapchainCreateInfo.presentMode = vk::PresentModeKHR::eFifo;
        swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
        swapchainCreateInfo.clipped = true;
        swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
        swapchainCreateInfo.queueFamilyIndexCount = 1;
        swapchainCreateInfo.pQueueFamilyIndices = &device.graphicsQueueIndex;

        const auto swapchain = device.logicalDevice.createSwapchainKHR(swapchainCreateInfo);
        const auto images = device.logicalDevice.getSwapchainImagesKHR(swapchain);

        auto views = std::vector<vk::UniqueImageView>{};
        for (const auto& image : images) {
            auto subresourceRange = vk::ImageSubresourceRange{};
            subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.baseArrayLayer = 0;
            subresourceRange.layerCount = 1;

            auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
            imageViewCreateInfo.subresourceRange = subresourceRange;
            imageViewCreateInfo.image = image;
            imageViewCreateInfo.format = surfaceFormat.format;
            imageViewCreateInfo.viewType = vk::ImageViewType::e2D;

            views.push_back(device.logicalDevice.createImageViewUnique(imageViewCreateInfo));
        }

        return VulkanSwapchain{
            swapchain,
            images,
            std::move(views)
        };
    }

    struct VertexLayoutInfo {
        ArrayVector<vk::VertexInputAttributeDescription, Constants::MaximumVertexElementCount> attributeDescriptions{};
        vk::VertexInputBindingDescription bindingDescription{};
    };

    static VertexLayoutInfo getVertexLayoutInfo(const PipelineCreateInfo& createInfo) {
        auto result = VertexLayoutInfo{};

        auto currentVertexOffset = 0u;
        for (usize index = 0; index < createInfo.vertexLayout.size(); index++) {
            const auto vertexLayoutElement = createInfo.vertexLayout[index];

            auto attributeDescription = vk::VertexInputAttributeDescription{};
            attributeDescription.format = mapVertexLayoutElementFormatToVulkanFormat(vertexLayoutElement);
            attributeDescription.offset = currentVertexOffset;
            attributeDescription.location = static_cast<u32>(index);
            attributeDescription.binding = 0u;

            currentVertexOffset += mapVertexLayoutElementToSize(vertexLayoutElement);
            result.attributeDescriptions.push(attributeDescription);
        }

        result.bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        result.bindingDescription.stride = currentVertexOffset;
        result.bindingDescription.binding = 0u;

        return result;
    }

    VulkanGraphicsDevice::~VulkanGraphicsDevice() {
        _destroySwapchain();
        _descriptorCache.terminate();

        for (auto textureId : std::ranges::iota_view(0ull, _textures.size())) {
            destroyTexture(textureId);
        }

        for (auto bufferId : std::ranges::iota_view(0ull, _buffers.size())) {
            destroyBuffer(bufferId);
        }

        for (const auto& pipeline : _pipelines) {
            _logicalDevice.destroyPipeline(pipeline.handle);
            _logicalDevice.destroyPipelineLayout(pipeline.layout);
        }

        _logicalDevice.destroyCommandPool(_transferCommandPool);
        _logicalDevice.destroyFence(_transferFence);
        _logicalDevice.destroySampler(_sampler);

        destroyVulkanSurface(_instance, _surface);

        for (const auto& frame : _frameData) {
            _logicalDevice.destroyCommandPool(frame.commandPool);
            _logicalDevice.destroyFence(frame.renderingFence);
            _logicalDevice.destroySemaphore(frame.renderingSemaphore);
            _logicalDevice.destroySemaphore(frame.swapchainSemaphore);
        }

        _allocator.destroy();
        _logicalDevice.destroy();

        if (IsDebugModeEnabled) {
            _instance.destroyDebugUtilsMessengerEXT(_debugUtilsMessenger, nullptr, _extensionDispatch);
        }

        _instance.destroy();
    }

    void VulkanGraphicsDevice::configure(const Window& window) {
        if (window.getFramebufferSize() == Vector2u{}) {
            _isSuspended = true;
            return;
        }

        if (!_isInitialized) {
            _initializeVulkan(window);
            _initializeSwapchain(window);
        } else {
            _destroySwapchain();
            _initializeSwapchain(window);
        }

        _isSuspended = false;
    }

    usize VulkanGraphicsDevice::createTexture(const TextureCreateInfo& createInfo) {
        auto imageIndex = _textures.size();

        auto imageCreateInfo = vk::ImageCreateInfo{};
        imageCreateInfo.extent = vk::Extent3D{ createInfo.width, createInfo.height, 1 };
        imageCreateInfo.format = mapTextureFormatToVulkanFormat(createInfo.format);
        imageCreateInfo.usage = mapTextureUsageToVulkanFlags(createInfo.usage);
        imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
        imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
        imageCreateInfo.imageType = vk::ImageType::e2D;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.mipLevels = 1;
        
        auto vulkanTexture = VulkanTexture{};
        vulkanTexture.allocation = _allocator.allocateImage(imageCreateInfo);

        auto subresourceRange = vk::ImageSubresourceRange{};
        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;

        auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
        imageViewCreateInfo.subresourceRange = subresourceRange;
        imageViewCreateInfo.image = vulkanTexture.allocation.image;
        imageViewCreateInfo.format = imageCreateInfo.format;
        imageViewCreateInfo.viewType = vk::ImageViewType::e2D;

        vulkanTexture.view = _logicalDevice.createImageView(imageViewCreateInfo);
        _textures.push_back(vulkanTexture);

        return imageIndex;
    }

    usize VulkanGraphicsDevice::createPipeline(const PipelineCreateInfo& createInfo) {
        auto vertexShaderModuleCreateInfo = vk::ShaderModuleCreateInfo{};
        vertexShaderModuleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.vertexShaderByteCode.data());
        vertexShaderModuleCreateInfo.codeSize = createInfo.vertexShaderByteCode.size_bytes();

        auto fragmentShaderModuleCreateInfo = vk::ShaderModuleCreateInfo{};
        fragmentShaderModuleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.fragmentShaderByteCode.data());
        fragmentShaderModuleCreateInfo.codeSize = createInfo.fragmentShaderByteCode.size_bytes();

        const auto vertexShaderModule = _logicalDevice.createShaderModule(vertexShaderModuleCreateInfo);
        const auto fragmentShaderModule = _logicalDevice.createShaderModule(fragmentShaderModuleCreateInfo);

        auto vertexShaderStageCreateInfo = vk::PipelineShaderStageCreateInfo{};
        vertexShaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertexShaderStageCreateInfo.module = vertexShaderModule;
        vertexShaderStageCreateInfo.pName = "main";

        auto fragmentShaderStageCreateInfo = vk::PipelineShaderStageCreateInfo{};
        fragmentShaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragmentShaderStageCreateInfo.module = fragmentShaderModule;
        fragmentShaderStageCreateInfo.pName = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStageCreateInfos{};
        shaderStageCreateInfos[0] = vertexShaderStageCreateInfo;
        shaderStageCreateInfos[1] = fragmentShaderStageCreateInfo;

        const auto vertexLayoutInfo = getVertexLayoutInfo(createInfo);

        vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
        vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexLayoutInfo.bindingDescription;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexLayoutInfo.attributeDescriptions.data();
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<u32>(vertexLayoutInfo.attributeDescriptions.size());

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
        inputAssemblyCreateInfo.primitiveRestartEnable = false;
        inputAssemblyCreateInfo.topology = vk::PrimitiveTopology::eTriangleList;

        auto descriptorSetLayouts = ArrayVector<vk::DescriptorSetLayout, Constants::MaximumBindGroupCount>{};
        for (auto index = 0; index < createInfo.bindGroupLayout.size(); index++) {
            const auto descriptorSetLayoutId = createInfo.bindGroupLayout[index];
            descriptorSetLayouts.push(_descriptorCache.getDescriptorSetLayoutHandle(descriptorSetLayoutId));
        }

        vk::PipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
        layoutCreateInfo.setLayoutCount = static_cast<u32>(descriptorSetLayouts.size());

        vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
        rasterizerCreateInfo.lineWidth = 1.f;
        rasterizerCreateInfo.polygonMode = vk::PolygonMode::eFill;
        rasterizerCreateInfo.cullMode = vk::CullModeFlagBits::eBack;
        rasterizerCreateInfo.frontFace = vk::FrontFace::eClockwise;

        vk::PipelineMultisampleStateCreateInfo multisampleCreateInfo{};
        multisampleCreateInfo.sampleShadingEnable = false;
        multisampleCreateInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
        multisampleCreateInfo.minSampleShading = 1.0f;
        multisampleCreateInfo.pSampleMask = nullptr;
        multisampleCreateInfo.alphaToCoverageEnable = false;
        multisampleCreateInfo.alphaToOneEnable = false;

        vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
        depthStencilCreateInfo.depthTestEnable = false;
        depthStencilCreateInfo.depthWriteEnable = false;
        depthStencilCreateInfo.depthCompareOp = vk::CompareOp::eNever;
        depthStencilCreateInfo.depthBoundsTestEnable = false;
        depthStencilCreateInfo.stencilTestEnable = false;
        depthStencilCreateInfo.front = vk::StencilOpState{};
        depthStencilCreateInfo.back = vk::StencilOpState{};
        depthStencilCreateInfo.minDepthBounds = 0.f;
        depthStencilCreateInfo.maxDepthBounds = 1.f;

        auto colorWireMask = vk::ColorComponentFlagBits::eR
            | vk::ColorComponentFlagBits::eG
            | vk::ColorComponentFlagBits::eB
            | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{};
        colorBlendAttachmentState.colorWriteMask = colorWireMask;
        colorBlendAttachmentState.blendEnable = true;
        colorBlendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        colorBlendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        colorBlendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
        colorBlendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eZero;
        colorBlendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
        colorBlendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;

        vk::PipelineColorBlendStateCreateInfo colorBlendAttachmentCreateInfo{};
        colorBlendAttachmentCreateInfo.logicOpEnable = false;
        colorBlendAttachmentCreateInfo.logicOp = vk::LogicOp::eCopy;
        colorBlendAttachmentCreateInfo.attachmentCount = 1;
        colorBlendAttachmentCreateInfo.pAttachments = &colorBlendAttachmentState;

        vk::PipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.colorAttachmentCount = 1;
        renderingCreateInfo.pColorAttachmentFormats = &RenderTargetFormat;

        vk::DynamicState dynamicStates[] = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        auto dynamicStateCreateInfo = vk::PipelineDynamicStateCreateInfo{};
        dynamicStateCreateInfo.pDynamicStates = dynamicStates;
        dynamicStateCreateInfo.dynamicStateCount = 2;

        auto viewportStateCreateInfo = vk::PipelineViewportStateCreateInfo{};
        viewportStateCreateInfo.scissorCount = 1;
        viewportStateCreateInfo.viewportCount = 1;

        const auto pipelineLayout = _logicalDevice.createPipelineLayout(layoutCreateInfo);

        auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo{};
        pipelineCreateInfo.pNext = &renderingCreateInfo;
        pipelineCreateInfo.stageCount = static_cast<u32>(shaderStageCreateInfos.size());
        pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
        pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
        pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendAttachmentCreateInfo;
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.layout = pipelineLayout;

        const auto pipelineResult = _logicalDevice.createGraphicsPipeline(nullptr, pipelineCreateInfo);
        if (pipelineResult.result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        _logicalDevice.destroyShaderModule(vertexShaderModule);
        _logicalDevice.destroyShaderModule(fragmentShaderModule);

        auto pipelineIndex = _pipelines.size();
        _pipelines.emplace_back(pipelineResult.value, pipelineLayout);

        return pipelineIndex;
    }

    usize VulkanGraphicsDevice::createBuffer(const BufferCreateInfo& createInfo) {
        auto bufferIndex = _buffers.size();

        auto bufferCreateInfo = vk::BufferCreateInfo{};
        bufferCreateInfo.size = createInfo.size;
        bufferCreateInfo.usage = mapBufferTypeToVulkanFlags(createInfo.type);

        _buffers.emplace_back(_allocator.allocateBuffer(bufferCreateInfo));

        return bufferIndex;
    }

    usize VulkanGraphicsDevice::createBindGroup(const BindGroup& group) {
        const auto bindGroupIndex = _bindGroups.size();

        auto descriptorIdentifier = VulkanDescriptorSetIdentifier{};
        descriptorIdentifier.layoutId = _descriptorCache.getOrCreateDescriptorSetLayout(group);
        descriptorIdentifier.bindingResourceId0 = Constants::MaximumIdValue;
        descriptorIdentifier.bindResourceType0 = group.type0;

        _bindGroups.push_back(descriptorIdentifier);

        return bindGroupIndex;
    }

    void VulkanGraphicsDevice::destroyTexture(usize textureId) {
        auto& texture = _textures[textureId];
        if (texture.view != nullptr) {
            _allocator.destroyTexture(texture.allocation);
            _logicalDevice.destroyImageView(texture.view);
            texture.view = nullptr;
        }
    }

    void VulkanGraphicsDevice::destroyBuffer(usize bufferId) {
        auto& buffer = _buffers[bufferId];
        if (buffer.allocation.buffer != nullptr) {
            _allocator.destroyBuffer(buffer.allocation);
            buffer.allocation.buffer = nullptr;
        }
    }

    void VulkanGraphicsDevice::setTextureData(usize textureId, std::span<const std::byte> data, u32 width, u32 height) {
        _logicalDevice.resetFences(_transferFence);

        auto& stagingBuffer = _buffers[_stagingCache.getStagingBuffer(*this, data.size_bytes(), StagingFrameIndex)];
        auto& texture = _textures[textureId];

        auto stagingBufferHandle = stagingBuffer.allocation.buffer;
        auto textureHandle = texture.allocation.image;

        auto commandBufferBeginInfo = vk::CommandBufferBeginInfo{};
        commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        _transferCommandBuffer.reset();
        _transferCommandBuffer.begin(commandBufferBeginInfo);

        std::copy(data.begin(), data.end(), _allocator.getMappedData(stagingBuffer.allocation));

        const auto commandBufferHelper = VulkanCommandBufferHelper{ _transferCommandBuffer };
        commandBufferHelper.transitionImage(textureHandle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        auto copyRegion = vk::BufferImageCopy{};;
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = vk::Extent3D{ width, height, 1 };

        _transferCommandBuffer.copyBufferToImage(stagingBufferHandle, textureHandle, vk::ImageLayout::eTransferDstOptimal, copyRegion);

        commandBufferHelper.transitionImage(textureHandle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eReadOnlyOptimal);

        _transferCommandBuffer.end();

        auto commandBufferSubmitInfo = vk::CommandBufferSubmitInfo{};
        commandBufferSubmitInfo.commandBuffer = _transferCommandBuffer;

        auto submitInfo = vk::SubmitInfo2{};
        submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
        submitInfo.commandBufferInfoCount = 1;

        _queue.submit2(submitInfo, _transferFence);

        const auto waitResult = _logicalDevice.waitForFences(_transferFence, true, Constants::MaximumIdValue);
        vk::resultCheck(waitResult, "waitForFences");

        _stagingCache.clearFrameStagingBufferLocks(StagingFrameIndex);
    }

    void VulkanGraphicsDevice::setBufferData(usize bufferId, std::span<const std::byte> data) {
        _logicalDevice.resetFences(_transferFence);

        auto& stagingBuffer = _buffers[_stagingCache.getStagingBuffer(*this, data.size_bytes(), StagingFrameIndex)];
        auto& buffer = _buffers[bufferId];

        auto stagingBufferHandle = stagingBuffer.allocation.buffer;
        auto bufferHandle = buffer.allocation.buffer;

        auto commandBufferBeginInfo = vk::CommandBufferBeginInfo{};
        commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        _transferCommandBuffer.reset();
        _transferCommandBuffer.begin(commandBufferBeginInfo);

        std::copy(data.begin(), data.end(), _allocator.getMappedData(stagingBuffer.allocation));

        auto bufferCopy = vk::BufferCopy{};
        bufferCopy.size = data.size_bytes();

        _transferCommandBuffer.copyBuffer(stagingBufferHandle, bufferHandle, bufferCopy);
        _transferCommandBuffer.end();

        auto commandBufferSubmitInfo = vk::CommandBufferSubmitInfo{};
        commandBufferSubmitInfo.commandBuffer = _transferCommandBuffer;

        auto submitInfo = vk::SubmitInfo2{};
        submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
        submitInfo.commandBufferInfoCount = 1;

        _queue.submit2(submitInfo, _transferFence);

        const auto waitResult = _logicalDevice.waitForFences(_transferFence, true, Constants::MaximumIdValue);
        vk::resultCheck(waitResult, "waitForFences");

        _stagingCache.clearFrameStagingBufferLocks(StagingFrameIndex);
    }

    usize VulkanGraphicsDevice::getCurrentFrameIndex() const {
        return _frameIndex;
    }

    Vector2u VulkanGraphicsDevice::getCurrentViewport() const {
        return Vector2u{ _viewport.width, _viewport.height };
    }

    bool VulkanGraphicsDevice::isSuspended() const {
        return _isSuspended;
    }

    void VulkanGraphicsDevice::beginCommands() {
        auto& frame = _frameData[_frameIndex];

        vk::resultCheck(_logicalDevice.waitForFences(frame.renderingFence, true, Constants::MaximumIdValue), "waitForFences");
        _logicalDevice.resetFences(frame.renderingFence);

        _descriptorCache.clearFrameDescriptorSetLocks(_frameIndex);
        _stagingCache.clearFrameStagingBufferLocks(_frameIndex);

        frame.commandBuffer.reset();
        frame.commandBuffer.begin(vk::CommandBufferBeginInfo{});

        const auto& renderTarget = _textures[_renderTargetId].allocation.image;

        const auto commandBufferHelper = VulkanCommandBufferHelper{ frame.commandBuffer };
        commandBufferHelper.transitionImage(renderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

        auto viewport = vk::Viewport{};
        viewport.width = static_cast<float>(_viewport.width);
        viewport.height = static_cast<float>(_viewport.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        auto scissor = vk::Rect2D{};
        scissor.extent = _viewport;

        frame.commandBuffer.setViewport(0, viewport);
        frame.commandBuffer.setScissor(0, scissor);
    }

    void VulkanGraphicsDevice::beginRenderPass() {
        const auto& renderTargetTexture = _textures[_renderTargetId];

        auto colorAttachment = vk::RenderingAttachmentInfo{};
        colorAttachment.imageView = renderTargetTexture.view;
        colorAttachment.imageLayout = vk::ImageLayout::eGeneral;
        colorAttachment.clearValue = vk::ClearColorValue{ 0.3f, 0.3f, 0.3f, 1.0f };
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

        auto renderingInfo = vk::RenderingInfo{};
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.renderArea = vk::Rect2D{ {}, _viewport };
        renderingInfo.layerCount = 1;

        auto& frame = _frameData[_frameIndex];
        frame.commandBuffer.beginRendering(renderingInfo);
    }

    void VulkanGraphicsDevice::copyBuffer(usize bufferId, std::span<const std::byte> data) {
        auto& frame = _frameData[_frameIndex];

        auto& stagingBuffer = _buffers[_stagingCache.getStagingBuffer(*this, data.size_bytes(), 0)];
        auto& buffer = _buffers[bufferId];

        auto stagingBufferHandle = stagingBuffer.allocation.buffer;
        auto bufferHandle = buffer.allocation.buffer;

        std::copy(data.begin(), data.end(), _allocator.getMappedData(stagingBuffer.allocation));

        auto memoryBarrier = vk::MemoryBarrier2{};
        memoryBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        memoryBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        memoryBarrier.dstStageMask = vk::PipelineStageFlagBits2::eIndexInput | vk::PipelineStageFlagBits2::eVertexInput;
        memoryBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead;

        auto dependencyInfo = vk::DependencyInfo{};
        dependencyInfo.pMemoryBarriers = &memoryBarrier;
        dependencyInfo.memoryBarrierCount = 1;

        auto bufferCopy = vk::BufferCopy{};
        bufferCopy.size = data.size_bytes();

        frame.commandBuffer.copyBuffer(stagingBufferHandle, bufferHandle, bufferCopy);
        frame.commandBuffer.pipelineBarrier2(dependencyInfo);
    }

    void VulkanGraphicsDevice::drawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset) {
        auto& frame = _frameData[_frameIndex];
        frame.commandBuffer.drawIndexed(indexCount, 1, indexOffset, vertexOffset, 0);
    }

    void VulkanGraphicsDevice::setBindGroupBinding(usize bindGroupId, u32 bindingIndex, usize resourceId) {
        const auto bindingResourceType = _descriptorCache.getBindGroupBindingType(bindGroupId, bindingIndex);

        auto& descriptorSetIdentifier = _bindGroups[bindGroupId];
        descriptorSetIdentifier.bindingResourceId0 = resourceId;
        descriptorSetIdentifier.frameId = getCurrentFrameIndex();

        const auto descriptorId = _descriptorCache.getOrAllocateDescriptorSet(descriptorSetIdentifier);
        if (_descriptorCache.isDescriptorSetBindingWritenTo(bindingIndex, descriptorId)) {
            return;
        }

        if (bindingResourceType == BindGroupElementType::UniformBuffer) {
            const auto& buffer = _buffers[resourceId];

            auto uniformUpdateInfo = UniformUpdateInfo{};
            uniformUpdateInfo.bindingIndex = bindingIndex;
            uniformUpdateInfo.buffer = buffer.allocation.buffer;
            uniformUpdateInfo.descriptorSetId = descriptorId;
            uniformUpdateInfo.resourceId = resourceId;

            _descriptorCache.updateUniformDescriptorSetBinding(uniformUpdateInfo);
        } else if (bindingResourceType == BindGroupElementType::SamplerTexture) {
            const auto& texture = _textures[resourceId];

            auto textureUpdateInfo = TextureUpdateInfo{};
            textureUpdateInfo.bindingIndex = bindingIndex;
            textureUpdateInfo.descriptorSetId = descriptorId;
            textureUpdateInfo.image = texture.view;
            textureUpdateInfo.sampler = _sampler;
            textureUpdateInfo.resourceId = resourceId;

            _descriptorCache.updateTextureDescriptorSetBinding(textureUpdateInfo);
        }
    }

    void VulkanGraphicsDevice::setPipeline(usize pipelineId) {
        auto& pipeline = _pipelines[pipelineId];

        auto& frame = _frameData[_frameIndex];
        frame.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.handle);
    }

    void VulkanGraphicsDevice::setVertexBuffer(usize vertexBufferId) {
        auto vertexBuffer = _buffers[vertexBufferId].allocation.buffer;

        auto& frame = _frameData[_frameIndex];
        frame.commandBuffer.bindVertexBuffers(0, vertexBuffer, vk::DeviceSize{ 0 });
    }

    void VulkanGraphicsDevice::setIndexBuffer(usize indexBufferId) {
        auto indexBuffer = _buffers[indexBufferId].allocation.buffer;

        auto& frame = _frameData[_frameIndex];
        frame.commandBuffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint32);
    }

    void VulkanGraphicsDevice::setBindGroup(usize bindGroupId, usize pipelineId, u32 slot) {
        auto& descriptorSetIdentifier = _bindGroups[bindGroupId];
        descriptorSetIdentifier.frameId = getCurrentFrameIndex();

        const auto descriptorSetId = _descriptorCache.getOrAllocateDescriptorSet(descriptorSetIdentifier);
        const auto descriptorSet = _descriptorCache.getDescriptorSetHandle(descriptorSetId);

        const auto pipelineLayout = _pipelines[pipelineId].layout;

        auto& frame = _frameData[_frameIndex];
        frame.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, slot, descriptorSet, nullptr);

        _descriptorCache.lockDescriptorSet(descriptorSetId);
    }

    PresentationResultType VulkanGraphicsDevice::endAndPresent() {
        auto& frame = _frameData[_frameIndex];
        frame.commandBuffer.endRendering();

        const auto imageIndexResult = _logicalDevice.acquireNextImageKHR(_swapchain, Constants::MaximumIdValue, frame.swapchainSemaphore);
        const auto imageIndex = imageIndexResult.value;

        if (imageIndexResult.result != vk::Result::eSuccess) {
            _isSuspended = true;
            frame.commandBuffer.end();
            return PresentationResultType::SurfaceOutOfDate;
        }

        auto& renderTarget = _textures[_renderTargetId].allocation.image;
        auto& swapchainImage = _swapchainImages[imageIndex];

        const auto commandBufferHelper = VulkanCommandBufferHelper{ frame.commandBuffer };
        commandBufferHelper.transitionImage(renderTarget, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
        commandBufferHelper.transitionImage(swapchainImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        commandBufferHelper.copyImageToImage(renderTarget, swapchainImage, _viewport, _viewport);
        commandBufferHelper.transitionImage(swapchainImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

        frame.commandBuffer.end();

        auto semaphoreWaitInfo = vk::SemaphoreSubmitInfo{};
        semaphoreWaitInfo.semaphore = frame.swapchainSemaphore;
        semaphoreWaitInfo.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        semaphoreWaitInfo.value = 1;

        auto semaphoreSignalInfo = vk::SemaphoreSubmitInfo{};
        semaphoreSignalInfo.semaphore = frame.renderingSemaphore;
        semaphoreSignalInfo.stageMask = vk::PipelineStageFlagBits2::eAllGraphics;
        semaphoreSignalInfo.value = 1;

        auto commandBufferSubmitInfo = vk::CommandBufferSubmitInfo{};
        commandBufferSubmitInfo.commandBuffer = frame.commandBuffer;

        auto submitInfo = vk::SubmitInfo2{};
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &semaphoreWaitInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &semaphoreSignalInfo;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

        _queue.submit2(submitInfo, frame.renderingFence);

        auto presentInfo = vk::PresentInfoKHR{};
        presentInfo.pSwapchains = &_swapchain;
        presentInfo.swapchainCount = 1;
        presentInfo.pWaitSemaphores = &frame.renderingSemaphore;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pImageIndices = &imageIndex;

        vk::resultCheck(_queue.presentKHR(presentInfo), "presentKHR");

        _frameIndex = (_frameIndex + 1) % Constants::ConcurrentFrameCount;

        return PresentationResultType::Success;
    }

    void VulkanGraphicsDevice::_initializeVulkan(const Window& window) {
        const auto instance = createVulkanInstance();

        _instance = instance.instance;
        _debugUtilsMessenger = instance.debugUtilsMessenger;
        _extensionDispatch = instance.extensionDispatch;

        _surface = createVulkanSurface(_instance, window.getNativeHandle());

        const auto device = createVulkanDevice(_instance, _surface);

        _physicalDevice = device.physicalDevice;
        _logicalDevice = device.logicalDevice;
        _queueIndex = device.graphicsQueueIndex;
        _queue = device.graphicsQueue;

        _descriptorCache.initialize(_logicalDevice);

        auto resourceAllocatorCreateInfo = VulkanAllocatorCreateInfo{};
        resourceAllocatorCreateInfo.device = _logicalDevice;
        resourceAllocatorCreateInfo.physicalDevice = _physicalDevice;
        resourceAllocatorCreateInfo.instance = _instance;

        _allocator = VulkanAllocator{ resourceAllocatorCreateInfo };

        auto commandPoolCreateInfo = vk::CommandPoolCreateInfo{};
        commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        commandPoolCreateInfo.queueFamilyIndex = _queueIndex;

        _transferCommandPool = _logicalDevice.createCommandPool(commandPoolCreateInfo);

        auto fenceCreateInfo = vk::FenceCreateInfo{};
        fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        _transferFence = _logicalDevice.createFence(fenceCreateInfo);

        auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{};
        commandBufferAllocateInfo.commandPool = _transferCommandPool;
        commandBufferAllocateInfo.commandBufferCount = 1;
        commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;

        _transferCommandBuffer = _logicalDevice.allocateCommandBuffers(commandBufferAllocateInfo).front();

        for (auto& frame : _frameData) {
            frame.commandPool = _logicalDevice.createCommandPool(commandPoolCreateInfo);

            auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{};
            commandBufferAllocateInfo.commandPool = frame.commandPool;
            commandBufferAllocateInfo.commandBufferCount = 1;
            commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;

            frame.commandBuffer = _logicalDevice.allocateCommandBuffers(commandBufferAllocateInfo).front();
            frame.renderingFence = _logicalDevice.createFence(fenceCreateInfo);
            frame.renderingSemaphore = _logicalDevice.createSemaphore({});
            frame.swapchainSemaphore = _logicalDevice.createSemaphore({});
        }

        auto samplerCreateInfo = vk::SamplerCreateInfo{};
        samplerCreateInfo.magFilter = vk::Filter::eLinear;
        samplerCreateInfo.minFilter = vk::Filter::eLinear;
        samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        samplerCreateInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
        samplerCreateInfo.unnormalizedCoordinates = false;
        samplerCreateInfo.compareEnable = false;
        samplerCreateInfo.compareOp = vk::CompareOp::eAlways;

        samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 0.0f;

        _sampler = _logicalDevice.createSampler(samplerCreateInfo);

        _isInitialized = true;
    }

    void VulkanGraphicsDevice::_initializeSwapchain(const Window& window) {
        const auto windowSize = window.getFramebufferSize();

        auto device = VulkanDevice{};
        device.logicalDevice = _logicalDevice;
        device.physicalDevice = _physicalDevice;
        device.graphicsQueueIndex = _queueIndex;

        auto swapchain = createVulkanSwapchain(device, _surface, windowSize.x, windowSize.y);

        _swapchain = swapchain.swapchain;
        _swapchainImages = swapchain.images;
        _swapchainViews = std::move(swapchain.views);

        auto textureCreateInfo = TextureCreateInfo{};
        textureCreateInfo.format = TextureFormatType::RGBA16SFloat;
        textureCreateInfo.usage = TextureUsageType::RenderTarget;
        textureCreateInfo.width = windowSize.x;
        textureCreateInfo.height = windowSize.x;

        _renderTargetId = createTexture(textureCreateInfo);
        _viewport = vk::Extent2D{ windowSize.x, windowSize.y };
    }

    void VulkanGraphicsDevice::_destroySwapchain() {
        _logicalDevice.waitIdle();

        _swapchainViews.clear();
        _swapchainImages.clear();

        _logicalDevice.destroySwapchainKHR(_swapchain);

        destroyTexture(_renderTargetId);
    }
}
