#include "Image.h"
#include "File.h"

#include "Vendor/Stb/stb_image.h"

namespace Pandora {

    constexpr u32 ChannelCount = 4;

    Image Image::load(const std::filesystem::path& path) {
        auto image = Image{};

        auto imageFileData = readFileToBytes(path);
        auto imageFileChannels = 0;

        auto width = 0;
        auto height = 0;

        auto pixelData = stbi_load_from_memory(
            imageFileData.data(),
            static_cast<int>(imageFileData.size()),
            &width,
            &height,
            &imageFileChannels,
            ChannelCount
        );

        if (pixelData == nullptr) {
            throw std::runtime_error(stbi_failure_reason());
        }

        auto pixelDataSize = static_cast<usize>(width) * height * ChannelCount;
        auto pixelDataEnd = pixelData + pixelDataSize;

        image._pixels.resize(pixelDataSize);
        std::copy(pixelData, pixelDataEnd, image._pixels.begin());

        stbi_image_free(pixelData);

        image._size.x = width;
        image._size.y = height;

        return image;
    }

    Image Image::create(u32 width, u32 height, Color8 color) {
        const auto pixelDataSize = static_cast<usize>(width) * height * ChannelCount;

        auto image = Image{};
        image._pixels.resize(pixelDataSize);
        image._size = Vector2u{ width, height };
        
        for (usize pixelStartIndex = 0; pixelStartIndex < pixelDataSize; pixelStartIndex += ChannelCount) {
            image._pixels[pixelStartIndex] = color.components[0];
            image._pixels[pixelStartIndex + 1] = color.components[1];
            image._pixels[pixelStartIndex + 2] = color.components[2];
            image._pixels[pixelStartIndex + 3] = color.components[3];
        }

        return image;
    }

    void Image::setPixel(usize x, usize y, Color8 color) {
        const auto pixelByteOffset = y * _size.x * ChannelCount + x * ChannelCount;
        _pixels[pixelByteOffset] = color.components[0];
        _pixels[pixelByteOffset + 1] = color.components[1];
        _pixels[pixelByteOffset + 2] = color.components[2];
        _pixels[pixelByteOffset + 3] = color.components[3];
    }

    const std::byte* Image::getPixels() const {
        return reinterpret_cast<const std::byte*>(_pixels.data());
    }

    Vector2u Image::getSize() const {
        return _size;
    }
}
