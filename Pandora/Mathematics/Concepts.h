#pragma once

#include <concepts>

namespace Pandora {

    template<class T>
    concept Numeric = std::integral<T> || std::floating_point<T>;
}
