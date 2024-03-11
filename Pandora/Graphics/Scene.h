#pragma once

#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Graphics/Texture.h"

#include <vector>

namespace Pandora {

    struct Sprite {
        Vector2f scale = Vector2f{ 1.f };
        Vector2f position{};
        Vector2f origin{};
        Texture texture{};
        u32 zIndex{};
    };

    struct Scene {
        std::vector<Sprite> sprites{};
    };
}
