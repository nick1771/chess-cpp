#pragma once

#include "Pandora.h"

#include <filesystem>
#include <vector>
#include <string>

namespace Pandora {

    std::vector<u8> readFileToBytes(const std::filesystem::path& path);

    std::string readFileToString(const std::filesystem::path& path);
}
