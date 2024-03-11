#define VMA_IMPLEMENTATION

#include "VulkanAllocator.h"

namespace Pandora::Implementation {

    VulkanAllocator::VulkanAllocator(const VulkanAllocatorCreateInfo& createInfo) {
        auto allocatorCreateInfo = VmaAllocatorCreateInfo{};
        allocatorCreateInfo.physicalDevice = createInfo.physicalDevice;
        allocatorCreateInfo.device = createInfo.device;
        allocatorCreateInfo.instance = createInfo.instance;

        const auto allocatorResult = vmaCreateAllocator(&allocatorCreateInfo, &_allocator);
        vk::resultCheck(vk::Result{ allocatorResult }, "vmaCreateAllocator");
    }

    void VulkanAllocator::destroy() {
        vmaDestroyAllocator(_allocator);
    }

    VulkanBufferAllocation VulkanAllocator::allocateBuffer(const vk::BufferCreateInfo& bufferCreateInfo) {
        auto allocationCreateInfo = VmaAllocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (bufferCreateInfo.usage & vk::BufferUsageFlagBits::eTransferSrc) {
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        auto createInfo = reinterpret_cast<const VkBufferCreateInfo*>(&bufferCreateInfo);
        auto buffer = VkBuffer{};

        auto allocation = VmaAllocation{};
        auto allocationInfo = VmaAllocationInfo{};

        const auto result = vmaCreateBuffer(_allocator, createInfo, &allocationCreateInfo, &buffer, &allocation, &allocationInfo);
        vk::resultCheck(vk::Result{ result }, "vmaCreateBuffer");

        auto allocationResult = VulkanBufferAllocation{};
        allocationResult.allocationInfo = allocationInfo;
        allocationResult.allocation = allocation;
        allocationResult.buffer = vk::Buffer{ buffer };

        return allocationResult;
    }

    VulkanImageAllocation VulkanAllocator::allocateImage(const vk::ImageCreateInfo& imageCreateInfo) {
        auto imageAllocateInfo = VmaAllocationCreateInfo{};
        imageAllocateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageAllocateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        auto createInfo = reinterpret_cast<const VkImageCreateInfo*>(&imageCreateInfo);
        auto allocation = VmaAllocation{};
        auto image = VkImage{};

        const auto result = vmaCreateImage(_allocator, createInfo, &imageAllocateInfo, &image, &allocation, nullptr);
        vk::resultCheck(vk::Result{ result }, "vmaCreateImage");

        auto allocationResult = VulkanImageAllocation{};
        allocationResult.allocation = allocation;
        allocationResult.image = vk::Image{ image };

        return allocationResult;
    }

    void VulkanAllocator::destroyTexture(const VulkanImageAllocation& allocation) {
        vmaDestroyImage(_allocator, allocation.image, allocation.allocation);
    }

    void VulkanAllocator::destroyBuffer(const VulkanBufferAllocation& allocation) {
        vmaDestroyBuffer(_allocator, allocation.buffer, allocation.allocation);
    }

    std::byte* VulkanAllocator::getMappedData(VulkanBufferAllocation& allocation) {
        return reinterpret_cast<std::byte*>(allocation.allocationInfo.pMappedData);;
    }
}
