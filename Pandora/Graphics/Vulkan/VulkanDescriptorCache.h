#pragma once

#include "Pandora/Pandora.h"
#include "VulkanResourceAllocator.h"

#include <limits>
#include <vector>
#include <array>

#include <vulkan/vulkan.hpp>
 
namespace Pandora::Implementation {

    inline constexpr auto MaximumId = std::numeric_limits<u32>::max();
    inline constexpr auto MaximumDescriptorCountPerElement = 512;
    inline constexpr auto MaximumDescriptorSetBindings = 1;

    enum class VulkanDescriptorBindingElementType {
        TextureSampler,
        Uniform,
        None
    };

    enum class VulkanDescriptorBindingLocationType {
        Fragment,
        Vertex,
        None
    };

    struct VulkanDescriptorSetIdentifier {
        u32 bindingResourceId0 = MaximumId;

        u32 layoutId{};
        u32 frameId{};

        bool isLocked{};

        inline bool operator==(const VulkanDescriptorSetIdentifier& left) const {
            return !isLocked && frameId == left.frameId && layoutId == left.layoutId
                && (bindingResourceId0 == MaximumId || bindingResourceId0 == left.bindingResourceId0);
        }
    };

    struct VulkanDescriptorSet {
        vk::DescriptorSet handle{};
        VulkanDescriptorSetIdentifier identifier{};
    };

    struct VulkanDescriptorSetLayoutIdentifier {
        VulkanDescriptorBindingElementType bindingType0 = VulkanDescriptorBindingElementType::None;
        VulkanDescriptorBindingLocationType bindingLocationType0 = VulkanDescriptorBindingLocationType::None;

        inline bool operator==(const VulkanDescriptorSetLayoutIdentifier& left) const {
            return bindingType0 == left.bindingType0 && bindingLocationType0 == left.bindingLocationType0;
        }
    };

    struct VulkanDescriptorSetLayout {
        vk::DescriptorSetLayout handle{};
        VulkanDescriptorSetLayoutIdentifier identifier{};
    };

    class VulkanDescriptorCache {
    public:
        void initialize(vk::Device device);

        u32 getOrAllocateDescriptorSet(VulkanDescriptorSetIdentifier identifier);
        u32 getOrCreateDescriptorSetLayout(VulkanDescriptorSetLayoutIdentifier identifier);

        void updateUniformDescriptorSetBinding(u32 bindingIndex, u32 descriptorSetId, u32 resourceId, const VulkanResourceAllocator& resourceAllocator);
        void updateTextureDescriptorSetBinding(u32 bindingIndex, u32 descriptorSetId, u32 resourceId, const VulkanResourceAllocator& resourceAllocator);
        
        void lockDescriptorSet(u32 descriptorSetId);
        void clearFrameDescriptorSetLocks(u32 frameId);

        bool isDescriptorSetBindingWritenTo(u32 bindingIndex, u32 descriptorSetId);

        vk::DescriptorSetLayout getDescriptorSetLayoutHandle(u32 descriptorSetLayoutId) const;
        vk::DescriptorSet getDescriptorSetHandle(u32 descriptorSetId) const;
    private:
        vk::Device _device{};

        std::array<vk::DescriptorPool, 2> _descriptorPools{};

        std::vector<VulkanDescriptorSet> _descriptorSets{};
        std::vector<VulkanDescriptorSetLayout> _descriptorSetLayouts{};
    };
}
