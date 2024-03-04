#include "File.h"

#include <fstream>
#include <stdexcept>
#include <format>

namespace Pandora {

    std::vector<u8> readFileToBytes(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error(std::format("File {} does not exist", path.string()));
        }

        const auto fileSize = std::filesystem::file_size(path);
        auto fileContent = std::vector<u8>(fileSize);

        auto file = std::ifstream{ path, std::ios::binary };
        file.read(reinterpret_cast<char*>(fileContent.data()), fileSize);

        return fileContent;
    }

    std::string readFileToString(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error(std::format("File {} does not exist", path.string()));
        }

        auto file = std::ifstream{ path };
        return std::string{
            std::istreambuf_iterator<char>{ file },
            std::istreambuf_iterator<char>{}
        };
    }
}
