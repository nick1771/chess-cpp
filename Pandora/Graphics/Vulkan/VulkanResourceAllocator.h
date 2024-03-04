#pragma once

#include "Pandora/Graphics/Vulkan/VulkanAllocator.h"
#include "Pandora/Pandora.h"

#include <variant>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace Pandora::Implementation {

    enum class VulkanBufferUsageType {
        Staging,
        Uniform,
        Vertex,
        Index,
    };

    enum class VulkanTextureUsageType {
        RenderTarget,
        Sampled
    };

    struct VulkanBufferResource {
        vk::Buffer handle{};
        VmaAllocationInfo allocationInfo{};
        VmaAllocation allocation{};
    };

    struct VulkanTextureResource {
        vk::Image handle{};
        vk::ImageView view{};
        vk::Extent2D extent{};
        vk::Format format{};
        vk::Sampler sampler{};
        VmaAllocation allocation{};
    };

    struct VulkanResourceAllocatorCreateInfo {
        vk::Device device{};
        vk::PhysicalDevice physicalDevice{};
        vk::Instance instance{};
        vk::Queue queue{};
        u32 graphicsQueueIndex{};
    };

    class VulkanResourceAllocator {
    public:
        static constexpr auto MaximumBufferSizeBytes = 1000000;

        void initialize(const VulkanResourceAllocatorCreateInfo& createInfo);

        u32 createBuffer(usize size, VulkanBufferUsageType usageType);
        u32 createTexture(u32 width, u32 height, vk::Format format, VulkanTextureUsageType usageType);

        void setTextureData(u32 textureId, const void* data, usize size);
        void setBufferData(u32 bufferId, const void* data, usize size);

        void destroyTexture(u32 textureId);

        template<class T>
        const T& getResource(u32 resourceId) const;
    private:
        using ResourceStorage = std::vector<std::variant<VulkanBufferResource, VulkanTextureResource>>;

        VmaAllocator _allocator{};
        ResourceStorage _resources{};

        vk::Sampler _sampler{};
        vk::Device _device{};
        vk::Queue _queue{};

        vk::CommandPool _transferCommandPool{};
        vk::CommandBuffer _transferCommandBuffer{};
        vk::Fence _transferFence{};

        u32 _stagingBufferId{};
    };

    template<class T>
    inline const T& VulkanResourceAllocator::getResource(u32 resourceId) const {
        return std::get<T>(_resources[resourceId]);
    }
}
