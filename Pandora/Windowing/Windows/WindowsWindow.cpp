#include "WindowsWindow.h"

#include <stdexcept>
#include <print>

namespace Pandora::Implementation {

    static std::string utf8FromWideString(std::wstring_view wideString) {
        const auto stringSize = WideCharToMultiByte(CP_UTF8, 0, wideString.data(), -1, nullptr, 0, nullptr, nullptr);

        auto string = std::string(stringSize, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wideString.data(), -1, string.data(), stringSize, nullptr, nullptr);

        return string;
    }

    static std::wstring wideStringFromUtf8(std::string_view string) {
        const auto wideStringSize = MultiByteToWideChar(CP_UTF8, 0, string.data(), -1, nullptr, 0);

        auto wideString = std::wstring(wideStringSize, '\0');
        MultiByteToWideChar(CP_UTF8, 0, string.data(), -1, wideString.data(), wideStringSize);

        return wideString;
    }

    static std::string getLastErrorMessage() {
        const auto errorCode = GetLastError();
        if (errorCode == 0) {
            return std::string{};
        }

        auto messageBuffer = std::wstring(512, '\0');
        auto messageFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

        const auto messageLength = FormatMessageW(
            messageFlags,
            nullptr,
            errorCode,
            0,
            messageBuffer.data(),
            static_cast<DWORD>(messageBuffer.size()),
            nullptr
        );

        messageBuffer.erase(messageLength);

        return utf8FromWideString(messageBuffer);
    }

    template<class T>
    T checkWindowsResult(T result) {
        if (static_cast<bool>(result)) {
            return result;
        } else {
            throw std::runtime_error{ getLastErrorMessage() };
        }
    }

    static KeyboardKeyType mapKeyCodeToEnum(WPARAM keyCode) {
        switch (keyCode) {
        case 0x1B: return KeyboardKeyType::Esc;
        case 0x41: return KeyboardKeyType::A;
        case 0x44: return KeyboardKeyType::D;
        case 0x53: return KeyboardKeyType::S;
        case 0x57: return KeyboardKeyType::W;
        case 0x20: return KeyboardKeyType::Space;
        default: return KeyboardKeyType::Unknown;
        }
    }

    static std::pair<i32, i32> getWindowSizeForFramebuffer(i32 width, i32 height) {
        auto clientSize = RECT{ 0, 0, width, height };
        checkWindowsResult(AdjustWindowRect(&clientSize, WS_OVERLAPPEDWINDOW, false));

        auto physicalWidth = clientSize.right - clientSize.left;
        auto physicalHeight = clientSize.bottom - clientSize.top;

        return std::make_pair(physicalWidth, physicalHeight);
    }

    constexpr auto WindowClassName = L"WINDOW_CLASS";
    constexpr auto WindowPropertyName = L"WINDOW_PROPERTY";

    static LRESULT windowProcedure(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
        const auto windowState = static_cast<WindowsWindow*>(GetPropW(handle, WindowPropertyName));
        if (windowState == nullptr) {
            return DefWindowProcW(handle, message, wparam, lparam);
        }

        switch (message) {
        case WM_CLOSE: {
            windowState->isCloseRequested = true;
            return 0;
        }
        case WM_KEYDOWN: {
            auto keyType = mapKeyCodeToEnum(wparam);
            windowState->keyState[static_cast<usize>(keyType)] = true;
            return 0;
        }
        case WM_KEYUP: {
            auto keyType = mapKeyCodeToEnum(wparam);
            windowState->keyState[static_cast<usize>(keyType)] = false;
            return 0;
        }
        case WM_MOUSEMOVE: {
            windowState->cursorPosition.x = static_cast<u32>(GET_X_LPARAM(lparam));
            windowState->cursorPosition.y = static_cast<u32>(GET_Y_LPARAM(lparam));
            return 0;
        }
        case WM_LBUTTONDOWN: {
            windowState->buttonState[static_cast<usize>(MouseButtonType::Left)] = true;
            return 0;
        }
        case WM_LBUTTONUP: {
            windowState->buttonState[static_cast<usize>(MouseButtonType::Left)] = false;
            windowState->pendingEvents.emplace_back(MouseButtonReleaseEvent{ MouseButtonType::Left });
            return 0;
        }
        case WM_RBUTTONDOWN: {
            windowState->buttonState[static_cast<usize>(MouseButtonType::Right)] = true;
            return 0;
        }
        case WM_RBUTTONUP: {
            windowState->buttonState[static_cast<usize>(MouseButtonType::Right)] = false;
            return 0;
        }
        case WM_SIZE: {
            const auto isMaximized = wparam == SIZE_MAXIMIZED || (windowState->isMaximized && wparam != SIZE_RESTORED);
            const auto isMinimized = wparam == SIZE_MINIMIZED;

            windowState->framebufferSize.x = static_cast<u32>(GET_X_LPARAM(lparam));
            windowState->framebufferSize.y = static_cast<u32>(GET_Y_LPARAM(lparam));

            if (windowState->isMinimized && wparam == SIZE_RESTORED || isMaximized) {
                const auto [width, height] = windowState->framebufferSize.data;
                windowState->pendingEvents.emplace_back(WindowResizeEndEvent{ width, height });
            } else if (isMinimized) {
                windowState->pendingEvents.emplace_back(WindowResizeEndEvent{ 0, 0 });
            }

            windowState->isMinimized = isMinimized;
            windowState->isMaximized = isMaximized;

            return 0;
        }
        case WM_ENTERSIZEMOVE: {
            windowState->framebufferSizeBeforeResize = windowState->framebufferSize;
            windowState->pendingEvents.emplace_back(WindowResizeBeginEvent{});
            return 0;
        }
        case WM_EXITSIZEMOVE: {
            if (windowState->framebufferSizeBeforeResize != windowState->framebufferSize) {
                const auto [width, height] = windowState->framebufferSize.data;
                windowState->pendingEvents.emplace_back(WindowResizeEndEvent{ width, height });
                return 0;
            }

            [[fallthrough]];
        }
        default:
            return DefWindowProcW(handle, message, wparam, lparam);
        }
    }

    constexpr auto DefaultWindowWidth = 800;
    constexpr auto DefaultWindowHeight = 600;

    WindowsWindow::WindowsWindow() {
        instanceHandle = checkWindowsResult(GetModuleHandleW(nullptr));

        auto windowClass = WNDCLASSW{};
        windowClass.hInstance = instanceHandle;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = WindowClassName;
        windowClass.lpfnWndProc = windowProcedure;
        windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;

        checkWindowsResult(RegisterClassW(&windowClass));

        windowHandle = CreateWindowW(
            WindowClassName,
            L"Default Title",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            nullptr,
            nullptr,
            instanceHandle,
            nullptr
        );

        checkWindowsResult(windowHandle);

        checkWindowsResult(SetPropW(windowHandle, WindowPropertyName, this));
        checkWindowsResult(SetFocus(windowHandle));

        setFramebufferSize(DefaultWindowWidth, DefaultWindowHeight);
    }

    WindowsWindow::~WindowsWindow() {
        RemovePropW(windowHandle, WindowPropertyName);
        DestroyWindow(windowHandle);
    }

    void WindowsWindow::show() const {
        ShowWindow(windowHandle, SW_SHOWNA);
    }

    void WindowsWindow::poll() {
        pendingEvents.clear();

        auto message = MSG{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    void WindowsWindow::setFramebufferSize(u32 width, u32 height) {
        const auto requiredWidth = static_cast<i32>(width);
        const auto requiredHeight = static_cast<i32>(height);

        const auto [windowWidth, windowHeight] = getWindowSizeForFramebuffer(requiredWidth, requiredHeight);
        const auto positionUpdateFlags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER;
        SetWindowPos(windowHandle, nullptr, 0, 0, windowWidth, windowHeight, positionUpdateFlags);

        framebufferSize.x = width;
        framebufferSize.y = height;
    }

    void WindowsWindow::setTitle(std::string_view title) const {
        const auto wideTitle = wideStringFromUtf8(title);
        SetWindowTextW(windowHandle, wideTitle.data());
    }
}