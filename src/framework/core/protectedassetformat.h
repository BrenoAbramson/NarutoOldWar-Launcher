#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ProtectedAssets {

inline constexpr std::array<uint8_t, 8> Magic{ 'O', 'T', 'C', 'S', 'E', 'C', '0', '1' };
inline constexpr uint32_t Version = 1;
inline constexpr uint32_t HeaderSize = 40;
inline constexpr uint32_t NonceSize = 12;
inline constexpr uint32_t TagSize = 16;
inline constexpr uint32_t DefaultChunkSize = 1024 * 1024;
inline constexpr uint64_t MaximumAssetSize = 1024ULL * 1024ULL * 1024ULL;

enum class Kind : uint8_t { Dat = 1, Spr = 2 };

struct Header {
    uint32_t version{};
    uint32_t chunkSize{};
    uint64_t datSize{};
    uint64_t sprSize{};
    uint32_t datChunks{};
    uint32_t sprChunks{};
};

inline uint32_t readU32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

inline uint64_t readU64(const uint8_t* data) {
    return static_cast<uint64_t>(readU32(data)) |
           (static_cast<uint64_t>(readU32(data + 4)) << 32);
}

inline void appendU32(std::vector<uint8_t>& output, const uint32_t value) {
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8));
    output.push_back(static_cast<uint8_t>(value >> 16));
    output.push_back(static_cast<uint8_t>(value >> 24));
}

inline void appendU64(std::vector<uint8_t>& output, const uint64_t value) {
    appendU32(output, static_cast<uint32_t>(value));
    appendU32(output, static_cast<uint32_t>(value >> 32));
}

inline uint32_t chunkCount(const uint64_t size, const uint32_t chunkSize) {
    return size == 0 ? 0 : static_cast<uint32_t>((size + chunkSize - 1) / chunkSize);
}

inline std::vector<uint8_t> encodeHeader(const Header& header) {
    std::vector<uint8_t> output;
    output.reserve(HeaderSize);
    output.insert(output.end(), Magic.begin(), Magic.end());
    appendU32(output, header.version);
    appendU32(output, header.chunkSize);
    appendU64(output, header.datSize);
    appendU64(output, header.sprSize);
    appendU32(output, header.datChunks);
    appendU32(output, header.sprChunks);
    return output;
}

inline Header decodeHeader(const uint8_t* data, const size_t size) {
    if (size < HeaderSize || std::memcmp(data, Magic.data(), Magic.size()) != 0)
        throw std::runtime_error("invalid protected-assets header");

    Header header;
    header.version = readU32(data + 8);
    header.chunkSize = readU32(data + 12);
    header.datSize = readU64(data + 16);
    header.sprSize = readU64(data + 24);
    header.datChunks = readU32(data + 32);
    header.sprChunks = readU32(data + 36);

    if (header.version != Version || header.chunkSize < 4096 || header.chunkSize > 16 * 1024 * 1024)
        throw std::runtime_error("unsupported protected-assets format");
    if (header.datSize == 0 || header.sprSize == 0 || header.datSize > MaximumAssetSize || header.sprSize > MaximumAssetSize)
        throw std::runtime_error("invalid protected-assets sizes");
    if (header.datChunks != chunkCount(header.datSize, header.chunkSize) ||
        header.sprChunks != chunkCount(header.sprSize, header.chunkSize))
        throw std::runtime_error("invalid protected-assets chunk table");
    return header;
}

inline std::vector<uint8_t> makeAad(const std::vector<uint8_t>& encodedHeader, const Kind kind,
                                    const uint32_t index, const uint32_t plainSize) {
    auto aad = encodedHeader;
    aad.push_back(static_cast<uint8_t>(kind));
    appendU32(aad, index);
    appendU32(aad, plainSize);
    return aad;
}

inline std::array<uint8_t, 32> parseKey(const std::string_view keyHex) {
    if (keyHex.size() != 64)
        throw std::runtime_error("asset key must contain 64 hexadecimal characters");
    auto nibble = [](const char c) -> uint8_t {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        throw std::runtime_error("asset key contains a non-hexadecimal character");
    };
    std::array<uint8_t, 32> key{};
    for (size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<uint8_t>((nibble(keyHex[i * 2]) << 4) | nibble(keyHex[i * 2 + 1]));
    return key;
}

} // namespace ProtectedAssets
