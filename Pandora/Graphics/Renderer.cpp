#include "Renderer.h"

#include "Pandora/Windowing/Window.h"
#include "Pandora/Image.h"

#include "Vulkan/VulkanRenderer.h"

namespace Pandora {

    Renderer::Renderer()
        : _implementation(std::make_unique<Implementation::VulkanRenderer>()) {
    }

    Renderer::~Renderer() = default;

    u32 Renderer::createTexture(const Image& image) {
        return _implementation->createTexture(image);
    }

    void Renderer::initialize(const Window& window) {
        _implementation->initialize(window);
    }

    void Renderer::resize(u32 width, u32 height) {
        _implementation->resize(width, height);
    }

    void Renderer::draw(u32 x, u32 y, u32 width, u32 height, u32 textureId) {
        _implementation->draw(x, y, width, height, textureId);
    }

    void Renderer::display() {
        _implementation->display();
    }
}
