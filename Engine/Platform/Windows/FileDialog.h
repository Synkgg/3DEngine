#pragma once

#include <string>

namespace FileDialog
{
    bool OpenScene(std::string& path);
    bool SaveScene(std::string& path);
    bool OpenProject(std::string& path);
    bool SelectFolder(std::string& path, const wchar_t* title = L"Choose Project Location");
}