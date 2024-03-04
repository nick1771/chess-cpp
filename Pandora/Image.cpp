#include "Image.h"
#include "File.h"

#include "Vendor/Stb/stb_image.h"

namespace Pandora {

    constexpr u32 CHANNEL_COUNT = 4;

    Image Image::load(const std::filesystem::path& path) {
        auto image = Image{};

        auto imageFileData = readFileToBytes(path);
        auto imageFileChannels = 0;

        auto width = 0;
        auto height = 0;

        auto pixelData = stbi_load_from_memory(
            imageFileData.data(),
            imageFileData.size(),
            &width,
            &height,
            &imageFileChannels,
            CHANNEL_COUNT
        );

        if (pixelData == nullptr) {
            throw std::runtime_error(stbi_failure_reason());
        }

        auto pixelDataSize = static_cast<usize>(width) * height * CHANNEL_COUNT;
        auto pixelDataEnd = pixelData + pixelDataSize;

        image._pixels.resize(pixelDataSize);
        std::copy(pixelData, pixelDataEnd, image._pixels.begin());

        stbi_image_free(pixelData);

        image._size.x = width;
        image._size.y = height;

        return image;
    }

    Image Image::create(u32 width, u32 height, Color8 color) {
        const auto pixelDataSize = static_cast<usize>(width) * height * CHANNEL_COUNT;

        auto image = Image{};
        image._pixels.resize(pixelDataSize);
        image._size = Vector2u{ width, height };
        
        for (usize pixelStartIndex = 0; pixelStartIndex < pixelDataSize; pixelStartIndex += CHANNEL_COUNT) {
            image._pixels[pixelStartIndex] = color.components[0];
            image._pixels[pixelStartIndex + 1] = color.components[1];
            image._pixels[pixelStartIndex + 2] = color.components[2];
            image._pixels[pixelStartIndex + 3] = color.components[3];
        }

        return image;
    }

    const u8* Image::getPixels() const {
        return _pixels.data();
    }

    Vector2u Image::getSize() const {
        return _size;
    }
}
