#include "judge/tempdir.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "util/log.h"

namespace oj::judge {

namespace fs = std::filesystem;

TempDir::TempDir() {
    char tmpl[] = "/tmp/oj_XXXXXX";
    char* p = ::mkdtemp(tmpl);
    if (!p) {
        OJ_ERROR("mkdtemp failed: " << std::strerror(errno));
        throw std::runtime_error("mkdtemp failed");
    }
    path_ = p;
    valid_ = true;
    // 将权限限制为仅所有者可访问(0700)。
    fs::permissions(path_, fs::perms::owner_all,
                    fs::perm_options::replace);
}

TempDir::~TempDir() {
    if (valid_) {
        std::error_code ec;
        fs::remove_all(path_, ec);
        if (ec) OJ_WARN("rm_rf " << path_ << " failed: " << ec.message());
    }
}

void TempDir::rm_rf(const std::string& path) {
    std::error_code ec;
    fs::remove_all(path, ec);
    if (ec) OJ_WARN("rm_rf " << path << " failed: " << ec.message());
}

void TempDir::write_file(const std::string& path, const std::string& content) {
    fs::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) throw std::runtime_error("write_file: open " + path + " failed");
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!ofs) throw std::runtime_error("write_file: write " + path + " failed");
}

std::string TempDir::read_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

}  // namespace oj::judge
