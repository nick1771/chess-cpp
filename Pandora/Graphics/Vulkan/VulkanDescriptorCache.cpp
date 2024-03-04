#include "VulkanDescriptorCache.h"

#include <ranges>

namespace Pandora::Implementation {

    static vk::DescriptorType mapBindingElementTypeToDescriptorType(VulkanDescriptorBindingElementType type) {
        switch (type) {
        case VulkanDescriptorBindingElementType::TextureSampler:
            return vk::DescriptorType::eCombinedImageSampler;
        case VulkanDescriptorBindingElementType::Uniform:
            return vk::DescriptorType::eUniformBuffer;
        default:
            throw std::runtime_error("Unreachable");
        }
    }

    static vk::ShaderStageFlagBits mapBindingLocationTypeToShaderStage(VulkanDescriptorBindingLocationType type) {
        switch (type) {
        case VulkanDescriptorBindingLocationType::Fragment:
            return vk::ShaderStageFlagBits::eFragment;
        case VulkanDescriptorBindingLocationType::Vertex:
            return vk::ShaderStageFlagBits::eVertex;
        default:
            throw std::runtime_error("Unreachable");
        }
    }

    void VulkanDescriptorCache::initialize(vk::Device device) {
        _device = device;

        for (const auto [index, descriptorPool] : std::views::enumerate(_descriptorPools)) {
            const auto bindingType = static_cast<VulkanDescriptorBindingElementType>(index);
            const auto descriptorType = mapBindingElementTypeToDescriptorType(bindingType);

            auto descriptorPoolSize = vk::DescriptorPoolSize{};
            descriptorPoolSize.descriptorCount = MaximumDescriptorCountPerElement;
            descriptorPoolSize.type = descriptorType;

            auto descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo{};
            descriptorPoolCreateInfo.poolSizeCount = 1;
            descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
            descriptorPoolCreateInfo.maxSets = MaximumDescriptorCountPerElement;

            descriptorPool = device.createDescriptorPool(descriptorPoolCreateInfo);
        }
    }

    u32 VulkanDescriptorCache::getOrAllocateDescriptorSet(VulkanDescriptorSetIdentifier identifier) {
        const auto isDescriptorSet = [&identifier](const auto& descriptorSet) { return descriptorSet.identifier == identifier; };
        const auto descriptorSetPosition = std::find_if(_descriptorSets.begin(), _descriptorSets.end(), isDescriptorSet);

        if (descriptorSetPosition != _descriptorSets.end()) {
            return std::distance(_descriptorSets.begin(), descriptorSetPosition);
        }

        const auto& descriptorSetLayout = _descriptorSetLayouts[identifier.layoutId];
        
        auto descriptorSetAllocateInfo = vk::DescriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.descriptorPool = _descriptorPools[static_cast<usize>(descriptorSetLayout.identifier.bindingType0)];
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout.handle;

        identifier.bindingResourceId0 = MaximumId;

        const auto descriptorSet = _device.allocateDescriptorSets(descriptorSetAllocateInfo);
        _descriptorSets.emplace_back(descriptorSet.front(), identifier);
        
        return _descriptorSets.size() - 1;
    }

    u32 VulkanDescriptorCache::getOrCreateDescriptorSetLayout(VulkanDescriptorSetLayoutIdentifier identifier) {
        const auto isLayout = [&identifier](const auto& layout) { return layout.identifier == identifier; };
        const auto identifierPosition = std::find_if(_descriptorSetLayouts.begin(), _descriptorSetLayouts.end(), isLayout);

        if (identifierPosition != _descriptorSetLayouts.end()) {
            return std::distance(_descriptorSetLayouts.begin(), identifierPosition);
        }

        auto descriptorSetLayoutBinding = vk::DescriptorSetLayoutBinding{};
        descriptorSetLayoutBinding.descriptorType = mapBindingElementTypeToDescriptorType(identifier.bindingType0);
        descriptorSetLayoutBinding.stageFlags = mapBindingLocationTypeToShaderStage(identifier.bindingLocationType0);
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.binding = 0;

        auto descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.bindingCount = 1;
        descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

        const auto descriptorSetLayout = _device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo);
        _descriptorSetLayouts.emplace_back(descriptorSetLayout, identifier);

        return _descriptorSetLayouts.size() - 1;
    }

    void VulkanDescriptorCache::updateUniformDescriptorSetBinding(u32 bindingIndex, u32 descriptorSetId, u32 resourceId, const VulkanResourceAllocator& resourceAllocator) {
        const auto& bufferResource = resourceAllocator.getResource<VulkanBufferResource>(resourceId);
        auto& descriptorSet = _descriptorSets[descriptorSetId];

        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = bufferResource.handle;
        bufferInfo.range = vk::WholeSize;

        auto bufferWriteDescriptorSet = vk::WriteDescriptorSet{};
        bufferWriteDescriptorSet.dstSet = descriptorSet.handle;
        bufferWriteDescriptorSet.dstBinding = bindingIndex;
        bufferWriteDescriptorSet.dstArrayElement = 0;
        bufferWriteDescriptorSet.descriptorType = vk::DescriptorType::eUniformBuffer;
        bufferWriteDescriptorSet.descriptorCount = 1;
        bufferWriteDescriptorSet.pBufferInfo = &bufferInfo;

        descriptorSet.identifier.bindingResourceId0 = resourceId;

        _device.updateDescriptorSets(bufferWriteDescriptorSet, nullptr);
    }

    void VulkanDescriptorCache::updateTextureDescriptorSetBinding(u32 bindingIndex, u32 descriptorSetId, u32 resourceId, const VulkanResourceAllocator& resourceAllocator) {
        const auto& textureResource = resourceAllocator.getResource<VulkanTextureResource>(resourceId);
        auto& descriptorSet = _descriptorSets[descriptorSetId];

        auto imageInfo = vk::DescriptorImageInfo{};
        imageInfo.imageView = textureResource.view;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.sampler = textureResource.sampler;

        auto imageWriteDescriptorSet = vk::WriteDescriptorSet{};
        imageWriteDescriptorSet.dstSet = descriptorSet.handle;
        imageWriteDescriptorSet.dstBinding = bindingIndex;
        imageWriteDescriptorSet.dstArrayElement = 0;
        imageWriteDescriptorSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        imageWriteDescriptorSet.descriptorCount = 1;
        imageWriteDescriptorSet.pImageInfo = &imageInfo;

        descriptorSet.identifier.bindingResourceId0 = resourceId;

        _device.updateDescriptorSets(imageWriteDescriptorSet, nullptr);
    }

    void VulkanDescriptorCache::lockDescriptorSet(u32 descriptorSetId) {
        _descriptorSets[descriptorSetId].identifier.isLocked = true;
    }

    void VulkanDescriptorCache::clearFrameDescriptorSetLocks(u32 frameId) {
        for (auto& descriptorSet : _descriptorSets) {
            if (descriptorSet.identifier.frameId == frameId) {
                descriptorSet.identifier.isLocked = false;
            }
        }
    }

    bool VulkanDescriptorCache::isDescriptorSetBindingWritenTo(u32 bindingIndex, u32 descriptorSetId) {
        return _descriptorSets[descriptorSetId].identifier.bindingResourceId0 != MaximumId;
    }

    vk::DescriptorSetLayout VulkanDescriptorCache::getDescriptorSetLayoutHandle(u32 descriptorSetLayoutId) const {
        return _descriptorSetLayouts[descriptorSetLayoutId].handle;
    }
    
    vk::DescriptorSet VulkanDescriptorCache::getDescriptorSetHandle(u32 descriptorSetId) const {
        return _descriptorSets[descriptorSetId].handle;
    }
}
