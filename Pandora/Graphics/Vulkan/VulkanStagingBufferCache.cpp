#include "VulkanStagingBufferCache.h"
#include "VulkanGraphicsDevice.h"

#include <ranges>

namespace Pandora::Implementation {

    void VulkanStagingBufferCache::clearFrameStagingBufferLocks(usize frameIndex) {
        const auto isMatchingFrame = [=](const auto& entry) { return entry.frameIndex == frameIndex; };
        for (auto& entry : std::views::filter(_buffers, isMatchingFrame)) {
            entry.isLocked = false;
        }
    }

    usize VulkanStagingBufferCache::getStagingBuffer(VulkanGraphicsDevice& device, usize requiredCapacity, usize frameIndex) {
        auto isMatchingBuffer = [requiredCapacity](const auto& entry) { return entry.capacity >= requiredCapacity && !entry.isLocked; };

        auto bufferPosition = std::ranges::find_if(_buffers, isMatchingBuffer);
        if (bufferPosition != _buffers.end()) {
            bufferPosition->isLocked = true;
            return bufferPosition->bufferId;
        }

        const auto bufferCreateInfo = BufferCreateInfo{ BufferType::Staging, requiredCapacity };
        const auto bufferId = device.createBuffer(bufferCreateInfo);

        _buffers.emplace_back(bufferId, requiredCapacity, frameIndex, true);

        return bufferId;
    }
}
