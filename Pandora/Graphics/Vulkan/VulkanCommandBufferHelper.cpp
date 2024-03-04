#include "VulkanCommandBufferHelper.h"

namespace  Pandora::Implementation {

    void VulkanCommandBufferHelper::transitionImage(vk::Image image, vk::ImageLayout current, vk::ImageLayout target) const
    {
        auto subresourceRange = vk::ImageSubresourceRange{};
        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        subresourceRange.levelCount = vk::RemainingMipLevels;
        subresourceRange.layerCount = vk::RemainingArrayLayers;

        auto imageBarrier = vk::ImageMemoryBarrier2{};
        imageBarrier.oldLayout = current;
        imageBarrier.newLayout = target;
        imageBarrier.subresourceRange = subresourceRange;
        imageBarrier.image = image;

        if (current == vk::ImageLayout::eUndefined && target == vk::ImageLayout::eTransferDstOptimal) {
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eNone;
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
        } else if (current == vk::ImageLayout::eTransferDstOptimal && target == vk::ImageLayout::eReadOnlyOptimal) {
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        } else if (current == vk::ImageLayout::eUndefined && target == vk::ImageLayout::eColorAttachmentOptimal) {
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eNone;
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        } else if (current == vk::ImageLayout::eColorAttachmentOptimal && target == vk::ImageLayout::eTransferSrcOptimal) {
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        } else if (current == vk::ImageLayout::eTransferDstOptimal && target == vk::ImageLayout::ePresentSrcKHR) {
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eNone;
        } else {
            imageBarrier.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
            imageBarrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
            imageBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead;
        }

        auto dependencyInfo = vk::DependencyInfo{};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageBarrier;

        commandBuffer.pipelineBarrier2(dependencyInfo);
    }

    void VulkanCommandBufferHelper::copyImageToImage(vk::Image source, vk::Image destination, vk::Extent2D sourceSize, vk::Extent2D destinationSize) const {
        auto blitRegion = vk::ImageBlit2{};
        blitRegion.srcOffsets[1].x = sourceSize.width;
        blitRegion.srcOffsets[1].y = sourceSize.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = destinationSize.width;
        blitRegion.dstOffsets[1].y = destinationSize.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        auto blitImageInfo = vk::BlitImageInfo2{};
        blitImageInfo.dstImage = destination;
        blitImageInfo.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
        blitImageInfo.srcImage = source;
        blitImageInfo.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
        blitImageInfo.filter = vk::Filter::eLinear;
        blitImageInfo.regionCount = 1;
        blitImageInfo.pRegions = &blitRegion;

        commandBuffer.blitImage2(blitImageInfo);
    }
}
