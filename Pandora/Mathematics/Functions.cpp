#include "Functions.h"

#include <numbers>

namespace Pandora {

    float DegreesToRadians(float degrees) {
        return degrees / (std::numbers::pi_v<float> * 180.0f);
    }
}
