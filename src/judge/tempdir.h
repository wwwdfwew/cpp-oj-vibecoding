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

    // 递归删除指定路径。
    static void rm_rf(const std::string& path);

    // 将完整内容写入文件;必要时自动创建父目录。
    static void write_file(const std::string& path, const std::string& content);

    // 读取整个文件的内容。
    static std::string read_file(const std::string& path);

private:
    std::string path_;
    bool valid_ = false;
};

}  // namespace oj::judge

#endif
