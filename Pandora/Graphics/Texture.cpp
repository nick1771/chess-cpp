#include "Texture.h"
#include "GraphicsDevice.h"
#include "Pandora/Image.h"

#include <span>

namespace Pandora {

    Texture::Texture(GraphicsDevice& device, const Image& image) {
        const auto imageSize = image.getSize();
        const auto imageByteSize = static_cast<usize>(imageSize.x) * imageSize.y * 4;

        auto textureCreateInfo = TextureCreateInfo{};
        textureCreateInfo.format = TextureFormatType::RGBA8UNorm;
        textureCreateInfo.usage = TextureUsageType::Sampled;
        textureCreateInfo.width = imageSize.x;
        textureCreateInfo.height = imageSize.y;

        _id = device.createTexture(textureCreateInfo);
        device.setTextureData(_id, std::span{ image.getPixels(), imageByteSize }, imageSize.x, imageSize.y);

        _size = imageSize;
    }

    Texture::Texture(GraphicsDevice& device, const std::filesystem::path& path)
        : Texture(device, Image::load(path)) {
    }

    Vector2u Texture::getSize() const {
        return _size;
    }

    usize Texture::getDeviceHandleId() const {
        return _id;
    }
}
