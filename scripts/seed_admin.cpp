// 对应 SPEC §3 Phase 1.5 —— 用 sha256(salt || password) 种子管理员账号
// 用法:seed_admin <username> <password> <role>
// 输出:SQL INSERT 语句(写到 stdout)
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string sha256_hex(const std::string& data) {
    unsigned char digest[32];
    unsigned int len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);
    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string random_salt_hex(size_t bytes = 16) {
    std::vector<unsigned char> buf(bytes);
    if (RAND_bytes(buf.data(), static_cast<int>(bytes)) != 1) {
        std::cerr << "RAND_bytes failed\n";
        std::exit(1);
    }
    std::ostringstream oss;
    for (auto b : buf) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
    }
    return oss.str();
}

std::string mysql_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\0': out += "\\0"; break;
            default:   out += c;
        }
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <user|admin>\n";
        return 2;
    }
    std::string username = argv[1];
    std::string password = argv[2];
    std::string role = argv[3];

    if (role != "user" && role != "admin") {
        std::cerr << "role must be 'user' or 'admin'\n";
        return 2;
    }

    std::string salt = random_salt_hex();
    std::string hash = sha256_hex(salt + password);

    std::cout << "INSERT INTO users (username, password_hash, salt, role) VALUES ("
              << "'" << mysql_escape(username) << "', "
              << "'" << mysql_escape(hash) << "', "
              << "'" << mysql_escape(salt) << "', "
              << "'" << role << "') "
              << "ON DUPLICATE KEY UPDATE "
              << "password_hash=VALUES(password_hash), "
              << "salt=VALUES(salt), "
              << "role=VALUES(role);";
    return 0;
}
