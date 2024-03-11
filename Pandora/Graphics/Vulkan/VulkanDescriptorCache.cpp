#include "VulkanDescriptorCache.h"
#include "VulkanMapping.h"

#include <ranges>

namespace Pandora::Implementation {

    void VulkanDescriptorCache::initialize(vk::Device device) {
        _device = device;

        auto samplerDescriptorPoolSize = vk::DescriptorPoolSize{};
        samplerDescriptorPoolSize.descriptorCount = Constants::MaximumTextureCount;
        samplerDescriptorPoolSize.type = vk::DescriptorType::eCombinedImageSampler;

        auto uniformBufferDescriptorPoolSize = vk::DescriptorPoolSize{};
        uniformBufferDescriptorPoolSize.descriptorCount = Constants::MaximumUniformBufferCount;
        uniformBufferDescriptorPoolSize.type = vk::DescriptorType::eUniformBuffer;

        auto poolSizes = { 
            samplerDescriptorPoolSize, 
            uniformBufferDescriptorPoolSize 
        };

        auto descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
        descriptorPoolCreateInfo.pPoolSizes = poolSizes.begin();
        descriptorPoolCreateInfo.maxSets = Constants::MaximumTextureCount + Constants::MaximumUniformBufferCount;

        _descriptorPool = device.createDescriptorPool(descriptorPoolCreateInfo);
    }

    void VulkanDescriptorCache::terminate() {
        _device.destroyDescriptorPool(_descriptorPool);
        for (const auto& layout : _descriptorSetLayouts) {
            _device.destroyDescriptorSetLayout(layout.handle);
        }
    }

    usize VulkanDescriptorCache::getOrAllocateDescriptorSet(VulkanDescriptorSetIdentifier identifier) {
        const auto isDescriptorSet = [&identifier](const auto& descriptorSet) { return descriptorSet.identifier == identifier; };
        const auto descriptorSetPosition = std::find_if(_descriptorSets.begin(), _descriptorSets.end(), isDescriptorSet);

        if (descriptorSetPosition != _descriptorSets.end()) {
            return std::distance(_descriptorSets.begin(), descriptorSetPosition);
        }

        const auto& descriptorSetLayout = _descriptorSetLayouts[identifier.layoutId];
        
        auto descriptorSetAllocateInfo = vk::DescriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.descriptorPool = _descriptorPool;
        descriptorSetAllocateInfo.descriptorSetCount = 1;
        descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout.handle;

        identifier.bindingResourceId0 = Constants::MaximumIdValue;
        identifier.bindResourceType0 = identifier.bindResourceType0;

        const auto descriptorSet = _device.allocateDescriptorSets(descriptorSetAllocateInfo);
        _descriptorSets.emplace_back(descriptorSet.front(), identifier);
        
        return _descriptorSets.size() - 1;
    }

    usize VulkanDescriptorCache::getOrCreateDescriptorSetLayout(BindGroup bindGroup) {
        const auto isLayout = [&bindGroup](const auto& layout) { return layout.bindGroup == bindGroup; };
        const auto identifierPosition = std::ranges::find_if(_descriptorSetLayouts, isLayout);

        if (identifierPosition != _descriptorSetLayouts.end()) {
            return std::distance(_descriptorSetLayouts.begin(), identifierPosition);
        }

        auto descriptorSetLayoutBinding = vk::DescriptorSetLayoutBinding{};
        descriptorSetLayoutBinding.descriptorType = mapBindingElementTypeToDescriptorType(bindGroup.type0);
        descriptorSetLayoutBinding.stageFlags = mapBindingLocationTypeToShaderStage(bindGroup.location0);
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.binding = 0;

        auto descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.bindingCount = 1;
        descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

        const auto descriptorSetLayout = _device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo);
        _descriptorSetLayouts.emplace_back(descriptorSetLayout, bindGroup);

        return _descriptorSetLayouts.size() - 1;
    }

    BindGroupElementType VulkanDescriptorCache::getBindGroupBindingType(usize descriptorLayoutId, u32 bindingIndex) {
        const auto& descriptorSetLayout = _descriptorSetLayouts[descriptorLayoutId];
        return descriptorSetLayout.bindGroup.type0;
    }

    void VulkanDescriptorCache::updateUniformDescriptorSetBinding(const UniformUpdateInfo& uniformUpdateInfo) {
        auto& descriptorSet = _descriptorSets[uniformUpdateInfo.descriptorSetId];

        auto bufferInfo = vk::DescriptorBufferInfo{};
        bufferInfo.buffer = uniformUpdateInfo.buffer;
        bufferInfo.range = vk::WholeSize;

        auto bufferWriteDescriptorSet = vk::WriteDescriptorSet{};
        bufferWriteDescriptorSet.dstSet = descriptorSet.handle;
        bufferWriteDescriptorSet.dstBinding = uniformUpdateInfo.bindingIndex;
        bufferWriteDescriptorSet.dstArrayElement = 0;
        bufferWriteDescriptorSet.descriptorType = vk::DescriptorType::eUniformBuffer;
        bufferWriteDescriptorSet.descriptorCount = 1;
        bufferWriteDescriptorSet.pBufferInfo = &bufferInfo;

        descriptorSet.identifier.bindingResourceId0 = uniformUpdateInfo.resourceId;

        _device.updateDescriptorSets(bufferWriteDescriptorSet, nullptr);
    }

    void VulkanDescriptorCache::updateTextureDescriptorSetBinding(const TextureUpdateInfo& textureUpdateInfo) {
        auto& descriptorSet = _descriptorSets[textureUpdateInfo.descriptorSetId];

        auto imageInfo = vk::DescriptorImageInfo{};
        imageInfo.imageView = textureUpdateInfo.image;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.sampler = textureUpdateInfo.sampler;

        auto imageWriteDescriptorSet = vk::WriteDescriptorSet{};
        imageWriteDescriptorSet.dstSet = descriptorSet.handle;
        imageWriteDescriptorSet.dstBinding = textureUpdateInfo.bindingIndex;
        imageWriteDescriptorSet.dstArrayElement = 0;
        imageWriteDescriptorSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        imageWriteDescriptorSet.descriptorCount = 1;
        imageWriteDescriptorSet.pImageInfo = &imageInfo;

        descriptorSet.identifier.bindingResourceId0 = textureUpdateInfo.resourceId;

        _device.updateDescriptorSets(imageWriteDescriptorSet, nullptr);
    }

    void VulkanDescriptorCache::lockDescriptorSet(usize descriptorSetId) {
        _descriptorSets[descriptorSetId].identifier.isLocked = true;
    }

    void VulkanDescriptorCache::clearFrameDescriptorSetLocks(usize frameId) {
        for (auto& descriptorSet : _descriptorSets) {
            if (descriptorSet.identifier.frameId == frameId) {
                descriptorSet.identifier.isLocked = false;
            }
        }
    }

    bool VulkanDescriptorCache::isDescriptorSetBindingWritenTo(u32 bindingIndex, usize descriptorSetId) {
        return _descriptorSets[descriptorSetId].identifier.bindingResourceId0 != Constants::MaximumIdValue;
    }

    vk::DescriptorSetLayout VulkanDescriptorCache::getDescriptorSetLayoutHandle(usize descriptorSetLayoutId) const {
        return _descriptorSetLayouts[descriptorSetLayoutId].handle;
    }

    vk::DescriptorSet VulkanDescriptorCache::getDescriptorSetHandle(usize descriptorSetId) const {
        return _descriptorSets[descriptorSetId].handle;
    }
}
