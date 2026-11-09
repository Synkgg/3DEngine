#include "FileDialog.h"

#include <Windows.h>
#include <shobjidl.h>

#include <string>
#include <filesystem>

namespace
{
    std::string WideToUTF8(const wchar_t* text)
    {
        if (text == nullptr)
        {
            return {};
        }

        int size = WideCharToMultiByte(
            CP_UTF8,
            0,
            text,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr
        );

        if (size <= 1)
        {
            return {};
        }

        std::string result(
            static_cast<size_t>(size - 1),
            '\0'
        );

        WideCharToMultiByte(
            CP_UTF8,
            0,
            text,
            -1,
            result.data(),
            size,
            nullptr,
            nullptr
        );

        return result;
    }

    bool InitializeCOM(bool& shouldUninitialize)
    {
        const HRESULT result =
            CoInitializeEx(
                nullptr,
                COINIT_APARTMENTTHREADED |
                COINIT_DISABLE_OLE1DDE
            );

        shouldUninitialize = SUCCEEDED(result);

        // The editor may already have COM initialized in another apartment
        // model. File dialogs are still available in that case; only skip the
        // matching CoUninitialize because this call did not initialize COM.
        return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;
    }

    void ShutdownCOM(bool shouldUninitialize)
    {
        if (shouldUninitialize)
        {
            CoUninitialize();
        }
    }
}

namespace FileDialog
{
    bool OpenScene(std::string& path)
    {
        bool shouldUninitialize = false;
        if (!InitializeCOM(shouldUninitialize))
        {
            return false;
        }

        IFileOpenDialog* dialog = nullptr;

        HRESULT result =
            CoCreateInstance(
                CLSID_FileOpenDialog,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&dialog)
            );

        if (FAILED(result))
        {
            ShutdownCOM(shouldUninitialize);
            return false;
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            {
                L"Scene Files (*.scene)",
                L"*.scene"
            },
            {
                L"All Files (*.*)",
                L"*.*"
            }
        };

        dialog->SetFileTypes(
            2,
            filters
        );

        dialog->SetTitle(
            L"Open Scene"
        );

        result = dialog->Show(nullptr);

        if (FAILED(result))
        {
            dialog->Release();
            ShutdownCOM(shouldUninitialize);
            return false;
        }

        IShellItem* item = nullptr;

        result =
            dialog->GetResult(&item);

        if (FAILED(result))
        {
            dialog->Release();
            ShutdownCOM(shouldUninitialize);
            return false;
        }

        PWSTR filePath = nullptr;

        result =
            item->GetDisplayName(
                SIGDN_FILESYSPATH,
                &filePath
            );

        if (SUCCEEDED(result))
        {
            path = WideToUTF8(filePath);

            CoTaskMemFree(filePath);
        }

        item->Release();
        dialog->Release();

        ShutdownCOM(shouldUninitialize);

        return !path.empty();
    }

    bool SaveScene(std::string& path)
    {
        bool shouldUninitialize = false;
        if (!InitializeCOM(shouldUninitialize))
        {
            return false;
        }

        IFileSaveDialog* dialog = nullptr;

        HRESULT result =
            CoCreateInstance(
                CLSID_FileSaveDialog,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&dialog)
            );

        if (FAILED(result))
        {
            ShutdownCOM(shouldUninitialize);
            return false;
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            {
                L"Scene Files (*.scene)",
                L"*.scene"
            },
            {
                L"All Files (*.*)",
                L"*.*"
            }
        };

        dialog->SetFileTypes(
            2,
            filters
        );

        dialog->SetDefaultExtension(
            L"scene"
        );

        dialog->SetFileName(
            L"scene.scene"
        );

        dialog->SetTitle(
            L"Save Scene"
        );

        dialog->SetOptions(
            FOS_OVERWRITEPROMPT |
            FOS_PATHMUSTEXIST
        );

        result = dialog->Show(nullptr);

        if (FAILED(result))
        {
            dialog->Release();
            ShutdownCOM(shouldUninitialize);
            return false;
        }

        IShellItem* item = nullptr;

        result =
            dialog->GetResult(&item);

        if (FAILED(result))
        {
            dialog->Release();
            ShutdownCOM(shouldUninitialize);
            return false;
        }

        PWSTR filePath = nullptr;

        result =
            item->GetDisplayName(
                SIGDN_FILESYSPATH,
                &filePath
            );

        if (SUCCEEDED(result))
        {
            path = WideToUTF8(filePath);

            CoTaskMemFree(filePath);
        }

        item->Release();
        dialog->Release();

        ShutdownCOM(shouldUninitialize);

        return !path.empty();
    }

    bool OpenProject(std::string& path)
    {
        bool shouldUninitialize = false;
        if (!InitializeCOM(shouldUninitialize)) return false;

        IFileOpenDialog* dialog = nullptr;
        HRESULT result = CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));

        if (FAILED(result))
        {
            ShutdownCOM(shouldUninitialize);
            return false;
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            { L"Engine Project (*.project)", L"*.project" },
            { L"All Files", L"*.*" }
        };

        dialog->SetFileTypes(2, filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetTitle(L"Open Project");

        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
        {
            dialog->SetOptions(
                options |
                FOS_FORCEFILESYSTEM |
                FOS_PATHMUSTEXIST |
                FOS_FILEMUSTEXIST |
                FOS_NOCHANGEDIR);
        }

        // Start in the nearest Projects folder instead of whatever location
        // Windows happened to remember for this dialog. This works both when
        // running from the repository and from out/build/<configuration>.
        std::filesystem::path cursor = std::filesystem::current_path();
        std::filesystem::path projectsDirectory;

        for (int depth = 0; depth < 8 && !cursor.empty(); ++depth)
        {
            const std::filesystem::path candidate = cursor / "Projects";
            std::error_code error;
            if (std::filesystem::is_directory(candidate, error))
            {
                projectsDirectory = candidate;
                break;
            }

            const std::filesystem::path parent = cursor.parent_path();
            if (parent == cursor) break;
            cursor = parent;
        }

        if (!projectsDirectory.empty())
        {
            IShellItem* projectsItem = nullptr;
            const std::wstring widePath = projectsDirectory.wstring();
            if (SUCCEEDED(SHCreateItemFromParsingName(
                    widePath.c_str(),
                    nullptr,
                    IID_PPV_ARGS(&projectsItem))))
            {
                // SetFolder controls the folder shown when the dialog opens.
                // SetDefaultFolder gives Windows a fallback without preventing
                // the user from navigating anywhere else on disk.
                dialog->SetDefaultFolder(projectsItem);
                dialog->SetFolder(projectsItem);
                projectsItem->Release();
            }
        }

        result = dialog->Show(nullptr);
        if (SUCCEEDED(result))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)))
            {
                PWSTR filePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &filePath)))
                {
                    path = WideToUTF8(filePath);
                    CoTaskMemFree(filePath);
                }
                item->Release();
            }
        }

        dialog->Release();
        ShutdownCOM(shouldUninitialize);
        return !path.empty();
    }

    bool SelectFolder(std::string& path, const wchar_t* title)
    {
        bool shouldUninitialize = false;
        if (!InitializeCOM(shouldUninitialize)) return false;
        IFileOpenDialog* dialog = nullptr;
        HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(result)) { ShutdownCOM(shouldUninitialize); return false; }
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
        dialog->SetTitle(title);
        result = dialog->Show(nullptr);
        if (SUCCEEDED(result))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)))
            {
                PWSTR folderPath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &folderPath)))
                {
                    path = WideToUTF8(folderPath);
                    CoTaskMemFree(folderPath);
                }
                item->Release();
            }
        }
        dialog->Release();
        ShutdownCOM(shouldUninitialize);
        return !path.empty();
    }
}
