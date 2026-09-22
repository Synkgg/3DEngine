#include "FileDialog.h"

#include <Windows.h>
#include <shobjidl.h>

#include <string>

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

    bool InitializeCOM()
    {
        HRESULT result =
            CoInitializeEx(
                nullptr,
                COINIT_APARTMENTTHREADED |
                COINIT_DISABLE_OLE1DDE
            );

        return SUCCEEDED(result);
    }

    void ShutdownCOM()
    {
        CoUninitialize();
    }
}

namespace FileDialog
{
    bool OpenScene(std::string& path)
    {
        if (!InitializeCOM())
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
            ShutdownCOM();
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
            ShutdownCOM();
            return false;
        }

        IShellItem* item = nullptr;

        result =
            dialog->GetResult(&item);

        if (FAILED(result))
        {
            dialog->Release();
            ShutdownCOM();
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

        ShutdownCOM();

        return !path.empty();
    }

    bool SaveScene(std::string& path)
    {
        if (!InitializeCOM())
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
            ShutdownCOM();
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
            ShutdownCOM();
            return false;
        }

        IShellItem* item = nullptr;

        result =
            dialog->GetResult(&item);

        if (FAILED(result))
        {
            dialog->Release();
            ShutdownCOM();
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

        ShutdownCOM();

        return !path.empty();
    }
}