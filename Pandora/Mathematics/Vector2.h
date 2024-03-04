#pragma once

#include "Pandora/Pandora.h"
#include "Concepts.h"

#include <cmath>

namespace Pandora {

    template<Numeric T>
    struct Vector2 {
        union {
            T data[2]{};
            struct {
                T x;
                T y;
            };
        };

        constexpr Vector2() = default;

        template<std::convertible_to<T> B>
        constexpr Vector2(B x, B y)
            : x(static_cast<T>(x))
            , y(static_cast<T>(y)) {
        }

        template<std::convertible_to<T> B>
        constexpr Vector2(B value)
            : x(static_cast<T>(value))
            , y(static_cast<T>(value)) {
        }

        template<std::convertible_to<T> B>
        constexpr Vector2(const Vector2<B>& value)
            : x(static_cast<T>(value.x))
            , y(static_cast<T>(value.y)) {
        }

        f32 magnitude() const {
            const auto sum = x * x + y * y;
            return std::sqrtf(sum);
        }

        Vector2<f32> normalize() const {
            const auto magnitude = magnitude();
            return { x / magnitude, y / magnitude };
        }

        constexpr bool operator==(const Vector2<T> other) const {
            return x == other.x && y == other.y;
        }
    };
}
