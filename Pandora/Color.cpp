#include "Color.h"

namespace Pandora {

    Color8 Color8::Red = Color8{ 255, 0, 0, 255 };
    Color8 Color8::Green = Color8{ 0, 255, 0, 255 };
    Color8 Color8::Blue = Color8{ 0, 0, 255, 255 };
    Color8 Color8::White = Color8{ 255, 255, 255, 255 };

    Color8::Color8(u8 red, u8 green, u8 blue, u8 alpha)
        : components({ red, green, blue, alpha }) {
    }
}
