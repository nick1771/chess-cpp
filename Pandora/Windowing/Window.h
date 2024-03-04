#pragma once

#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Pandora.h"
#include "WindowEvent.h"

#include <string_view>
#include <vector>
#include <memory>
#include <any>

namespace Pandora::Implementation {
    struct WindowsWindow;
}

namespace Pandora {

    class Window {
    public:
        Window();
        ~Window();

        void show() const;
        void poll();

        bool isKeyPressed(KeyboardKeyType keyType) const;
        bool isMouseButtonPressed(MouseButtonType buttonType) const;
        bool isCloseRequested() const;

        void setFramebufferSize(u32 width, u32 height);
        void setCloseRequested(bool isCloseRequested);
        void setTitle(std::string_view title) const;

        std::vector<Event> getPendingEvents();
        std::any getNativeHandle() const;
        Vector2u getCursorPosition() const;
        Vector2u getFramebufferSize() const;
    private:
        std::unique_ptr<Implementation::WindowsWindow> _implementation;
    };
}
