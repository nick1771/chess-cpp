#include "VulkanResourceAllocator.h"
#include "VulkanCommandBufferHelper.h"

namespace Pandora::Implementation {

    void VulkanResourceAllocator::initialize(const VulkanResourceAllocatorCreateInfo& createInfo) {
        _device = createInfo.device;
        _queue = createInfo.queue;

        auto allocatorCreateInfo = VmaAllocatorCreateInfo{};
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocatorCreateInfo.physicalDevice = createInfo.physicalDevice;
        allocatorCreateInfo.device = createInfo.device;
        allocatorCreateInfo.instance = createInfo.instance;

        const auto allocatorResult = vmaCreateAllocator(&allocatorCreateInfo, &_allocator);
        vk::resultCheck(static_cast<vk::Result>(allocatorResult), "vmaCreateAllocator");

        auto transferCommandPoolCreateInfo = vk::CommandPoolCreateInfo{};
        transferCommandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        transferCommandPoolCreateInfo.queueFamilyIndex = createInfo.graphicsQueueIndex;

        _transferCommandPool = _device.createCommandPool(transferCommandPoolCreateInfo);

        auto fenceCreateInfo = vk::FenceCreateInfo{};
        fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        _transferFence = _device.createFence(fenceCreateInfo);

        auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{};
        commandBufferAllocateInfo.commandPool = _transferCommandPool;
        commandBufferAllocateInfo.commandBufferCount = 1;
        commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;

        _transferCommandBuffer = _device.allocateCommandBuffers(commandBufferAllocateInfo).front();
        _stagingBufferId = createBuffer(MaximumBufferSizeBytes, VulkanBufferUsageType::Staging);

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

        _sampler = _device.createSampler(samplerCreateInfo);
    }

    u32 VulkanResourceAllocator::createBuffer(usize size, VulkanBufferUsageType usageType) {
        auto bufferCreateInfo = vk::BufferCreateInfo{};
        bufferCreateInfo.size = size;

        auto bufferAllocationCreateInfo = VmaAllocationCreateInfo{};
        bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (usageType == VulkanBufferUsageType::Staging) {
            bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
            bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        } else if (usageType == VulkanBufferUsageType::Vertex) {
            bufferCreateInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer
                | vk::BufferUsageFlagBits::eTransferDst;
        } else if (usageType == VulkanBufferUsageType::Uniform) {
            bufferCreateInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer
                | vk::BufferUsageFlagBits::eTransferDst;
        } else if (usageType == VulkanBufferUsageType::Index) {
            bufferCreateInfo.usage = vk::BufferUsageFlagBits::eIndexBuffer
                | vk::BufferUsageFlagBits::eTransferDst;
        }

        auto resource = VulkanBufferResource{};
        auto createResult = vmaCreateBuffer(
            _allocator,
            reinterpret_cast<const VkBufferCreateInfo*>(&bufferCreateInfo),
            &bufferAllocationCreateInfo,
            reinterpret_cast<VkBuffer*>(&resource.handle),
            &resource.allocation,
            &resource.allocationInfo
        );

        vk::resultCheck(static_cast<vk::Result>(createResult), "vmaCreateBuffer");
        _resources.push_back(resource);

        return _resources.size() - 1;
    }

    u32 VulkanResourceAllocator::createTexture(u32 width, u32 height, vk::Format format, VulkanTextureUsageType usageType) {
        auto resource = VulkanTextureResource{};
        resource.format = format;
        resource.extent = vk::Extent2D{ width, height };
        resource.sampler = _sampler;

        auto imageCreateInfo = vk::ImageCreateInfo{};
        imageCreateInfo.imageType = vk::ImageType::e2D;
        imageCreateInfo.format = resource.format;
        imageCreateInfo.extent = vk::Extent3D{ resource.extent, 1 };
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
        imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
        imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;

        if (usageType == VulkanTextureUsageType::RenderTarget) {
            imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransferDst
                | vk::ImageUsageFlagBits::eTransferSrc
                | vk::ImageUsageFlagBits::eStorage
                | vk::ImageUsageFlagBits::eColorAttachment;
        } else {
            imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransferDst
                | vk::ImageUsageFlagBits::eSampled;
        }

        auto imageAllocateInfo = VmaAllocationCreateInfo{};
        imageAllocateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageAllocateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        auto createResult = vmaCreateImage(
            _allocator,
            reinterpret_cast<const VkImageCreateInfo*>(&imageCreateInfo),
            &imageAllocateInfo,
            reinterpret_cast<VkImage*>(&resource.handle),
            &resource.allocation,
            nullptr
        );

        vk::resultCheck(static_cast<vk::Result>(createResult), "vmaCreateImage");

        auto subresourceRange = vk::ImageSubresourceRange{};
        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;

        auto imageViewCreateInfo = vk::ImageViewCreateInfo{};
        imageViewCreateInfo.subresourceRange = subresourceRange;
        imageViewCreateInfo.image = resource.handle;
        imageViewCreateInfo.format = resource.format;
        imageViewCreateInfo.viewType = vk::ImageViewType::e2D;

        resource.view = _device.createImageView(imageViewCreateInfo);
        _resources.push_back(resource);

        return _resources.size() - 1;
    }

    void VulkanResourceAllocator::setTextureData(u32 textureId, const void* data, usize size) {
        _device.resetFences(_transferFence);

        auto& stagingBuffer = getResource<VulkanBufferResource>(_stagingBufferId);
        auto& resourceBuffer = getResource<VulkanTextureResource>(textureId);

        auto commandBufferBeginInfo = vk::CommandBufferBeginInfo{};
        commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        _transferCommandBuffer.reset();
        _transferCommandBuffer.begin(commandBufferBeginInfo);

        memcpy(stagingBuffer.allocationInfo.pMappedData, data, size);

        const auto commandBufferHelper = VulkanCommandBufferHelper{ _transferCommandBuffer };
        commandBufferHelper.transitionImage(resourceBuffer.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        auto copyRegion = vk::BufferImageCopy{};;
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = vk::Extent3D{ resourceBuffer.extent, 1 };

        _transferCommandBuffer.copyBufferToImage(stagingBuffer.handle, resourceBuffer.handle, vk::ImageLayout::eTransferDstOptimal, copyRegion);

        commandBufferHelper.transitionImage(resourceBuffer.handle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eReadOnlyOptimal);

        _transferCommandBuffer.end();

        auto commandBufferSubmitInfo = vk::CommandBufferSubmitInfo{};
        commandBufferSubmitInfo.commandBuffer = _transferCommandBuffer;

        auto submitInfo = vk::SubmitInfo2{};
        submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
        submitInfo.commandBufferInfoCount = 1;

        _queue.submit2(submitInfo, _transferFence);

        vk::resultCheck(_device.waitForFences(_transferFence, true, UINT64_MAX), "waitForFences");
    }

    void VulkanResourceAllocator::setBufferData(u32 bufferId, const void* data, usize size) {
        _device.resetFences(_transferFence);

        auto& stagingBuffer = getResource<VulkanBufferResource>(_stagingBufferId);
        auto& resourceBuffer = getResource<VulkanBufferResource>(bufferId);

        auto commandBufferBeginInfo = vk::CommandBufferBeginInfo{};
        commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        _transferCommandBuffer.reset();
        _transferCommandBuffer.begin(commandBufferBeginInfo);
        
        memcpy(stagingBuffer.allocationInfo.pMappedData, data, size);

        auto bufferCopy = vk::BufferCopy{};
        bufferCopy.size = size;

        _transferCommandBuffer.copyBuffer(stagingBuffer.handle, resourceBuffer.handle, bufferCopy);
        _transferCommandBuffer.end();

        auto commandBufferSubmitInfo = vk::CommandBufferSubmitInfo{};
        commandBufferSubmitInfo.commandBuffer = _transferCommandBuffer;

        auto submitInfo = vk::SubmitInfo2{};
        submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
        submitInfo.commandBufferInfoCount = 1;

        _queue.submit2(submitInfo, _transferFence);

        vk::resultCheck(_device.waitForFences(_transferFence, true, UINT64_MAX), "waitForFences");
    }

    void VulkanResourceAllocator::destroyTexture(u32 textureId) {
        auto& resourceBuffer = getResource<VulkanTextureResource>(textureId);
        _device.destroyImageView(resourceBuffer.view);
        vmaDestroyImage(_allocator, resourceBuffer.handle, resourceBuffer.allocation);
    }
}
