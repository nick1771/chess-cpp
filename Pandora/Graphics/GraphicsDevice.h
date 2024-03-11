#pragma once

#include "Constants.h"

#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Collections/ArrayVector.h"
#include "Pandora/Pandora.h"

#include <memory>
#include <span>

namespace Pandora::Implementation {

    class VulkanGraphicsDevice;
}

namespace Pandora {

    enum class BufferType {
        Staging,
        Uniform,
        Vertex,
        Index,
    };

    enum class TextureFormatType {
        RGBA8UNorm,
        RGBA16SFloat
    };

    enum class VertexElementType {
        Float2
    };

    enum class BindGroupElementType {
        SamplerTexture,
        UniformBuffer,
        None,
    };

    enum class BindGroupLocationType {
        Vertex,
        Fragment,
        None
    };

    enum class PresentationResultType {
        SurfaceOutOfDate,
        Success,
    };

    enum class TextureUsageType {
        RenderTarget,
        Sampled
    };

    struct BindGroup {
        BindGroupElementType type0{};
        BindGroupLocationType location0{};

        auto operator<=>(const BindGroup&) const = default;
    };

    struct PipelineCreateInfo {
        using VertexLayout = ArrayVector<VertexElementType, Constants::MaximumVertexElementCount>;
        using BindGroups = ArrayVector<u32, Constants::MaximumBindGroupCount>;

        std::span<u8> vertexShaderByteCode{};
        std::span<u8> fragmentShaderByteCode{};
        BindGroups bindGroupLayout{};
        VertexLayout vertexLayout{};
    };

    struct BufferCreateInfo {
        BufferType type{};
        usize size{};
    };

    struct TextureCreateInfo {
        TextureFormatType format{};
        TextureUsageType usage{};
        u32 width{};
        u32 height{};
    };

    class Window;

    class GraphicsDevice {
    public:
        GraphicsDevice();
        ~GraphicsDevice();

        void configure(const Window& window);

        usize createTexture(const TextureCreateInfo& createInfo);
        usize createPipeline(const PipelineCreateInfo& createInfo);
        usize createBuffer(const BufferCreateInfo& createInfo);
        usize createBindGroup(const BindGroup& group);

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
        std::unique_ptr<Implementation::VulkanGraphicsDevice> _implementation;
    };
}
