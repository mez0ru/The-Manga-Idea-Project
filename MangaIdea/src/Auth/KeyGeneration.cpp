#include "KeyGeneration.h"

const std::string CalcHmacSHA512(const std::string_view& decodedKey, const std::string_view& msg)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
    unsigned int hashLen;

    HMAC(
        EVP_sha512(),
        decodedKey.data(),
        static_cast<int>(decodedKey.size()),
        reinterpret_cast<unsigned char const*>(msg.data()),
        static_cast<int>(msg.size()),
        hash.data(),
        &hashLen
    );

    std::stringstream ss;
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

const std::string Random64Hex()
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> hex;

    RAND_bytes(hex.data(), hex.size());

    std::stringstream ss;
    for (int i = 0; i < EVP_MAX_MD_SIZE; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hex[i];
    }

    return ss.str();
}