#include "Window.h"
#include "Windows/WindowsWindow.h"

namespace Pandora {

    Window::Window()
        : _implementation(std::make_unique<Implementation::WindowsWindow>()) {
    }

    Window::~Window() = default;

    void Window::show() const {
        _implementation->show();
    }

    void Window::poll() {
        _implementation->poll();
    }

    bool Window::isKeyPressed(KeyboardKeyType keyType) const {
        const auto index = static_cast<usize>(keyType);
        return _implementation->keyState[index];
    }

    bool Window::isMouseButtonPressed(MouseButtonType buttonType) const {
        const auto index = static_cast<usize>(buttonType);
        return _implementation->buttonState[index];
    }

    bool Window::isCloseRequested() const {
        return _implementation->isCloseRequested;
    }

    void Window::setFramebufferSize(u32 width, u32 height) {
        _implementation->setFramebufferSize(width, height);
    }

    void Window::setCloseRequested(bool isCloseRequested) {
        _implementation->isCloseRequested = isCloseRequested;
    }

    void Window::setTitle(std::string_view title) const {
        _implementation->setTitle(title);
    }

    std::any Window::getNativeHandle() const {
        return _implementation->windowHandle;
    }

    Vector2u Window::getCursorPosition() const {
        return _implementation->cursorPosition;
    }

    Vector2u Window::getFramebufferSize() const {
        return _implementation->framebufferSize;
    }

    std::vector<Event> Window::getPendingEvents() {
        return _implementation->pendingEvents;
    }
}
