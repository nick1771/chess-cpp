#include "VulkanRenderer.h"
#include "VulkanCommandBufferHelper.h"

#include "Pandora/Mathematics/MatrixTransform.h"
#include "Pandora/File.h"

namespace Pandora::Implementation {

    u32 VulkanRenderer::createTexture(const Image& image) {
        const auto imageDimensions = image.getSize();
        const auto imageFormat = vk::Format::eR8G8B8A8Unorm;
        const auto imageUsage = VulkanTextureUsageType::Sampled;

        const auto textureId = _resourceAllocator.createTexture(imageDimensions.x, imageDimensions.y, imageFormat, imageUsage);
        const auto textureSize = imageDimensions.x * imageDimensions.y * 4;
        _resourceAllocator.setTextureData(textureId, image.getPixels(), textureSize);

        return textureId;
    }

    void VulkanRenderer::initialize(const Window& window) {
        VulkanRendererBase::initialize(window);

        auto globalUniformDescriptorSetLayoutIdentifier = VulkanDescriptorSetLayoutIdentifier{};
        globalUniformDescriptorSetLayoutIdentifier.bindingLocationType0 = VulkanDescriptorBindingLocationType::Vertex;
        globalUniformDescriptorSetLayoutIdentifier.bindingType0 = VulkanDescriptorBindingElementType::Uniform;

        auto spriteTextureDescriptorSetLayoutIdentifier = VulkanDescriptorSetLayoutIdentifier{};
        spriteTextureDescriptorSetLayoutIdentifier.bindingLocationType0 = VulkanDescriptorBindingLocationType::Fragment;
        spriteTextureDescriptorSetLayoutIdentifier.bindingType0 = VulkanDescriptorBindingElementType::TextureSampler;

        _globalUniformBufferDescriptorSetLayoutId = _descriptorCache.getOrCreateDescriptorSetLayout(globalUniformDescriptorSetLayoutIdentifier);
        _spriteTextureDescriptorSetLayoutId = _descriptorCache.getOrCreateDescriptorSetLayout(spriteTextureDescriptorSetLayoutIdentifier);

        const auto vertexShaderCode = readFileToBytes("C:/Dev/RealProjects/ChessCppCmake/content/vertex_shader2.spv");
        const auto fragmentShaderCode = readFileToBytes("C:/Dev/RealProjects/ChessCppCmake/content/fragment_shader2.spv");

        auto spritePipelineCreateInfo = VulkanPipelineCreateInfo{};
        spritePipelineCreateInfo.device = _logicalDevice;
        spritePipelineCreateInfo.isAlphaBlendEnabled = true;
        spritePipelineCreateInfo.vertexShaderByteCode = vertexShaderCode.data();
        spritePipelineCreateInfo.vertexShaderByteCodeSize = vertexShaderCode.size();
        spritePipelineCreateInfo.fragmentShaderByteCode = fragmentShaderCode.data();
        spritePipelineCreateInfo.fragmentShaderByteCodeSize = fragmentShaderCode.size();
        spritePipelineCreateInfo.descriptorSetLayouts.push_back(_descriptorCache.getDescriptorSetLayoutHandle(_globalUniformBufferDescriptorSetLayoutId));
        spritePipelineCreateInfo.descriptorSetLayouts.push_back(_descriptorCache.getDescriptorSetLayoutHandle(_spriteTextureDescriptorSetLayoutId));
        spritePipelineCreateInfo.vertexLayout.push_back(VulkanVertexElementType::Float2);
        spritePipelineCreateInfo.vertexLayout.push_back(VulkanVertexElementType::Float2);
        spritePipelineCreateInfo.colorAttachmentFormat = vk::Format::eR16G16B16A16Sfloat;

        const auto pipelineResult = createPipeline(spritePipelineCreateInfo);

        _spriteBatchPipeline = pipelineResult.pipeline;
        _spriteBatchPipelineLayout = pipelineResult.layout;

        auto commandPoolCreateInfo = vk::CommandPoolCreateInfo{};
        commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        commandPoolCreateInfo.queueFamilyIndex = _graphicsQueueIndex;

        auto fenceCreateInfo = vk::FenceCreateInfo{};
        fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        const auto maximumIndexBufferSize = MaximumSpriteCount * 6 * sizeof(u32);
        const auto maximumVertexBufferSize = MaximumSpriteCount * 4 * sizeof(VulkanSpriteVertex);

        for (auto& frame : _frameData) {
            frame.commandPool = _logicalDevice.createCommandPool(commandPoolCreateInfo);

            auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo{};
            commandBufferAllocateInfo.commandPool = frame.commandPool;
            commandBufferAllocateInfo.commandBufferCount = 1;
            commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;

            frame.commandBuffer = _logicalDevice.allocateCommandBuffers(commandBufferAllocateInfo).front();
            frame.renderingFence = _logicalDevice.createFence(fenceCreateInfo);
            frame.renderingSemaphore = _logicalDevice.createSemaphore({});
            frame.swapchainSemaphore = _logicalDevice.createSemaphore({});

            frame.spriteDrawData.indexBuffer = _resourceAllocator.createBuffer(maximumIndexBufferSize, VulkanBufferUsageType::Index);
            frame.spriteDrawData.indexStagingBuffer = _resourceAllocator.createBuffer(maximumIndexBufferSize, VulkanBufferUsageType::Staging);
            frame.spriteDrawData.vertexBuffer = _resourceAllocator.createBuffer(maximumIndexBufferSize, VulkanBufferUsageType::Vertex);
            frame.spriteDrawData.vertexStagingBuffer = _resourceAllocator.createBuffer(maximumIndexBufferSize, VulkanBufferUsageType::Staging);
        }

        _globalUniformBuffer.projection = ortho(0, _viewport.width, 0, _viewport.height, -1.f, 1.f);

        _globalUniformBufferId = _resourceAllocator.createBuffer(sizeof(VulkanSpriteBatchGlobalUniformBuffer), VulkanBufferUsageType::Uniform);
        _resourceAllocator.setBufferData(_globalUniformBufferId, &_globalUniformBuffer, sizeof(VulkanSpriteBatchGlobalUniformBuffer));
    }

    void VulkanRenderer::resize(u32 width, u32 height) {
        VulkanRendererBase::resize(width, height);

        _isGlobalUniformBufferUpdated = true;

        _globalUniformBuffer.projection = ortho(0, _viewport.width, 0, _viewport.height, -1.f, 1.f);
        _resourceAllocator.setBufferData(_globalUniformBufferId, &_globalUniformBuffer, sizeof(VulkanSpriteBatchGlobalUniformBuffer));
    }

    void VulkanRenderer::draw(u32 x, u32 y, u32 width, u32 height, u32 textureId) {
        _spriteDrawCommands.emplace_back(textureId, height, width, x, y);
    }

    void VulkanRenderer::display() {
        if (_isSuspended || _spriteDrawCommands.empty()) {
            _spriteDrawCommands.clear();
            return;
        }

        _generateDrawCommands();

        _beginDrawing();
        _copyBufferData();

        const auto& renderTargetTexture = _resourceAllocator.getResource<VulkanTextureResource>(_renderTargetId);

        auto colorAttachment = vk::RenderingAttachmentInfo{};
        colorAttachment.imageView = renderTargetTexture.view;
        colorAttachment.imageLayout = vk::ImageLayout::eGeneral;
        colorAttachment.clearValue = vk::ClearColorValue{ 0.9f, 0.9f, 0.9f, 1.0f };
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

        auto renderingInfo = vk::RenderingInfo{};
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.renderArea = vk::Rect2D{ {}, _viewport };
        renderingInfo.layerCount = 1;

        auto& currentFrame = _frameData[_frameIndex];
        auto& commandBuffer = currentFrame.commandBuffer;

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _spriteBatchPipeline);

        const auto& spriteDrawData = currentFrame.spriteDrawData;

        const auto& indexBuffer = _resourceAllocator.getResource<VulkanBufferResource>(spriteDrawData.indexBuffer);
        const auto& vertexBuffer = _resourceAllocator.getResource<VulkanBufferResource>(spriteDrawData.vertexBuffer);

        commandBuffer.bindIndexBuffer(indexBuffer.handle, 0, vk::IndexType::eUint32);
        commandBuffer.bindVertexBuffers(0, vertexBuffer.handle, vk::DeviceSize{ 0 });

        for (const auto& drawCommand : spriteDrawData.drawCommands) {
            const auto& texture = _resourceAllocator.getResource<VulkanTextureResource>(drawCommand.textureId);

            auto textureDescriptorIdentifier = VulkanDescriptorSetIdentifier{};
            textureDescriptorIdentifier.layoutId = _spriteTextureDescriptorSetLayoutId;
            textureDescriptorIdentifier.bindingResourceId0 = drawCommand.textureId;
            textureDescriptorIdentifier.frameId = _frameIndex;

            const auto textureDescriptorSetId = _descriptorCache.getOrAllocateDescriptorSet(textureDescriptorIdentifier);
            if (!_descriptorCache.isDescriptorSetBindingWritenTo(0, textureDescriptorSetId)) {
                _descriptorCache.updateTextureDescriptorSetBinding(0, textureDescriptorSetId, drawCommand.textureId, _resourceAllocator);
            }

            _descriptorCache.lockDescriptorSet(textureDescriptorSetId);

            const auto imageDescriptorSet = _descriptorCache.getDescriptorSetHandle(textureDescriptorSetId);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _spriteBatchPipelineLayout, 1, imageDescriptorSet, nullptr);

            const auto firstIndex = drawCommand.quadOffset * 6;
            const auto vertexOffset = drawCommand.quadOffset * 4;

            commandBuffer.drawIndexed(drawCommand.indexCount, 1, firstIndex, vertexOffset, 0);
        }

        commandBuffer.endRendering();

        _endDrawing();
    }

    void VulkanRenderer::_generateDrawCommands() {
        const auto isLess = [](const auto& left, const auto& right) { return left.textureId < right.textureId; };
        std::sort(_spriteDrawCommands.begin(), _spriteDrawCommands.end(), isLess);

        _vertices.resize(_spriteDrawCommands.size() * 4);
        _indices.resize(_spriteDrawCommands.size() * 6);

        auto& currentFrame = _frameData[_frameIndex];

        auto currentIndexOffset = 0ull;
        auto currentVertexOffset = 0ull;
        auto currentQuadOffset = 0;

        auto currentEntryDrawCommand = VulkanSpriteBatchEntryDrawCommand{};
        currentEntryDrawCommand.textureId = _spriteDrawCommands.front().textureId;

        for (const auto& command : _spriteDrawCommands) {
            if (command.textureId != currentEntryDrawCommand.textureId) {
                currentFrame.spriteDrawData.drawCommands.push_back(currentEntryDrawCommand);

                currentEntryDrawCommand.textureId = command.textureId;
                currentEntryDrawCommand.quadOffset = currentQuadOffset;
                currentEntryDrawCommand.indexCount = 0;
            }

            _vertices[currentVertexOffset].position = { command.x,  command.y };
            _vertices[currentVertexOffset + 1].position = { command.x + command.width, command.y };
            _vertices[currentVertexOffset + 2].position = { command.x + command.width, command.y + command.height };
            _vertices[currentVertexOffset + 3].position = { command.x, command.y + command.height };

            _vertices[currentVertexOffset].texturePosition = { 1.0f, 0.0f };
            _vertices[currentVertexOffset + 1].texturePosition = { 0.0f, 0.0f };
            _vertices[currentVertexOffset + 2].texturePosition = { 0.0f, 1.0f };
            _vertices[currentVertexOffset + 3].texturePosition = { 1.0f, 1.0f };

            const auto quadOffset = currentEntryDrawCommand.indexCount / 6 * 4;
            
            _indices[currentIndexOffset] = quadOffset;
            _indices[currentIndexOffset + 1] = quadOffset + 1;
            _indices[currentIndexOffset + 2] = quadOffset + 2;

            _indices[currentIndexOffset + 3] = quadOffset + 2;
            _indices[currentIndexOffset + 4] = quadOffset + 3;
            _indices[currentIndexOffset + 5] = quadOffset;

            currentEntryDrawCommand.indexCount += 6;

            currentVertexOffset += 4;
            currentIndexOffset += 6;
            currentQuadOffset += 1;
        }

        currentFrame.spriteDrawData.drawCommands.push_back(currentEntryDrawCommand);
    }

    void VulkanRenderer::_copyBufferData() {
        auto& currentFrame = _frameData[_frameIndex];
        auto& spriteDrawData = currentFrame.spriteDrawData;

        const auto& vertexStagingBuffer = _resourceAllocator.getResource<VulkanBufferResource>(spriteDrawData.vertexStagingBuffer);
        const auto& indexStagingBuffer = _resourceAllocator.getResource<VulkanBufferResource>(spriteDrawData.indexStagingBuffer);
        const auto& vertexBuffer = _resourceAllocator.getResource<VulkanBufferResource>(spriteDrawData.vertexBuffer);
        const auto& indexBuffer = _resourceAllocator.getResource<VulkanBufferResource>(spriteDrawData.indexBuffer);

        const auto vertexBufferSize = _vertices.size() * sizeof(VulkanSpriteVertex);
        const auto indexBufferSize = _indices.size() * sizeof(u32);

        memcpy(vertexStagingBuffer.allocationInfo.pMappedData, _vertices.data(), vertexBufferSize);
        memcpy(indexStagingBuffer.allocationInfo.pMappedData, _indices.data(), indexBufferSize);

        auto memoryBarrier = vk::MemoryBarrier2{};
        memoryBarrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        memoryBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        memoryBarrier.dstStageMask = vk::PipelineStageFlagBits2::eIndexInput | vk::PipelineStageFlagBits2::eVertexInput;
        memoryBarrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead;

        auto dependencyInfo = vk::DependencyInfo{};
        dependencyInfo.pMemoryBarriers = &memoryBarrier;
        dependencyInfo.memoryBarrierCount = 1;

        auto vertexBufferCopy = vk::BufferCopy{};
        vertexBufferCopy.size = vertexBufferSize;

        auto indexBufferCopy = vk::BufferCopy{};
        indexBufferCopy.size = indexBufferSize;

        auto commandBuffer = currentFrame.commandBuffer;
        commandBuffer.copyBuffer(vertexStagingBuffer.handle, vertexBuffer.handle, vertexBufferCopy);
        commandBuffer.copyBuffer(indexStagingBuffer.handle, indexBuffer.handle, indexBufferCopy);
        commandBuffer.pipelineBarrier2(dependencyInfo);
    }

    void VulkanRenderer::_beginDrawing() {
        auto& currentFrame = _frameData[_frameIndex];

        vk::resultCheck(_logicalDevice.waitForFences(currentFrame.renderingFence, true, UINT64_MAX), "waitForFences");
        _logicalDevice.resetFences(currentFrame.renderingFence);

        _descriptorCache.clearFrameDescriptorSetLocks(_frameIndex);

        auto commandBuffer = currentFrame.commandBuffer;

        commandBuffer.reset();
        commandBuffer.begin(vk::CommandBufferBeginInfo{});

        const auto& renderTargetTexture = _resourceAllocator.getResource<VulkanTextureResource>(_renderTargetId);

        const auto commandBufferHelper = VulkanCommandBufferHelper{ commandBuffer };
        commandBufferHelper.transitionImage(renderTargetTexture.handle, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

        auto viewport = vk::Viewport{};
        viewport.width = _viewport.width;
        viewport.height = _viewport.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        auto scissor = vk::Rect2D{};
        scissor.extent = _viewport;

        commandBuffer.setViewport(0, viewport);
        commandBuffer.setScissor(0, scissor);

        auto globalUniformBufferDescriptorIdentifier = VulkanDescriptorSetIdentifier{};
        globalUniformBufferDescriptorIdentifier.layoutId = _globalUniformBufferDescriptorSetLayoutId;
        globalUniformBufferDescriptorIdentifier.bindingResourceId0 = _globalUniformBufferId;
        globalUniformBufferDescriptorIdentifier.frameId = _frameIndex;

        const auto globalUniformBufferDescriptorId = _descriptorCache.getOrAllocateDescriptorSet(globalUniformBufferDescriptorIdentifier);
        if (!_descriptorCache.isDescriptorSetBindingWritenTo(0, globalUniformBufferDescriptorId) || _isGlobalUniformBufferUpdated) {
            _descriptorCache.updateUniformDescriptorSetBinding(0, globalUniformBufferDescriptorId, _globalUniformBufferId, _resourceAllocator);
        }

        const auto uniformDescriptorSet = _descriptorCache.getDescriptorSetHandle(globalUniformBufferDescriptorId);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _spriteBatchPipelineLayout, 0, uniformDescriptorSet, nullptr);

        _descriptorCache.lockDescriptorSet(globalUniformBufferDescriptorId);
    }

    void VulkanRenderer::_endDrawing() {
        auto& currentFrame = _frameData[_frameIndex];
        auto& commandBuffer = currentFrame.commandBuffer;

        const auto imageIndex = _logicalDevice.acquireNextImageKHR(_swapchain, UINT64_MAX, currentFrame.swapchainSemaphore);
        if (imageIndex.result == vk::Result::eSuccess) {
            const auto& renderTargetTexture = _resourceAllocator.getResource<VulkanTextureResource>(_renderTargetId);
            auto& swapchainImage = _swapchainImages[imageIndex.value];

            const auto commandBufferHelper = VulkanCommandBufferHelper{ commandBuffer };
            commandBufferHelper.transitionImage(renderTargetTexture.handle, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
            commandBufferHelper.transitionImage(swapchainImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            commandBufferHelper.copyImageToImage(renderTargetTexture.handle, swapchainImage, _viewport, _viewport);
            commandBufferHelper.transitionImage(swapchainImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
        }

        commandBuffer.end();

        auto semaphoreWaitInfo = vk::SemaphoreSubmitInfo{};
        semaphoreWaitInfo.semaphore = currentFrame.swapchainSemaphore;
        semaphoreWaitInfo.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        semaphoreWaitInfo.value = 1;

        auto semaphoreSignalInfo = vk::SemaphoreSubmitInfo{};
        semaphoreSignalInfo.semaphore = currentFrame.renderingSemaphore;
        semaphoreSignalInfo.stageMask = vk::PipelineStageFlagBits2::eAllGraphics;
        semaphoreSignalInfo.value = 1;

        auto commandBufferSubmitInfo = vk::CommandBufferSubmitInfo{};
        commandBufferSubmitInfo.commandBuffer = currentFrame.commandBuffer;

        auto submitInfo = vk::SubmitInfo2{};
        submitInfo.waitSemaphoreInfoCount = 1;
        submitInfo.pWaitSemaphoreInfos = &semaphoreWaitInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos = &semaphoreSignalInfo;
        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

        try {
            _graphicsQueue.submit2(submitInfo, currentFrame.renderingFence);

            if (imageIndex.result == vk::Result::eSuccess) {
                auto presentInfo = vk::PresentInfoKHR{};
                presentInfo.pSwapchains = &_swapchain;
                presentInfo.swapchainCount = 1;
                presentInfo.pWaitSemaphores = &currentFrame.renderingSemaphore;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pImageIndices = &imageIndex.value;

                vk::resultCheck(_graphicsQueue.presentKHR(presentInfo), "presentKHR");
            }
        } catch (const vk::SystemError&) {
            _isSuspended = true;
        }

        if (imageIndex.result != vk::Result::eSuccess) {
            _isSuspended = true;
        }

        currentFrame.spriteDrawData.drawCommands.clear();
        _spriteDrawCommands.clear();

        _frameIndex = (_frameIndex + 1) % ConcurrentFrameCount;
    }
}
