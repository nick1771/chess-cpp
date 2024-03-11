#pragma once

#include "Pandora/Mathematics/Vector.h"

namespace Pandora {

    struct Camera {
        Vector2f position{};
        Vector2f size{};

        auto operator<=>(const Camera&) const = default;
    };
}
