#pragma once

#include "Pandora/Graphics/GraphicsDevice.h"

#include <vulkan/vulkan.hpp>
#include <vma/vk_mem_alloc.h>

namespace Pandora::Implementation {

    struct VulkanBufferAllocation {
        vk::Buffer buffer{};
        VmaAllocation allocation{};
        VmaAllocationInfo allocationInfo{};
    };

    struct VulkanImageAllocation {
        vk::Image image{};
        VmaAllocation allocation{};
    };

    struct VulkanAllocatorCreateInfo {
        vk::PhysicalDevice physicalDevice{};
        vk::Instance instance{};
        vk::Device device{};
    };

    class VulkanAllocator {
    public:
        VulkanAllocator(const VulkanAllocatorCreateInfo& createInfo);
        VulkanAllocator() = default;

        void destroy();

        VulkanBufferAllocation allocateBuffer(const vk::BufferCreateInfo& bufferCreateInfo);
        VulkanImageAllocation allocateImage(const vk::ImageCreateInfo& imageCreateInfo);

        void destroyTexture(const VulkanImageAllocation& allocation);
        void destroyBuffer(const VulkanBufferAllocation& allocation);

        std::byte* getMappedData(VulkanBufferAllocation& allocation);
    private:
        VmaAllocator _allocator{};
    };
}
