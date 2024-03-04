#pragma once

#include "VulkanRendererBase.h"

#include "Pandora/Mathematics/Matrix.h"
#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Image.h"

#include <array>
#include <vector>

namespace Pandora::Implementation {

    struct VulkanSpriteBatchGlobalUniformBuffer {
        Matrix4f projection{};
    };

    struct VulkanSpriteBatchEntryDrawCommand {
        u32 indexCount{};
        u32 quadOffset{};
        u32 textureId{};
    };

    struct VulkanSpriteDrawData {
        std::vector<VulkanSpriteBatchEntryDrawCommand> drawCommands{};
        u32 vertexBuffer{};
        u32 vertexStagingBuffer{};
        u32 indexBuffer{};
        u32 indexStagingBuffer{};
    };

    struct VulkanFrameData {
        vk::CommandBuffer commandBuffer{};
        vk::CommandPool commandPool{};
        vk::Semaphore swapchainSemaphore{};
        vk::Semaphore renderingSemaphore{};
        vk::Fence renderingFence{};

        VulkanSpriteDrawData spriteDrawData{};
    };

    struct VulkanSpriteVertex {
        Vector2f position{};
        Vector2f texturePosition{};
    };

    struct VulkanSpriteDrawCommand {
        u32 textureId{};
        f32 height{};
        f32 width{};
        f32 x{};
        f32 y{};
    };

    inline constexpr auto ConcurrentFrameCount = 2;
    inline constexpr auto MaximumSpriteCount = 1000ull;

    class VulkanRenderer final : public VulkanRendererBase {
    public:
        u32 createTexture(const Image& image);

        void initialize(const Window& window) override;
        void resize(u32 width, u32 height) override;
        void draw(u32 x, u32 y, u32 width, u32 height, u32 textureId);
        void display();
    private:
        void _generateDrawCommands();
        void _copyBufferData();
        void _beginDrawing();
        void _endDrawing();

        vk::PipelineLayout _spriteBatchPipelineLayout{};
        vk::Pipeline _spriteBatchPipeline{};

        bool _isGlobalUniformBufferUpdated{};
        VulkanSpriteBatchGlobalUniformBuffer _globalUniformBuffer{};
        u32 _globalUniformBufferId{};

        u32 _globalUniformBufferDescriptorSetLayoutId{};
        u32 _spriteTextureDescriptorSetLayoutId{};

        std::vector<VulkanSpriteDrawCommand> _spriteDrawCommands{};
        std::array<VulkanFrameData, ConcurrentFrameCount> _frameData{};

        std::vector<VulkanSpriteVertex> _vertices{};
        std::vector<u32> _indices{};

        u32 _frameIndex{};
    };
}
