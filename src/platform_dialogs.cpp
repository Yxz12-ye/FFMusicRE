#include "platform_dialogs.h"

#ifdef _WIN32

#include <shobjidl.h>
#include <windows.h>

#include <iterator>

namespace platform_dialogs {
namespace {

class ScopedComApartment {
public:
    ScopedComApartment()
        : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))
    {
    }

    ~ScopedComApartment()
    {
        if (result_ == S_OK || result_ == S_FALSE) {
            CoUninitialize();
        }
    }

    auto ready() const -> bool
    {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_;
};

auto shell_item_to_path(IShellItem *item) -> std::optional<std::filesystem::path>
{
    PWSTR raw_path = nullptr;
    const HRESULT result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path);
    if (FAILED(result) || raw_path == nullptr) {
        return std::nullopt;
    }

    std::filesystem::path path(raw_path);
    CoTaskMemFree(raw_path);
    return path;
}

} // namespace

auto pick_audio_files() -> std::vector<std::filesystem::path>
{
    std::vector<std::filesystem::path> paths;
    ScopedComApartment apartment;
    if (!apartment.ready()) {
        return paths;
    }

    IFileOpenDialog *dialog = nullptr;
    if (FAILED(CoCreateInstance(
            CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return paths;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(
        options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT);

    const COMDLG_FILTERSPEC filters[] = {
        { L"Audio Files", L"*.mp3;*.flac;*.ogg;*.wav;*.aif;*.aiff" },
        { L"All Files", L"*.*" },
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dialog->SetDefaultExtension(L"mp3");
    dialog->SetTitle(L"Select Audio Files");

    const HRESULT show_result = dialog->Show(nullptr);
    if (show_result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        dialog->Release();
        return paths;
    }

    IShellItemArray *items = nullptr;
    if (FAILED(show_result) || FAILED(dialog->GetResults(&items)) || items == nullptr) {
        dialog->Release();
        return paths;
    }

    DWORD count = 0;
    items->GetCount(&count);

    for (DWORD index = 0; index < count; ++index) {
        IShellItem *item = nullptr;
        if (FAILED(items->GetItemAt(index, &item)) || item == nullptr) {
            continue;
        }

        if (const auto path = shell_item_to_path(item)) {
            paths.push_back(*path);
        }
        item->Release();
    }

    items->Release();
    dialog->Release();
    return paths;
}

auto pick_folder() -> std::optional<std::filesystem::path>
{
    ScopedComApartment apartment;
    if (!apartment.ready()) {
        return std::nullopt;
    }

    IFileOpenDialog *dialog = nullptr;
    if (FAILED(CoCreateInstance(
            CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return std::nullopt;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_PICKFOLDERS);
    dialog->SetTitle(L"Select Music Folder");

    const HRESULT show_result = dialog->Show(nullptr);
    if (show_result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        dialog->Release();
        return std::nullopt;
    }

    IShellItem *item = nullptr;
    if (FAILED(show_result) || FAILED(dialog->GetResult(&item)) || item == nullptr) {
        dialog->Release();
        return std::nullopt;
    }

    const auto path = shell_item_to_path(item);
    item->Release();
    dialog->Release();
    return path;
}

} // namespace platform_dialogs

#else

namespace platform_dialogs {

auto pick_audio_files() -> std::vector<std::filesystem::path>
{
    return {};
}

auto pick_folder() -> std::optional<std::filesystem::path>
{
    return std::nullopt;
}

} // namespace platform_dialogs

#endif
