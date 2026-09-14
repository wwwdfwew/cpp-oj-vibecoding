#ifndef CPP_OJ_VIBECODING_JUDGE_TEMPDIR_H
#define CPP_OJ_VIBECODING_JUDGE_TEMPDIR_H

#include <string>

namespace oj::judge {

class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::string& path() const { return path_; }
    explicit operator bool() const { return valid_; }

    // Recursively remove a path.
    static void rm_rf(const std::string& path);

    // Write whole content to file; creates parent dirs as needed.
    static void write_file(const std::string& path, const std::string& content);

    // Read whole file content.
    static std::string read_file(const std::string& path);

private:
    std::string path_;
    bool valid_ = false;
};

}  // namespace oj::judge

#endif
