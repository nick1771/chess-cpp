#pragma once

#include "Pandora/Mathematics/Vector.h"

#include <filesystem>
#include <compare>

namespace Pandora {

    class GraphicsDevice;
    class Image;

    class Texture {
    public:
        Texture(GraphicsDevice& device, const Image& image);
        Texture(GraphicsDevice& device, const std::filesystem::path& path);
        Texture() = default;

        usize getDeviceHandleId() const;
        Vector2u getSize() const;

        auto operator<=>(const Texture&) const = default;
    private:
        Vector2u _size{};
        usize _id{};
    };
}
