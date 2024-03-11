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

        void pollEvents();
        void waitEvents();

        bool isKeyPressed(KeyboardKeyType keyType) const;
        bool isMouseButtonPressed(MouseButtonType buttonType) const;
        bool isCloseRequested() const;

        void setFramebufferSize(u32 width, u32 height);
        void setCloseRequested(bool isCloseRequested);
        void setResizeable(bool isResizeable) const;
        void setTitle(std::string_view title) const;

        std::vector<Event> getPendingEvents();
        std::any getNativeHandle() const;
        Vector2u getCursorPosition() const;
        Vector2u getFramebufferSize() const;
    private:
        std::unique_ptr<Implementation::WindowsWindow> _implementation;
    };
}
