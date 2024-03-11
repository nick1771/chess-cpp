#pragma once

#include "Pandora/Graphics/GraphicsDevice.h"
#include "Pandora/Mathematics/Vector.h"

#include "VulkanDescriptorCache.h"
#include "VulkanExtensionDispatch.h"
#include "VulkanStagingBufferCache.h"
#include "VulkanAllocator.h"

namespace Pandora::Implementation {

    struct VulkanBuffer {
        VulkanBufferAllocation allocation{};
    };

    struct VulkanTexture {
        vk::ImageView view{};
        VulkanImageAllocation allocation{};
    };

    struct VulkanPipeline {
        vk::Pipeline handle{};
        vk::PipelineLayout layout{};
    };

    struct VulkanFrameData {
        vk::CommandBuffer commandBuffer{};
        vk::CommandPool commandPool{};
        vk::Semaphore swapchainSemaphore{};
        vk::Semaphore renderingSemaphore{};
        vk::Fence renderingFence{};
    };

    class VulkanGraphicsDevice {
    public:
        ~VulkanGraphicsDevice();

        void configure(const Window& window);

        usize createTexture(const TextureCreateInfo& createInfo);
        usize createPipeline(const PipelineCreateInfo& createInfo);
        usize createBuffer(const BufferCreateInfo& createInfo);
        usize createBindGroup(const BindGroup& group);

        void destroyTexture(usize textureId);
        void destroyBuffer(usize bufferId);

        void setTextureData(usize textureId, std::span<const std::byte> data, u32 width, u32 height);
        void setBufferData(usize bufferId, std::span<const std::byte> data);

        usize getCurrentFrameIndex() const;
        Vector2u getCurrentViewport() const;

        bool isSuspended() const;

        void beginCommands();
        void beginRenderPass();
        void copyBuffer(usize bufferId, std::span<const std::byte> data);
        void drawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset);
        void setBindGroupBinding(usize bindGroupId, u32 bindingIndex, usize resourceId);
        void setPipeline(usize pipelineId);
        void setVertexBuffer(usize vertexBufferId);
        void setIndexBuffer(usize indexBufferId);
        void setBindGroup(usize bindGroupId, usize pipelineId, u32 slot);
        PresentationResultType endAndPresent();
    private:
        void _initializeVulkan(const Window& window);
        void _initializeSwapchain(const Window& window);
        void _destroySwapchain();

        VulkanAllocator _allocator{};
        VulkanStagingBufferCache _stagingCache{};
        VulkanExtensionDispatch _extensionDispatch{};
        VulkanDescriptorCache _descriptorCache{};

        vk::DebugUtilsMessengerEXT _debugUtilsMessenger{};
        vk::Instance _instance{};

        vk::PhysicalDevice _physicalDevice{};
        vk::Device _logicalDevice{};

        vk::CommandPool _transferCommandPool{};
        vk::CommandBuffer _transferCommandBuffer{};
        vk::Fence _transferFence{};

        u32 _queueIndex{};
        vk::Queue _queue{};

        vk::Extent2D _viewport{};
        vk::Sampler _sampler{};
        vk::SurfaceKHR _surface{};
        vk::SwapchainKHR _swapchain{};
        std::vector<vk::Image> _swapchainImages{};
        std::vector<vk::UniqueImageView> _swapchainViews{};

        std::array<VulkanFrameData, Constants::ConcurrentFrameCount> _frameData{};

        std::vector<VulkanDescriptorSetIdentifier> _bindGroups{};
        std::vector<VulkanBuffer> _buffers{};
        std::vector<VulkanTexture> _textures{};
        std::vector<VulkanPipeline> _pipelines{};

        bool _isInitialized{};
        bool _isSuspended{};

        usize _renderTargetId{};
        usize _frameIndex{};
    };
}
