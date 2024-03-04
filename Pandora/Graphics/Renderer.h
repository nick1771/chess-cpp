#pragma once

#include "Pandora/Pandora.h"

#include <memory>

namespace Pandora::Implementation {
    class VulkanRenderer;
}

namespace Pandora {

    class Window;
    class Image;

    class Renderer {
    public:
        Renderer();
        ~Renderer();

        u32 createTexture(const Image& image);

        void initialize(const Window& window);
        void resize(u32 width, u32 height);
        void draw(u32 x, u32 y, u32 width, u32 height, u32 textureId);
        void display();
    private:
        std::unique_ptr<Implementation::VulkanRenderer> _implementation;
    };
}
