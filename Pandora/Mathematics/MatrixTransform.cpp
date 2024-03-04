#include "MatrixTransform.h"

namespace Pandora {

    Matrix4<f32> ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
        return {
            2.f / (right - left), 0.f, 0.f, 0.f,
            0.f, 2.f / (top - bottom), 0.f, 0.f,
            0.f, 0.f, -2.0f / (far - near), 0.f,
            -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.f
        };
    }
}
