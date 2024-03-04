#pragma once

#include "Mathematics/Vector.h"
#include "Color.h"

#include <filesystem>
#include <vector>

namespace Pandora {

    class Image {
    public:
        static Image load(const std::filesystem::path& path);
        static Image create(u32 width, u32 height, Color8 color);

        const u8* getPixels() const;
        Vector2u getSize() const;
    private:
        Vector2u _size{};
        std::vector<u8> _pixels{};
    };
}
