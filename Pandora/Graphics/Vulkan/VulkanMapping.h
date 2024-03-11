#pragma once

#include "Pandora/Graphics/GraphicsDevice.h"

#include <vulkan/vulkan.hpp>

namespace Pandora::Implementation {
    
    inline vk::BufferUsageFlags mapBufferTypeToVulkanFlags(BufferType type) {
        using enum vk::BufferUsageFlagBits;
        using enum BufferType;

        switch (type) {
        case Vertex:
            return eVertexBuffer | eTransferDst;
        case Uniform:
            return eUniformBuffer | eTransferDst;
        case Index:
            return eIndexBuffer | eTransferDst;
        case Staging:
            return eTransferSrc;
        default:
            throw std::runtime_error("Unreachable");
        }
    }

    inline vk::Format mapTextureFormatToVulkanFormat(TextureFormatType type) {
        using enum TextureFormatType;
        using enum vk::Format;

        switch (type) {
        case RGBA8UNorm:
            return eR8G8B8A8Unorm;
        case RGBA16SFloat:
            return eR16G16B16A16Sfloat;
        default:
            throw std::runtime_error("Unreachable");
        }
    }

    inline vk::ImageUsageFlags mapTextureUsageToVulkanFlags(TextureUsageType usage) {
        using enum vk::ImageUsageFlagBits;
        using enum TextureUsageType;

        switch (usage) {
        case RenderTarget:
            return eTransferDst | eTransferSrc | eStorage | eColorAttachment;
        case Sampled:
            return eTransferDst | eSampled;
        default:
            throw std::runtime_error("Unreachable");
        }
    }

    inline u32 mapVertexLayoutElementToSize(VertexElementType elementType) {
        using enum VertexElementType;

        switch (elementType) {
        case Float2:
            return sizeof(float) * 2;
        default:
            throw std::runtime_error("Unreachable");
        }
    }

    inline vk::Format mapVertexLayoutElementFormatToVulkanFormat(VertexElementType elementType) {
        using enum VertexElementType;
        using enum vk::Format;

        switch (elementType) {
        case Float2:
            return eR32G32Sfloat;
        default:
            throw std::runtime_error("Unreachable");
        }
    }

    inline vk::DescriptorType mapBindingElementTypeToDescriptorType(BindGroupElementType type) {
        using enum BindGroupElementType;
        using enum vk::DescriptorType;

        switch (type) {
        case SamplerTexture:
            return eCombinedImageSampler;
        case UniformBuffer:
            return eUniformBuffer;
        default:
            throw std::runtime_error("Unreachable");
        }
    }

    inline vk::ShaderStageFlagBits mapBindingLocationTypeToShaderStage(BindGroupLocationType type) {
        using enum vk::ShaderStageFlagBits;
        using enum BindGroupLocationType;

        switch (type) {
        case Fragment:
            return eFragment;
        case Vertex:
            return eVertex;
        default:
            throw std::runtime_error("Unreachable");
        }
    }
}
