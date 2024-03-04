#pragma once

#include "Matrix4.h"

namespace Pandora {

    Matrix4<f32> ortho(f32 left, f32 right, f32 bottom, f32 top, f32 far, f32 near);
}
