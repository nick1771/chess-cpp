#pragma once

#include <concepts>
#include <variant>

namespace Pandora {

    enum class KeyboardKeyType {
        A,
        S,
        D,
        W,
        Esc,
        Space,
        Unknown
    };

    enum class MouseButtonType {
        Left,
        Right,
        Unknown
    };

    struct MouseButtonReleaseEvent {
        MouseButtonType buttonType{};
    };

    struct WindowResizeEndEvent {
        u32 width{};
        u32 height{};
    };

    struct WindowResizeBeginEvent {
    };

    class Event{
        using EventStorage = std::variant<
            MouseButtonReleaseEvent,
            WindowResizeBeginEvent,
            WindowResizeEndEvent
        >;
    public:
        template<class T>
            requires std::constructible_from<EventStorage, T>
        Event(T&& data)
            : _data(std::forward<T>(data)) {
        }

        template<class T>
        bool is() const {
            return std::holds_alternative<T>(_data);
        }

        template<class T>
        const T& getData() const {
            return std::get<T>(_data);
        }
    private:
        EventStorage _data{};
    };
}
