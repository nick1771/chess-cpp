#pragma once

#include "Pandora/Pandora.h"

#include <limits>

namespace Pandora::Constants {

    inline constexpr usize ConcurrentFrameCount = 2;
    inline constexpr usize MaximumVertexElementCount = 2;
    inline constexpr usize MaximumBindGroupCount = 3;
    inline constexpr usize MaximumBindGroupBindingCount = 1;
    inline constexpr usize MaximumSpriteCount = 100;
    inline constexpr usize MaximumIdValue = std::numeric_limits<usize>::max();
    inline constexpr usize MaximumTextureCount = 100 * ConcurrentFrameCount;
    inline constexpr usize MaximumUniformBufferCount = 100 * ConcurrentFrameCount;
}
