#include "protectedassetcontainer.h"

#include "protectedassetformat.h"

#include <physfs.h>

#if OTCLIENT_PROTECTED_ASSETS
#include <ProtectedAssetsConfig.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string_view>
#endif

namespace {

#if OTCLIENT_PROTECTED_ASSETS
using ProtectedAssets::Kind;

struct PhysFsCloser {
    void operator()(PHYSFS_File* file) const {
        if (file)
            PHYSFS_close(file);
    }
};

using PhysFsFile = std::unique_ptr<PHYSFS_File, PhysFsCloser>;

std::string lowerFileName(const std::string& path) {
    const auto slash = path.find_last_of('/');
    auto name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    for (auto& c : name)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return name;
}

std::optional<Kind> assetKind(const std::string& path) {
    const auto name = lowerFileName(path);
    if (name == "tibia.dat") return Kind::Dat;
    if (name == "tibia.spr") return Kind::Spr;
    return std::nullopt;
}

std::string containerPath(const std::string& path) {
    const auto slash = path.find_last_of('/');
    return (slash == std::string::npos ? std::string{} : path.substr(0, slash + 1)) + "assets.sec";
}

void readExact(PHYSFS_File* file, void* output, const uint64_t size) {
    if (PHYSFS_readBytes(file, output, size) != static_cast<PHYSFS_sint64>(size))
        throw std::runtime_error("truncated protected-assets container");
}

uint32_t readU32(PHYSFS_File* file) {
    std::array<uint8_t, 4> bytes{};
    readExact(file, bytes.data(), bytes.size());
    return ProtectedAssets::readU32(bytes.data());
}

void skipAsset(PHYSFS_File* file, const uint32_t chunks, const uint32_t chunkSize) {
    for (uint32_t index = 0; index < chunks; ++index) {
        const auto current = static_cast<uint64_t>(PHYSFS_tell(file));
        std::array<uint8_t, ProtectedAssets::NonceSize> nonce{};
        readExact(file, nonce.data(), nonce.size());
        const auto cipherSize = readU32(file);
        if (cipherSize == 0 || cipherSize > chunkSize)
            throw std::runtime_error("invalid encrypted chunk size");
        if (!PHYSFS_seek(file, current + nonce.size() + 4ULL + cipherSize + ProtectedAssets::TagSize))
            throw std::runtime_error("invalid protected-assets chunk offset");
    }
}

std::vector<uint8_t> decryptAsset(PHYSFS_File* file, const ProtectedAssets::Header& header,
                                  const std::vector<uint8_t>& encodedHeader, const Kind kind) {
    const uint64_t assetSize = kind == Kind::Dat ? header.datSize : header.sprSize;
    const uint32_t chunks = kind == Kind::Dat ? header.datChunks : header.sprChunks;
    const auto key = ProtectedAssets::parseKey(OTCLIENT_ASSET_KEY_HEX);

    if (kind == Kind::Spr)
        skipAsset(file, header.datChunks, header.chunkSize);

    std::vector<uint8_t> output;
    output.reserve(static_cast<size_t>(assetSize));

    for (uint32_t index = 0; index < chunks; ++index) {
        std::array<uint8_t, ProtectedAssets::NonceSize> nonce{};
        readExact(file, nonce.data(), nonce.size());
        const uint32_t cipherSize = readU32(file);
        const uint64_t remaining = assetSize - output.size();
        const uint32_t expectedSize = static_cast<uint32_t>(std::min<uint64_t>(header.chunkSize, remaining));
        if (cipherSize != expectedSize)
            throw std::runtime_error("invalid encrypted asset chunk length");

        std::vector<uint8_t> cipher(cipherSize);
        std::array<uint8_t, ProtectedAssets::TagSize> tag{};
        readExact(file, cipher.data(), cipher.size());
        readExact(file, tag.data(), tag.size());

        const auto aad = ProtectedAssets::makeAad(encodedHeader, kind, index, expectedSize);
        std::vector<uint8_t> plain(cipherSize);
        std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
        if (!context)
            throw std::runtime_error("unable to allocate protected-assets cipher context");

        int written = 0;
        int finalWritten = 0;
        if (EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
            EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN, nonce.size(), nullptr) != 1 ||
            EVP_DecryptInit_ex(context.get(), nullptr, nullptr, key.data(), nonce.data()) != 1 ||
            EVP_DecryptUpdate(context.get(), nullptr, &written, aad.data(), static_cast<int>(aad.size())) != 1 ||
            EVP_DecryptUpdate(context.get(), plain.data(), &written, cipher.data(), static_cast<int>(cipher.size())) != 1 ||
            EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1 ||
            EVP_DecryptFinal_ex(context.get(), plain.data() + written, &finalWritten) != 1)
            throw std::runtime_error("protected-assets authentication failed");

        plain.resize(static_cast<size_t>(written + finalWritten));
        output.insert(output.end(), plain.begin(), plain.end());
    }

    if (output.size() != assetSize)
        throw std::runtime_error("protected-assets size mismatch");
    return output;
}
#endif

} // namespace

bool ProtectedAssetContainer::contains(const std::string& virtualPath) {
#if OTCLIENT_PROTECTED_ASSETS
    return assetKind(virtualPath).has_value() && PHYSFS_exists(containerPath(virtualPath).c_str());
#else
    (void)virtualPath;
    return false;
#endif
}

std::optional<std::vector<uint8_t>> ProtectedAssetContainer::read(const std::string& virtualPath) {
#if OTCLIENT_PROTECTED_ASSETS
    const auto kind = assetKind(virtualPath);
    if (!kind.has_value())
        return std::nullopt;

    PhysFsFile file(PHYSFS_openRead(containerPath(virtualPath).c_str()));
    if (!file)
        return std::nullopt;

    std::vector<uint8_t> encodedHeader(ProtectedAssets::HeaderSize);
    readExact(file.get(), encodedHeader.data(), encodedHeader.size());
    const auto header = ProtectedAssets::decodeHeader(encodedHeader.data(), encodedHeader.size());
    return decryptAsset(file.get(), header, encodedHeader, *kind);
#else
    (void)virtualPath;
    return std::nullopt;
#endif
}
