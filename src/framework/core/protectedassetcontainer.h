#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class ProtectedAssetContainer
{
public:
    static bool contains(const std::string& virtualPath);
    static std::optional<std::vector<uint8_t>> read(const std::string& virtualPath);
};
