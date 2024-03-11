#pragma once

#include "Pandora/Graphics/Constants.h"
#include "Pandora/Graphics/GraphicsDevice.h"
#include "Pandora/Pandora.h"

#include <limits>
#include <vector>
#include <array>

#include <vulkan/vulkan.hpp>

namespace Pandora::Implementation {

    struct VulkanDescriptorSetIdentifier {
        usize bindingResourceId0 = Constants::MaximumIdValue;
        BindGroupElementType bindResourceType0{};

        usize layoutId{};
        usize frameId{};

        bool isLocked{};

        inline bool operator==(const VulkanDescriptorSetIdentifier& left) const {
            return !isLocked && frameId == left.frameId 
                && layoutId == left.layoutId 
                && bindResourceType0 == left.bindResourceType0
                && (bindingResourceId0 == Constants::MaximumIdValue || bindingResourceId0 == left.bindingResourceId0);
        }
    };

    struct VulkanDescriptorSet {
        vk::DescriptorSet handle{};
        VulkanDescriptorSetIdentifier identifier{};
    };

    struct VulkanDescriptorSetLayout {
        vk::DescriptorSetLayout handle{};
        BindGroup bindGroup{};
    };

    struct UniformUpdateInfo {
        u32 bindingIndex;
        usize descriptorSetId;
        usize resourceId{};
        vk::Buffer buffer{};
    };

    struct TextureUpdateInfo {
        u32 bindingIndex;
        usize descriptorSetId;
        usize resourceId{};
        vk::ImageView image{};
        vk::Sampler sampler{};
    };

    class VulkanDescriptorCache {
    public:
        void initialize(vk::Device device);
        void terminate();

        usize getOrAllocateDescriptorSet(VulkanDescriptorSetIdentifier identifier);
        usize getOrCreateDescriptorSetLayout(BindGroup bindGroup);
        BindGroupElementType getBindGroupBindingType(usize descriptorLayoutId, u32 bindingIndex);

        void updateUniformDescriptorSetBinding(const UniformUpdateInfo& uniformUpdateInfo);
        void updateTextureDescriptorSetBinding(const TextureUpdateInfo& textureUpdateInfo);
        
        void lockDescriptorSet(usize descriptorSetId);
        void clearFrameDescriptorSetLocks(usize frameId);

        bool isDescriptorSetBindingWritenTo(u32 bindingIndex, usize descriptorSetId);

        vk::DescriptorSetLayout getDescriptorSetLayoutHandle(usize descriptorSetLayoutId) const;
        vk::DescriptorSet getDescriptorSetHandle(usize descriptorSetId) const;
    private:
        vk::Device _device{};
        vk::DescriptorPool _descriptorPool{};

        std::vector<VulkanDescriptorSet> _descriptorSets{};
        std::vector<VulkanDescriptorSetLayout> _descriptorSetLayouts{};
    };
}
