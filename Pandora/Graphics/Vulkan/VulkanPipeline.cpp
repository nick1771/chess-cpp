#include "VulkanPipeline.h"

namespace Pandora::Implementation {

    static u32 getVertexLayoutElementSize(VulkanVertexElementType elementType) {
        switch (elementType) {
        case VulkanVertexElementType::Float2:
            return sizeof(float) * 2;
        case VulkanVertexElementType::Float3:
            return sizeof(float) * 3;
        }
    }

    static vk::Format getVertexLayoutElementFormat(VulkanVertexElementType elementType) {
        switch (elementType) {
        case VulkanVertexElementType::Float2:
            return vk::Format::eR32G32Sfloat;
        case VulkanVertexElementType::Float3:
            return vk::Format::eR32G32B32Sfloat;
        }
    }

    struct VertexLayoutInfo {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};
        vk::VertexInputBindingDescription bindingDescription{};
    };

    static VertexLayoutInfo getVertexLayoutInfo(const VulkanPipelineCreateInfo createInfo) {
        auto result = VertexLayoutInfo{};

        auto currentVertexOffset = 0u;
        for (usize index = 0; index < createInfo.vertexLayout.size(); index++) {
            const auto vertexLayoutElement = createInfo.vertexLayout[index];

            auto& attributeDescription = result.attributeDescriptions[index];
            attributeDescription.format = getVertexLayoutElementFormat(vertexLayoutElement);
            attributeDescription.offset = currentVertexOffset;
            attributeDescription.location = index;
            attributeDescription.binding = 0u;

            currentVertexOffset += getVertexLayoutElementSize(vertexLayoutElement);
        }
        
        result.bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        result.bindingDescription.stride = currentVertexOffset;
        result.bindingDescription.binding = 0u;

        return result;
    }

    VulkanPipeline createPipeline(const VulkanPipelineCreateInfo createInfo) {
        auto device = createInfo.device;

        auto vertexShaderModuleCreateInfo = vk::ShaderModuleCreateInfo{};
        vertexShaderModuleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.vertexShaderByteCode);
        vertexShaderModuleCreateInfo.codeSize = createInfo.vertexShaderByteCodeSize;

        auto fragmentShaderModuleCreateInfo = vk::ShaderModuleCreateInfo{};
        fragmentShaderModuleCreateInfo.pCode = reinterpret_cast<const u32*>(createInfo.fragmentShaderByteCode);
        fragmentShaderModuleCreateInfo.codeSize = createInfo.fragmentShaderByteCodeSize;

        const auto vertexShaderModule = device.createShaderModule(vertexShaderModuleCreateInfo);
        const auto fragmentShaderModule = device.createShaderModule(fragmentShaderModuleCreateInfo);

        auto vertexShaderStageCreateInfo = vk::PipelineShaderStageCreateInfo{};
        vertexShaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertexShaderStageCreateInfo.module = vertexShaderModule;
        vertexShaderStageCreateInfo.pName = "main";

        auto fragmentShaderStageCreateInfo = vk::PipelineShaderStageCreateInfo{};
        fragmentShaderStageCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
        fragmentShaderStageCreateInfo.module = fragmentShaderModule;
        fragmentShaderStageCreateInfo.pName = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStageCreateInfos{};
        shaderStageCreateInfos[0] = vertexShaderStageCreateInfo;
        shaderStageCreateInfos[1] = fragmentShaderStageCreateInfo;

        const auto vertexLayoutInfo = getVertexLayoutInfo(createInfo);

        vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
        vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexLayoutInfo.bindingDescription;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexLayoutInfo.attributeDescriptions.data();
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = vertexLayoutInfo.attributeDescriptions.size();

        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
        inputAssemblyCreateInfo.primitiveRestartEnable = false;
        inputAssemblyCreateInfo.topology = vk::PrimitiveTopology::eTriangleList;

        vk::PipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.pSetLayouts = createInfo.descriptorSetLayouts.data();
        layoutCreateInfo.setLayoutCount = createInfo.descriptorSetLayouts.size();

        vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
        rasterizerCreateInfo.lineWidth = 1.f;
        rasterizerCreateInfo.polygonMode = vk::PolygonMode::eFill;
        rasterizerCreateInfo.cullMode = vk::CullModeFlagBits::eBack;
        rasterizerCreateInfo.frontFace = vk::FrontFace::eClockwise;

        vk::PipelineMultisampleStateCreateInfo multisampleCreateInfo{};
        multisampleCreateInfo.sampleShadingEnable = false;
        multisampleCreateInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
        multisampleCreateInfo.minSampleShading = 1.0f;
        multisampleCreateInfo.pSampleMask = nullptr;
        multisampleCreateInfo.alphaToCoverageEnable = false;
        multisampleCreateInfo.alphaToOneEnable = false;

        vk::PipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
        depthStencilCreateInfo.depthTestEnable = false;
        depthStencilCreateInfo.depthWriteEnable = false;
        depthStencilCreateInfo.depthCompareOp = vk::CompareOp::eNever;
        depthStencilCreateInfo.depthBoundsTestEnable = false;
        depthStencilCreateInfo.stencilTestEnable = false;
        depthStencilCreateInfo.front = vk::StencilOpState{};
        depthStencilCreateInfo.back = vk::StencilOpState{};
        depthStencilCreateInfo.minDepthBounds = 0.f;
        depthStencilCreateInfo.maxDepthBounds = 1.f;

        auto colorWireMask = vk::ColorComponentFlagBits::eR
            | vk::ColorComponentFlagBits::eG
            | vk::ColorComponentFlagBits::eB
            | vk::ColorComponentFlagBits::eA;

        vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{};
        colorBlendAttachmentState.colorWriteMask = colorWireMask;
        colorBlendAttachmentState.blendEnable = true;
        colorBlendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        colorBlendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        colorBlendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
        colorBlendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eZero;
        colorBlendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
        colorBlendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;

        vk::PipelineColorBlendStateCreateInfo colorBlendAttachmentCreateInfo{};
        colorBlendAttachmentCreateInfo.logicOpEnable = false;
        colorBlendAttachmentCreateInfo.logicOp = vk::LogicOp::eCopy;
        colorBlendAttachmentCreateInfo.attachmentCount = 1;
        colorBlendAttachmentCreateInfo.pAttachments = &colorBlendAttachmentState;

        vk::PipelineRenderingCreateInfo renderingCreateInfo{};
        renderingCreateInfo.colorAttachmentCount = 1;
        renderingCreateInfo.pColorAttachmentFormats = &createInfo.colorAttachmentFormat;

        vk::DynamicState dynamicStates[] = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        auto dynamicStateCreateInfo = vk::PipelineDynamicStateCreateInfo{};
        dynamicStateCreateInfo.pDynamicStates = dynamicStates;
        dynamicStateCreateInfo.dynamicStateCount = 2;

        auto viewportStateCreateInfo = vk::PipelineViewportStateCreateInfo{};
        viewportStateCreateInfo.scissorCount = 1;
        viewportStateCreateInfo.viewportCount = 1;

        const auto pipelineLayout = device.createPipelineLayout(layoutCreateInfo);

        auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo{};
        pipelineCreateInfo.pNext = &renderingCreateInfo;
        pipelineCreateInfo.stageCount = shaderStageCreateInfos.size();
        pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
        pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
        pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendAttachmentCreateInfo;
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.layout = pipelineLayout;

        const auto pipelineResult = device.createGraphicsPipeline(nullptr, pipelineCreateInfo);
        if (pipelineResult.result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        device.destroyShaderModule(vertexShaderModule);
        device.destroyShaderModule(fragmentShaderModule);

        return VulkanPipeline{ pipelineLayout, pipelineResult.value};
    }
}
