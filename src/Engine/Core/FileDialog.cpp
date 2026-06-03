#include "Engine/Core/FileDialog.hpp"

#include <array>
#include <cstdio>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shobjidl.h>
#include <windows.h>
#endif

namespace Engine {

namespace {

#if defined(_WIN32)
std::wstring widen(const std::string& text)
{
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::wstring filterPattern(const FileDialogFilter& filter)
{
    if (filter.patterns.empty()) {
        return L"*.*";
    }

    std::wstring joined;
    for (std::size_t index = 0; index < filter.patterns.size(); ++index) {
        if (index > 0) {
            joined += L";";
        }
        joined += widen(filter.patterns[index]);
    }
    return joined;
}
#else
std::string shellQuote(const std::string& text)
{
    std::string quoted = "'";
    for (const char character : text) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(character);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

std::optional<std::filesystem::path> runPickerCommand(const std::string& command)
{
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return std::nullopt;
    }

    std::array<char, 4096> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int result = pclose(pipe);
    if (result != 0 || output.empty()) {
        return std::nullopt;
    }

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    if (output.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path(output);
}
#endif

} // namespace

std::optional<std::filesystem::path> FileDialog::openFile(const FileDialogOptions& options)
{
#if defined(_WIN32)
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool initializedCom = SUCCEEDED(initResult);
    if (initResult == RPC_E_CHANGED_MODE) {
        initResult = S_OK;
    }
    if (FAILED(initResult)) {
        return std::nullopt;
    }

    IFileOpenDialog* dialog = nullptr;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
    if (FAILED(result)) {
        if (initializedCom) {
            CoUninitialize();
        }
        return std::nullopt;
    }

    const std::wstring title = widen(options.title);
    if (!title.empty()) {
        dialog->SetTitle(title.c_str());
    }

    std::vector<std::wstring> filterNames;
    std::vector<std::wstring> filterPatterns;
    std::vector<COMDLG_FILTERSPEC> filterSpecs;
    filterNames.reserve(options.filters.size());
    filterPatterns.reserve(options.filters.size());
    filterSpecs.reserve(options.filters.size());
    for (const FileDialogFilter& filter : options.filters) {
        filterNames.push_back(widen(filter.name));
        filterPatterns.push_back(filterPattern(filter));
        filterSpecs.push_back({filterNames.back().c_str(), filterPatterns.back().c_str()});
    }
    if (!filterSpecs.empty()) {
        dialog->SetFileTypes(static_cast<UINT>(filterSpecs.size()), filterSpecs.data());
    }

    IShellItem* folder = nullptr;
    const std::filesystem::path initialDirectory = options.initialDirectory.empty()
        ? std::filesystem::current_path()
        : options.initialDirectory;
    const std::wstring initialPath = initialDirectory.wstring();
    if (SUCCEEDED(SHCreateItemFromParsingName(initialPath.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
        dialog->SetFolder(folder);
        folder->Release();
    }

    std::optional<std::filesystem::path> selectedPath;
    result = dialog->Show(nullptr);
    if (SUCCEEDED(result)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                selectedPath = std::filesystem::path(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }

    dialog->Release();
    if (initializedCom) {
        CoUninitialize();
    }
    return selectedPath;
#elif defined(__APPLE__)
    std::ostringstream command;
    command << "osascript -e " << shellQuote("POSIX path of (choose file with prompt \"" + options.title + "\")");
    return runPickerCommand(command.str());
#else
    const std::string title = shellQuote(options.title);
    if (auto path = runPickerCommand("zenity --file-selection --title=" + title + " 2>/dev/null")) {
        return path;
    }
    if (auto path = runPickerCommand("kdialog --getopenfilename . --title " + title + " 2>/dev/null")) {
        return path;
    }
    return std::nullopt;
#endif
}

} // namespace Engine
