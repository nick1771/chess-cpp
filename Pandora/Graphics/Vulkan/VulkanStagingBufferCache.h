#pragma once

#include "Pandora/Pandora.h"

#include <vulkan/vulkan.hpp>
#include <vector>

namespace Pandora::Implementation {

    struct VulkanStagingBufferCacheEntry {
        u32 bufferId{};
        usize capacity{};
        usize frameIndex{};
        bool isLocked{};
    };

    class VulkanGraphicsDevice;

    class VulkanStagingBufferCache {
    public:
        usize getStagingBuffer(VulkanGraphicsDevice& device, usize requiredCapacity, usize frameIndex);
        void clearFrameStagingBufferLocks(usize frameIndex);
    private:
        std::vector<VulkanStagingBufferCacheEntry> _buffers{};
    };
}
