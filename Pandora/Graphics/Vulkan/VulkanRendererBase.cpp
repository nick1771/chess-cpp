#include "VulkanRendererBase.h"
#include "VulkanSurface.h"

#include <stdexcept>
#include <print>

namespace Pandora::Implementation {

    constexpr std::array<const char*, 3> RequiredInstanceExtensions = {
        "VK_KHR_win32_surface",
        "VK_KHR_surface",
        "VK_EXT_debug_utils",
    };

    constexpr std::array<const char*, 1> RequiredInstanceLayers = {
        "VK_LAYER_KHRONOS_validation",
    };

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

        auto instanceCreateInfo = vk::InstanceCreateInfo{};
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.ppEnabledExtensionNames = RequiredInstanceExtensions.data();
        instanceCreateInfo.enabledExtensionCount = RequiredInstanceExtensions.size();
        instanceCreateInfo.ppEnabledLayerNames = RequiredInstanceLayers.data();
        instanceCreateInfo.enabledLayerCount = RequiredInstanceLayers.size();

        auto debugUtilsMessengerCreateInfo = getDebugUtilsMessengerCreateInfo();
        instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;

        const auto instance = vk::createInstance(instanceCreateInfo);

        const auto extensionDispatch = VulkanExtensionDispatch{ instance };
        const auto debugUtilsMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfo, nullptr, extensionDispatch);

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
        deviceCreateInfo.enabledExtensionCount = RequiredDeviceExtensions.size();
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

    static vk::CommandPool createCommandPool(vk::Device device, u32 graphicsQueueIndex) {
        auto commandPoolCreateInfo = vk::CommandPoolCreateInfo{};
        commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        commandPoolCreateInfo.queueFamilyIndex = graphicsQueueIndex;

        return device.createCommandPool(commandPoolCreateInfo);
    }

    static vk::Fence createFence(vk::Device device) {
        auto fenceCreateInfo = vk::FenceCreateInfo{};
        fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        return device.createFence(fenceCreateInfo);
    }

    static vk::CommandBuffer createCommandBuffer(vk::Device device, vk::CommandPool commandPool) {
        auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{};
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.commandBufferCount = 1;
        commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;

        return device.allocateCommandBuffers(commandBufferAllocateInfo).front();
    }

    void VulkanRendererBase::initialize(const Window& window) {
        const auto instance = createVulkanInstance();

        _instance = instance.instance;
        _debugUtilsMessenger = instance.debugUtilsMessenger;
        _extensionDispatch = instance.extensionDispatch;

        _surface = createVulkanSurface(_instance, window.getNativeHandle());

        const auto device = createVulkanDevice(_instance, _surface);

        _physicalDevice = device.physicalDevice;
        _logicalDevice = device.logicalDevice;
        _graphicsQueueIndex = device.graphicsQueueIndex;
        _graphicsQueue = device.graphicsQueue;

        const auto windowSize = window.getFramebufferSize();
        auto swapchain = createVulkanSwapchain(device, _surface, windowSize.x, windowSize.y);

        _swapchain = swapchain.swapchain;
        _swapchainImages = swapchain.images;
        _swapchainViews = std::move(swapchain.views);

        _descriptorCache.initialize(_logicalDevice);

        auto resourceAllocatorCreateInfo = VulkanResourceAllocatorCreateInfo{};
        resourceAllocatorCreateInfo.device = _logicalDevice;
        resourceAllocatorCreateInfo.physicalDevice = _physicalDevice;
        resourceAllocatorCreateInfo.graphicsQueueIndex = _graphicsQueueIndex;
        resourceAllocatorCreateInfo.instance = _instance;
        resourceAllocatorCreateInfo.queue = _graphicsQueue;

        _resourceAllocator.initialize(resourceAllocatorCreateInfo);

        const auto renderTargetFormat = vk::Format::eR16G16B16A16Sfloat;
        const auto renderTargetUsage = VulkanTextureUsageType::RenderTarget;

        _renderTargetId = _resourceAllocator.createTexture(windowSize.x, windowSize.y, renderTargetFormat, renderTargetUsage);
        _viewport = vk::Extent2D{ windowSize.x, windowSize.y };
    }

    void VulkanRendererBase::resize(u32 width, u32 height) {
        if (width == 0 || height == 0) {
            _isSuspended = true;
            return;
        }

        _logicalDevice.waitIdle();

        _swapchainViews.clear();
        _swapchainImages.clear();

        _logicalDevice.destroySwapchainKHR(_swapchain);

        auto device = VulkanDevice{};
        device.logicalDevice = _logicalDevice;
        device.physicalDevice = _physicalDevice;
        device.graphicsQueueIndex = _graphicsQueueIndex;

        auto swapchain = createVulkanSwapchain(device, _surface, width, height);

        _swapchain = swapchain.swapchain;
        _swapchainImages = swapchain.images;
        _swapchainViews = std::move(swapchain.views);

        _viewport = vk::Extent2D{ width, height };

        _resourceAllocator.destroyTexture(_renderTargetId);

        const auto renderTargetFormat = vk::Format::eR16G16B16A16Sfloat;
        const auto renderTargetUsage = VulkanTextureUsageType::RenderTarget;
        _renderTargetId = _resourceAllocator.createTexture(width, height, renderTargetFormat, renderTargetUsage);

        _isSuspended = false;
    }
}
