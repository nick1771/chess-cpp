#pragma once

#include "Pandora/Pandora.h"
#include "Concepts.h"

#include <cmath>

namespace Pandora {

    template<Numeric T>
    struct Vector4 {
        union {
            T data[4]{};
            struct {
                T x;
                T y;
                T z;
                T w;
            };
        };

        constexpr Vector4() = default;

        template<std::convertible_to<T> B>
        constexpr Vector4(B x, B y, B z, B w)
            : x(static_cast<T>(x))
            , y(static_cast<T>(y))
            , z(static_cast<T>(z)) 
            , w(static_cast<T>(w)) {
        }

        template<std::convertible_to<T> B>
        constexpr Vector4(B value)
            : x(static_cast<T>(value))
            , y(static_cast<T>(value))
            , z(static_cast<T>(value)) 
            , w(static_cast<T>(value)) {
        }
    };
}
