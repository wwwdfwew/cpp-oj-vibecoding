#include "auth/password.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace oj::auth {

std::string sha256_hex(const std::string& data) {
    unsigned char digest[32];
    unsigned int len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("sha256 failed");
    }
    EVP_MD_CTX_free(ctx);
    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string random_salt_hex(size_t bytes) {
    std::vector<unsigned char> buf(bytes);
    if (RAND_bytes(buf.data(), static_cast<int>(bytes)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    std::ostringstream oss;
    for (auto b : buf) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
    }
    return oss.str();
}

std::string hash_password(const std::string& salt_hex, const std::string& password) {
    return sha256_hex(salt_hex + password);
}

bool verify_password(const std::string& salt_hex,
                     const std::string& password,
                     const std::string& expected_hash_hex) {
    return hash_password(salt_hex, password) == expected_hash_hex;
}

}  // namespace oj::auth
