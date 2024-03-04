#pragma once

#include "Pandora/Pandora.h"
#include "Concepts.h"

namespace Pandora {

    template<Numeric T>
    struct Matrix4 {
        T data[16]{};
    };
}
