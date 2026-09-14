#ifndef CPP_OJ_VIBECODING_AUTH_PASSWORD_H
#define CPP_OJ_VIBECODING_AUTH_PASSWORD_H

#include <string>

namespace oj::auth {

// SPEC §2.3: sha256(salt || password)
std::string sha256_hex(const std::string& data);
std::string random_salt_hex(size_t bytes = 16);

std::string hash_password(const std::string& salt_hex, const std::string& password);
bool verify_password(const std::string& salt_hex,
                     const std::string& password,
                     const std::string& expected_hash_hex);

}  // namespace oj::auth

#endif
