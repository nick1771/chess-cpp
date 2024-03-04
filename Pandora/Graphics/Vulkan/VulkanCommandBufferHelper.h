#pragma once

#include <vulkan/vulkan.hpp>

namespace Pandora::Implementation {

    struct VulkanCommandBufferHelper {
        vk::CommandBuffer commandBuffer;

        void transitionImage(vk::Image image, vk::ImageLayout current, vk::ImageLayout target) const;
        void copyImageToImage(vk::Image source, vk::Image destination, vk::Extent2D sourceSize, vk::Extent2D destinationSize) const;
    };
}
