#pragma once

#include "Pandora/Pandora.h"

#include <vulkan/vulkan.hpp>
#include <vector>

namespace Pandora::Implementation {

    enum class VulkanVertexElementType {
        Float3,
        Float2
    };

    struct VulkanPipelineCreateInfo {
        vk::Device device{};

        const void* vertexShaderByteCode{};
        usize vertexShaderByteCodeSize{};

        const void* fragmentShaderByteCode{};
        usize fragmentShaderByteCodeSize{};

        std::vector<VulkanVertexElementType> vertexLayout{};
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{};

        vk::Format colorAttachmentFormat{};
        bool isAlphaBlendEnabled{};
    };

    struct VulkanPipeline {
        vk::PipelineLayout layout{};
        vk::Pipeline pipeline{};
    };

    VulkanPipeline createPipeline(const VulkanPipelineCreateInfo createInfo);
}
