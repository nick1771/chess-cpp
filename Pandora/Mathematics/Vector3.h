#pragma once

#include "Pandora/Pandora.h"
#include "Concepts.h"

namespace Pandora::Mathematics {

    template<Numeric T>
    struct Vector3 {
        union {
            T data[3]{};
            struct {
                T x;
                T y;
                T z;
            };
        };

        constexpr Vector3() = default;

        template<std::convertible_to<T> B>
        constexpr Vector3(B x, B y, B z)
            : x(static_cast<T>(x))
            , y(static_cast<T>(y))
            , z(static_cast<T>(z)) {
        }

        template<std::convertible_to<T> B>
        constexpr Vector3(B value)
            : x(static_cast<T>(value))
            , y(static_cast<T>(value))
            , z(static_cast<T>(value)) {
        }
    };
}
