#include "GraphicsDevice.h"
#include "Vulkan/VulkanGraphicsDevice.h"
#include "Pandora/Mathematics/Vector.h"

namespace Pandora {

    GraphicsDevice::GraphicsDevice()
        : _implementation(std::make_unique<Implementation::VulkanGraphicsDevice>()) {
    }

    GraphicsDevice::~GraphicsDevice() = default;

    void GraphicsDevice::configure(const Window& window) {
        _implementation->configure(window);
    }

    usize GraphicsDevice::createTexture(const TextureCreateInfo& createInfo) {
        return _implementation->createTexture(createInfo);
    }

    usize GraphicsDevice::createPipeline(const PipelineCreateInfo& createInfo) {
        return _implementation->createPipeline(createInfo);
    }

    usize GraphicsDevice::createBuffer(const BufferCreateInfo& createInfo) {
        return _implementation->createBuffer(createInfo);
    }

    usize GraphicsDevice::createBindGroup(const BindGroup& group) {
        return _implementation->createBindGroup(group);
    }

    void GraphicsDevice::setTextureData(usize textureId, std::span<const std::byte> data, u32 width, u32 height) {
        _implementation->setTextureData(textureId, data, width, height);
    }

    void GraphicsDevice::setBufferData(usize bufferId, std::span<const std::byte> data) {
        _implementation->setBufferData(bufferId, data);
    }

    usize GraphicsDevice::getCurrentFrameIndex() const {
        return _implementation->getCurrentFrameIndex();
    }

    Vector2u GraphicsDevice::getCurrentViewport() const {
        return _implementation->getCurrentViewport();
    }

    bool GraphicsDevice::isSuspended() const {
        return _implementation->isSuspended();
    }

    void GraphicsDevice::beginCommands() {
        _implementation->beginCommands();
    }

    void GraphicsDevice::beginRenderPass() {
        _implementation->beginRenderPass();
    }

    void GraphicsDevice::copyBuffer(usize bufferId, std::span<const std::byte> data) {
        _implementation->copyBuffer(bufferId, data);
    }

    void GraphicsDevice::drawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset) {
        _implementation->drawIndexed(indexCount, indexOffset, vertexOffset);
    }

    void GraphicsDevice::setBindGroupBinding(usize bindGroupId, u32 bindingIndex, usize resourceId) {
        _implementation->setBindGroupBinding(bindGroupId, bindingIndex, resourceId);
    }

    void GraphicsDevice::setPipeline(usize pipelineId) {
        _implementation->setPipeline(pipelineId);
    }

    void GraphicsDevice::setVertexBuffer(usize vertexBufferId) {
        _implementation->setVertexBuffer(vertexBufferId);
    }

    void GraphicsDevice::setIndexBuffer(usize indexBufferId) {
        _implementation->setIndexBuffer(indexBufferId);
    }

    void GraphicsDevice::setBindGroup(usize bindGroupId, usize pipelineId, u32 slot) {
        _implementation->setBindGroup(bindGroupId, pipelineId, slot);
    }

    PresentationResultType GraphicsDevice::endAndPresent() {
        return _implementation->endAndPresent();
    }
}
