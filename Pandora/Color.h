#pragma once

#include "Pandora.h"

#include <array>

namespace Pandora {

    struct Color8 {
        std::array<u8, 4> components{};

        Color8(u8 red, u8 green, u8 blue, u8 alpha = 255);
        Color8() = default;

        static Color8 Red;
        static Color8 Blue;
        static Color8 Green;
    };
}
