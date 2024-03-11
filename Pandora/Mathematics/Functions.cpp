#include "Functions.h"

#include <numbers>

namespace Pandora {

    float degreesToRadians(float degrees) {
        return degrees / (std::numbers::pi_v<float> * 180.0f);
    }
}
