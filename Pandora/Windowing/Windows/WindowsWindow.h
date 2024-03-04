#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "Pandora/Windowing/Window.h"
#include "Pandora/Windowing/WindowEvent.h"

#include <windows.h>
#include <windowsx.h>

#include <array>
#include <vector>

namespace Pandora::Implementation {

    struct WindowsWindow {
        WindowsWindow();
        ~WindowsWindow();

        void show() const;
        void poll();
     
        void setFramebufferSize(u32 width, u32 height);
        void setTitle(std::string_view title) const;

        static constexpr auto KeyboardKeyCount = static_cast<usize>(KeyboardKeyType::Unknown) + 1;
        static constexpr auto MouseButtonCount = static_cast<usize>(MouseButtonType::Unknown) + 1;

        HINSTANCE instanceHandle{};
        HWND windowHandle{};

        std::array<bool, KeyboardKeyCount> keyState{};
        std::array<bool, MouseButtonCount> buttonState{};
        std::vector<Event> pendingEvents{};

        Vector2u cursorPosition{};

        Vector2u framebufferSize{};
        Vector2u framebufferSizeBeforeResize{};

        bool isCloseRequested{};

        bool isMinimized{};
        bool isMaximized{};
    };
}
