#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Engine {

struct FileDialogFilter {
    std::string name;
    std::vector<std::string> patterns;
};

struct FileDialogOptions {
    std::string title{"Open File"};
    std::filesystem::path initialDirectory;
    std::vector<FileDialogFilter> filters;
};

class FileDialog {
public:
    [[nodiscard]] static std::optional<std::filesystem::path> openFile(const FileDialogOptions& options = {});
};

} // namespace Engine
