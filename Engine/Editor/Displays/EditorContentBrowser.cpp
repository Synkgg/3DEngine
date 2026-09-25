#include "../Editor.h"
#include "../HierarchyFolder.h"

#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"
#include "../../Core/Logger.h"
#include "../../Scene/Scene.h"
#include "../../Scene/SceneSerializer.h"

#include "../Fonts/IconsFontAwesome6.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#endif

namespace
{
    bool OpenAssetExternally(
        const std::filesystem::path& path)
    {
#ifdef _WIN32
        const std::wstring widePath =
            path.wstring();

        HINSTANCE result =
            ShellExecuteW(
                nullptr,
                L"open",
                widePath.c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            );

        if (reinterpret_cast<INT_PTR>(result) <= 32)
        {
            Logger::Error(
                std::string(
                    "Failed to open asset: "
                ) +
                path.string()
            );

            return false;
        }

        return true;
#else
        Logger::Warning(
            "Opening assets externally is currently only supported on Windows."
        );

        return false;
#endif
    }
}

void Editor::RenderContentBrowser(
    Renderer& renderer,
    Scene& scene,
    ImFont* iconFont)
{
    namespace fs = std::filesystem;

    ImGui::Begin(
        "Assets"
    );

    fs::path currentPath =
        m_ContentBrowserPath;

    const fs::path assetsRoot =
        fs::current_path() / "Assets";

    std::error_code error;

    if (!fs::exists(
        currentPath,
        error))
    {
        ImGui::TextDisabled(
            "Assets folder not found."
        );

        ImGui::End();
        return;
    }

    /*
     * Navigation.
     */
    if (currentPath != assetsRoot)
    {
        if (ImGui::Button("<-"))
        {
            currentPath =
                currentPath.parent_path();

            m_ContentBrowserPath =
                currentPath.string();

            m_SelectedAssetPath.clear();
        }

        ImGui::SameLine();
    }

    const fs::path relativePath =
        fs::relative(
            currentPath,
            assetsRoot,
            error
        );

    if (relativePath.empty())
    {
        ImGui::Text(
            "Assets"
        );
    }
    else
    {
        ImGui::Text(
            "Assets / %s",
            relativePath.generic_string().c_str()
        );
    }

    /*
     * Search.
     */
    const float searchWidth =
        200.0f;

    const float clearButtonWidth =
        24.0f;

    const float spacing =
        ImGui::GetStyle().ItemSpacing.x;

    const float totalWidth =
        searchWidth +
        clearButtonWidth +
        spacing;

    const float contentWidth =
        ImGui::GetContentRegionAvail().x;

    if (contentWidth > totalWidth)
    {
        ImGui::SameLine(
            ImGui::GetCursorPosX() +
            contentWidth -
            totalWidth
        );
    }

    ImGui::SetNextItemWidth(
        searchWidth
    );

    ImGui::InputTextWithHint(
        "##ContentBrowserSearch",
        "Search...",
        m_ContentBrowserSearchBuffer,
        sizeof(m_ContentBrowserSearchBuffer)
    );

    ImGui::SameLine();

    if (ImGui::Button(
        "X",
        ImVec2(
            clearButtonWidth,
            0.0f
        )))
    {
        m_ContentBrowserSearchBuffer[0] =
            '\0';
    }

    ImGui::Separator();

    int folderCount = 0;
    int assetCount = 0;
    {
        std::error_code countError;
        for (const fs::directory_entry& entry : fs::directory_iterator(currentPath, countError))
        {
            if (countError) break;
            if (entry.is_directory()) ++folderCount; else ++assetCount;
        }
    }
    ImGui::TextDisabled("%d folders   %d assets", folderCount, assetCount);
    ImGui::SameLine();
    ImGui::TextDisabled("   Drag assets into Details to assign them");
    ImGui::Separator();

    /*
     * Asset entries.
     */
    struct AssetEntry
    {
        fs::path path;
        bool directory;
    };

    std::vector<AssetEntry> entries;

    const std::string searchText =
        m_ContentBrowserSearchBuffer;

    auto ToLower =
        [](std::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(
                        std::tolower(character)
                        );
                }
            );

            return value;
        };

    if (searchText.empty())
    {
        for (const fs::directory_entry& entry :
            fs::directory_iterator(
                currentPath,
                error))
        {
            if (error)
            {
                break;
            }

            entries.push_back(
                {
                    entry.path(),
                    entry.is_directory()
                }
            );
        }
    }
    else
    {
        const std::string lowerSearch =
            ToLower(searchText);

        std::error_code searchError;

        for (const fs::directory_entry& entry :
            fs::recursive_directory_iterator(
                assetsRoot,
                searchError))
        {
            if (searchError)
            {
                break;
            }

            const std::string filename =
                entry.path().filename().string();

            if (ToLower(filename).find(
                lowerSearch
            ) != std::string::npos)
            {
                entries.push_back(
                    {
                        entry.path(),
                        entry.is_directory()
                    }
                );
            }
        }
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](
            const AssetEntry& a,
            const AssetEntry& b)
        {
            if (a.directory !=
                b.directory)
            {
                return a.directory >
                    b.directory;
            }

            return a.path.filename().string() <
                b.path.filename().string();
        }
    );

    /*
     * Grid.
     */
    const float itemWidth =
        104.0f;

    int columnCount =
        static_cast<int>(
            ImGui::GetContentRegionAvail().x /
            itemWidth
            );

    columnCount =
        std::max(
            columnCount,
            1
        );

    if (ImGui::BeginTable(
        "ContentBrowserGrid",
        columnCount,
        ImGuiTableFlags_SizingFixedSame))
    {
        for (const AssetEntry& entry :
            entries)
        {
            ImGui::TableNextColumn();

            const std::string filename =
                entry.path.filename().string();

            const std::string extension =
                ToLower(
                    entry.path.extension().string()
                );

            const bool isImage =
                !entry.directory &&
                (
                    extension == ".png" ||
                    extension == ".jpg" ||
                    extension == ".jpeg" ||
                    extension == ".bmp"
                    );

            const bool isScene =
                !entry.directory &&
                extension == ".scene";

            const bool isScript =
                !entry.directory &&
                extension == ".lua";

            const bool isUI =
                !entry.directory &&
                extension == ".ui";

            const bool isMesh =
                !entry.directory &&
                extension == ".obj";

            ImGui::PushID(
                entry.path.string().c_str()
            );

            const bool selected =
                m_SelectedAssetPath ==
                entry.path.string();

            /*
             * Preview.
             */
            if (isImage)
            {
                Texture2D* texture =
                    renderer.LoadTexture(
                        entry.path.string()
                    );

                if (texture != nullptr &&
                    texture->IsLoaded())
                {
                    ImTextureID textureID =
                        (ImTextureID)(
                            std::intptr_t(
                                texture->GetID()
                            )
                            );

                    ImGui::ImageButton(
                        "##thumbnail",
                        textureID,
                        ImVec2(72.0f, 72.0f),
                        ImVec2(
                            0.0f,
                            1.0f
                        ),
                        ImVec2(
                            1.0f,
                            0.0f
                        )
                    );
                }
                else
                {
                    if (iconFont != nullptr)
                    {
                        ImGui::PushFont(
                            iconFont
                        );
                    }

                    ImGui::Button(
                        ICON_FA_FILE_IMAGE,
                        ImVec2(72.0f, 72.0f)
                    );

                    if (iconFont != nullptr)
                    {
                        ImGui::PopFont();
                    }
                }
            }
            else
            {
                const char* icon =
                    ICON_FA_FILE;

                ImVec4 iconColor =
                    ImVec4(
                        0.7f,
                        0.73f,
                        0.78f,
                        1.0f
                    );

                if (entry.directory)
                {
                    icon =
                        ICON_FA_FOLDER;

                    iconColor =
                        ImVec4(
                            1.0f,
                            0.75f,
                            0.2f,
                            1.0f
                        );
                }
                else if (isScene)
                {
                    icon =
                        ICON_FA_FILE;

                    iconColor =
                        ImVec4(
                            0.3f,
                            0.65f,
                            1.0f,
                            1.0f
                        );
                }
                else if (isMesh)
                {
                    icon = ICON_FA_CUBE;
                    iconColor = ImVec4(0.35f, 0.78f, 0.95f, 1.0f);
                }
                else if (isScript)
                {
                    icon =
                        ICON_FA_FILE_CODE;

                    iconColor =
                        ImVec4(
                            0.75f,
                            0.4f,
                            1.0f,
                            1.0f
                        );
                }

                if (iconFont != nullptr)
                {
                    ImGui::PushFont(
                        iconFont
                    );
                }

                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    iconColor
                );

                ImGui::Button(
                    icon,
                    ImVec2(72.0f, 72.0f)
                );

                ImGui::PopStyleColor();

                if (iconFont != nullptr)
                {
                    ImGui::PopFont();
                }
            }

            /*
             * Select.
             */
            if (ImGui::IsItemClicked(
                ImGuiMouseButton_Left))
            {
                m_SelectedAssetPath =
                    entry.path.string();
            }

            /*
             * Open folder.
             */
            if (entry.directory &&
                ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(
                    ImGuiMouseButton_Left))
            {
                m_ContentBrowserPath =
                    entry.path.string();

                m_SelectedAssetPath.clear();
            }

            /*
             * Open scene.
             */
            if (isScene &&
                ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(
                    ImGuiMouseButton_Left))
            {
                if (m_Playing)
                {
                    StopPlaying();
                }

                m_SelectedEntity =
                    Entity();

                m_NameEditEntityID =
                    0;

                m_NameEditBuffer[0] =
                    '\0';

                SceneSerializer serializer(
                    scene
                );

                if (serializer.Load(
                    entry.path.string(),
                    m_HierarchyFolders
                ))
                {
                    m_SceneFilePath =
                        entry.path.string();

                    m_SelectedAssetPath =
                        entry.path.string();

                    Logger::Info(
                        std::string(
                            "Loaded scene: "
                        ) +
                        entry.path.filename().string()
                    );
                }
                else
                {
                    Logger::Error(
                        std::string(
                            "Failed to load scene: "
                        ) +
                        entry.path.string()
                    );
                }
            }

            /*
             * Open UI asset in the Widget Blueprint editor.
             */
            if (isUI &&
                ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_SelectedAssetPath = entry.path.string();
                m_PendingUIAssetPath = entry.path.string();
            }

            if (isMesh &&
                ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_MeshPreviewPath = fs::relative(entry.path, fs::current_path(), error).generic_string();
            }

            /*
             * Open image/script.
             */
            if (!entry.directory &&
                !isScene &&
                (isImage || isScript) &&
                ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(
                    ImGuiMouseButton_Left))
            {
                OpenAssetExternally(
                    entry.path
                );
            }

            /*
             * Drag/drop asset.
             */
            if (!entry.directory &&
                ImGui::BeginDragDropSource())
            {
                const fs::path relativeAssetPath =
                    fs::relative(
                        entry.path,
                        fs::current_path(),
                        error
                    );

                const std::string assetPath =
                    relativeAssetPath.generic_string();

                ImGui::SetDragDropPayload(
                    "ASSET_FILE",
                    assetPath.c_str(),
                    assetPath.size() + 1
                );

                if (isImage)
                {
                    Texture2D* texture =
                        renderer.LoadTexture(
                            entry.path.string()
                        );

                    if (texture != nullptr &&
                        texture->IsLoaded())
                    {
                        ImTextureID textureID =
                            (ImTextureID)(
                                std::intptr_t(
                                    texture->GetID()
                                )
                                );

                        ImGui::Image(
                            textureID,
                            ImVec2(
                                48.0f,
                                48.0f
                            ),
                            ImVec2(
                                0.0f,
                                1.0f
                            ),
                            ImVec2(
                                1.0f,
                                0.0f
                            )
                        );

                        ImGui::SameLine();
                    }
                }

                ImGui::Text(
                    "%s",
                    filename.c_str()
                );

                ImGui::EndDragDropSource();
            }

            /*
             * Right-click.
             */
            if (ImGui::IsItemClicked(
                ImGuiMouseButton_Right))
            {
                m_SelectedAssetPath =
                    entry.path.string();
            }

            /*
             * Asset context menu.
             */
            if (ImGui::BeginPopupContextItem(
                "##AssetContextMenu"))
            {
                ImGui::Text(
                    "%s",
                    filename.c_str()
                );

                ImGui::Separator();

                /*
                 * Open.
                 */
                if (ImGui::MenuItem(
                    "Open"))
                {
                    if (isScene)
                    {
                        if (m_Playing)
                        {
                            StopPlaying();
                        }

                        m_SelectedEntity =
                            Entity();

                        m_NameEditEntityID =
                            0;

                        m_NameEditBuffer[0] =
                            '\0';

                        SceneSerializer serializer(
                            scene
                        );

                        if (serializer.Load(
                            entry.path.string(),
                            m_HierarchyFolders
                        ))
                        {
                            m_SceneFilePath =
                                entry.path.string();

                            m_SelectedAssetPath =
                                entry.path.string();

                            Logger::Info(
                                std::string(
                                    "Loaded scene: "
                                ) +
                                entry.path.filename().string()
                            );
                        }
                        else
                        {
                            Logger::Error(
                                std::string(
                                    "Failed to load scene: "
                                ) +
                                entry.path.string()
                            );
                        }
                    }
                    else if (isUI)
                    {
                        m_SelectedAssetPath = entry.path.string();
                        m_PendingUIAssetPath = entry.path.string();
                    }
                    else if (
                        isImage ||
                        isScript)
                    {
                        OpenAssetExternally(
                            entry.path
                        );
                    }
                }

                /*
                 * Rename.
                 */
                if (ImGui::MenuItem(
                    "Rename"))
                {
                    m_AssetPopupPath =
                        entry.path.string();

                    std::snprintf(
                        m_AssetPopupBuffer,
                        sizeof(m_AssetPopupBuffer),
                        "%s",
                        entry.path.stem().string().c_str()
                    );

                    m_AssetPopup =
                        AssetPopup::Rename;
                }

                /*
                 * Delete.
                 */
                if (ImGui::MenuItem(
                    "Delete"))
                {
                    m_AssetPopupPath =
                        entry.path.string();

                    m_AssetPopupBuffer[0] =
                        '\0';

                    m_AssetPopup =
                        AssetPopup::Delete;
                }

                ImGui::Separator();

                ImGui::MenuItem(
                    "Properties"
                );

                ImGui::EndPopup();
            }

            /*
             * Selection outline.
             */
            if (selected)
            {
                const ImVec2 min =
                    ImGui::GetItemRectMin();

                const ImVec2 max =
                    ImGui::GetItemRectMax();

                ImGui::GetWindowDrawList()
                    ->AddRect(
                        ImVec2(
                            min.x - 2.0f,
                            min.y - 2.0f
                        ),
                        ImVec2(
                            max.x + 2.0f,
                            max.y + 2.0f
                        ),
                        ImGui::GetColorU32(
                            ImGuiCol_CheckMark
                        ),
                        4.0f,
                        0,
                        2.0f
                    );
            }

            /*
             * Filename.
             */
            ImGui::BeginGroup();

            ImGui::TextWrapped(
                "%s",
                filename.c_str()
            );

            ImGui::EndGroup();

            /*
             * Tooltip.
             */
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();

                ImGui::Text(
                    "%s",
                    filename.c_str()
                );

                ImGui::Separator();

                if (entry.directory)
                {
                    ImGui::Text(
                        "Type: Folder"
                    );
                }
                else if (isImage)
                {
                    Texture2D* texture =
                        renderer.LoadTexture(
                            entry.path.string()
                        );

                    ImGui::Text(
                        "Type: Texture"
                    );

                    if (texture != nullptr &&
                        texture->IsLoaded())
                    {
                        ImGui::Text(
                            "Size: %d x %d",
                            texture->GetWidth(),
                            texture->GetHeight()
                        );
                    }
                }
                else if (isScene)
                {
                    ImGui::Text(
                        "Type: Scene"
                    );
                }
                else if (isMesh)
                {
                    ImGui::Text("Type: OBJ Mesh");
                    ImGui::TextDisabled("Double-click to preview");
                }
                else if (isScript)
                {
                    ImGui::Text(
                        "Type: Lua Script"
                    );
                }
                else
                {
                    ImGui::Text(
                        "Type: File"
                    );
                }

                ImGui::TextWrapped(
                    "Path: %s",
                    entry.path.string().c_str()
                );

                ImGui::EndTooltip();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (!m_MeshPreviewPath.empty())
    {
        ImGui::SetNextWindowSize(ImVec2(440.0f, 210.0f), ImGuiCond_FirstUseEver);
        bool previewOpen = true;
        if (ImGui::Begin("Mesh Preview", &previewOpen))
        {
            ImGui::TextDisabled("MESH ASSET");
            ImGui::Separator();
            ImGui::TextWrapped("%s", fs::path(m_MeshPreviewPath).filename().string().c_str());
            ImGui::TextDisabled("%s", m_MeshPreviewPath.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("Drag the OBJ from Assets onto an entity's Mesh component to assign it.");
            ImGui::Spacing();
            ImGui::Text("Scene preview pipeline");
            ImGui::BulletText("Cached OBJ geometry");
            ImGui::BulletText("Material and local lighting");
            ImGui::BulletText("Directional shadows and HDR bloom");
            ImGui::Spacing();
            if (ImGui::Button("Clear Preview", ImVec2(110.0f, 0.0f)))
                previewOpen = false;
        }
        ImGui::End();
        if (!previewOpen)
            m_MeshPreviewPath.clear();
    }

    /*
     * Background context menu.
     */
    if (ImGui::BeginPopupContextWindow(
        "ContentBrowserBackground",
        ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::BeginMenu(
            "Create"))
        {
            if (ImGui::MenuItem(
                "Folder"))
            {
                m_AssetPopupPath =
                    currentPath.string();

                m_AssetPopupBuffer[0] =
                    '\0';

                m_AssetPopup =
                    AssetPopup::CreateFolder;
            }

            if (ImGui::MenuItem(
                "Scene"))
            {
                m_AssetPopupPath =
                    currentPath.string();

                m_AssetPopupBuffer[0] =
                    '\0';

                m_AssetPopup =
                    AssetPopup::CreateScene;
            }

            if (ImGui::MenuItem(
                "Script"))
            {
                m_AssetPopupPath =
                    currentPath.string();

                m_AssetPopupBuffer[0] =
                    '\0';

                m_AssetPopup =
                    AssetPopup::CreateScript;
            }

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    RenderContentBrowserPopups(
        scene
    );

    ImGui::End();
}

void Editor::RenderContentBrowserPopups(
    Scene& scene)
{
    namespace fs = std::filesystem;

    switch (m_AssetPopup)
    {
    case AssetPopup::Rename:
        ImGui::OpenPopup(
            "RenameAssetPopup"
        );
        break;

    case AssetPopup::Delete:
        ImGui::OpenPopup(
            "DeleteAssetPopup"
        );
        break;

    case AssetPopup::CreateFolder:
        ImGui::OpenPopup(
            "CreateFolderPopup"
        );
        break;

    case AssetPopup::CreateScene:
        ImGui::OpenPopup(
            "CreateScenePopup"
        );
        break;

    case AssetPopup::CreateScript:
        ImGui::OpenPopup(
            "CreateScriptPopup"
        );
        break;

    case AssetPopup::None:
        break;
    }

    m_AssetPopup =
        AssetPopup::None;

    /*
     * Create Folder.
     */
    if (ImGui::BeginPopupModal(
        "CreateFolderPopup",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(
            "Create Folder"
        );

        ImGui::Separator();

        ImGui::SetNextItemWidth(
            300.0f
        );

        const bool enterPressed =
            ImGui::InputText(
                "##AssetPopupInput",
                m_AssetPopupBuffer,
                sizeof(m_AssetPopupBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue
            );

        bool confirmed = false;

        if (ImGui::Button(
            "Create"))
        {
            confirmed = true;
        }

        ImGui::SameLine();

        if (ImGui::Button(
            "Cancel"))
        {
            m_AssetPopupPath.clear();

            m_AssetPopupBuffer[0] =
                '\0';

            ImGui::CloseCurrentPopup();
        }

        if (enterPressed || confirmed)
        {
            const std::string name =
                m_AssetPopupBuffer;

            if (name.empty())
            {
                Logger::Warning(
                    "Folder name cannot be empty."
                );
            }
            else if (
                name == "." ||
                name == ".." ||
                name.find_first_of(
                    "\\/:*?\"<>|"
                ) != std::string::npos)
            {
                Logger::Warning(
                    "Invalid folder name."
                );
            }
            else
            {
                const fs::path newPath =
                    fs::path(
                        m_AssetPopupPath
                    ) /
                    name;

                std::error_code error;

                fs::create_directory(
                    newPath,
                    error
                );

                if (error)
                {
                    Logger::Error(
                        std::string(
                            "Failed to create folder: "
                        ) +
                        error.message()
                    );
                }
                else
                {
                    Logger::Info(
                        std::string(
                            "Created folder: "
                        ) +
                        newPath.string()
                    );

                    m_AssetPopupPath.clear();

                    m_AssetPopupBuffer[0] =
                        '\0';

                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::EndPopup();
    }

    /*
     * Create Scene.
     */
    if (ImGui::BeginPopupModal(
        "CreateScenePopup",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(
            "Create Scene"
        );

        ImGui::Separator();

        ImGui::SetNextItemWidth(
            300.0f
        );

        const bool enterPressed =
            ImGui::InputText(
                "##CreateSceneName",
                m_AssetPopupBuffer,
                sizeof(m_AssetPopupBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue
            );

        bool confirmed = false;

        if (ImGui::Button(
            "Create"))
        {
            confirmed = true;
        }

        ImGui::SameLine();

        if (ImGui::Button(
            "Cancel"))
        {
            m_AssetPopupPath.clear();

            m_AssetPopupBuffer[0] =
                '\0';

            ImGui::CloseCurrentPopup();
        }

        if (enterPressed || confirmed)
        {
            std::string name =
                m_AssetPopupBuffer;

            if (name.empty())
            {
                Logger::Warning(
                    "Scene name cannot be empty."
                );
            }
            else if (
                name == "." ||
                name == ".." ||
                name.find_first_of(
                    "\\/:*?\"<>|"
                ) != std::string::npos)
            {
                Logger::Warning(
                    "Invalid scene name."
                );
            }
            else
            {
                const std::string extension =
                    ".scene";

                if (name.size() <
                    extension.size() ||
                    name.substr(
                        name.size() -
                        extension.size()
                    ) != extension)
                {
                    name += extension;
                }

                const fs::path scenePath =
                    fs::path(
                        m_AssetPopupPath
                    ) /
                    name;

                if (fs::exists(
                    scenePath))
                {
                    Logger::Warning(
                        std::string(
                            "A scene with that name already exists: "
                        ) +
                        scenePath.filename().string()
                    );
                }
                else
                {
                    Scene newScene;

                    std::vector<HierarchyFolder>
                        emptyFolders;

                    SceneSerializer serializer(
                        newScene
                    );

                    if (serializer.Save(
                        scenePath.string(),
                        emptyFolders
                    ))
                    {
                        m_SelectedAssetPath =
                            scenePath.string();

                        Logger::Info(
                            std::string(
                                "Created scene: "
                            ) +
                            scenePath.string()
                        );

                        m_AssetPopupPath.clear();

                        m_AssetPopupBuffer[0] =
                            '\0';

                        ImGui::CloseCurrentPopup();
                    }
                    else
                    {
                        Logger::Error(
                            std::string(
                                "Failed to create scene: "
                            ) +
                            scenePath.string()
                        );
                    }
                }
            }
        }

        ImGui::EndPopup();
    }

    /*
     * Create Script.
     */
    if (ImGui::BeginPopupModal(
        "CreateScriptPopup",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(
            "Create Script"
        );

        ImGui::Separator();

        ImGui::SetNextItemWidth(
            300.0f
        );

        const bool enterPressed =
            ImGui::InputText(
                "##CreateScriptName",
                m_AssetPopupBuffer,
                sizeof(m_AssetPopupBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue
            );

        bool confirmed = false;

        if (ImGui::Button(
            "Create"))
        {
            confirmed = true;
        }

        ImGui::SameLine();

        if (ImGui::Button(
            "Cancel"))
        {
            m_AssetPopupPath.clear();

            m_AssetPopupBuffer[0] =
                '\0';

            ImGui::CloseCurrentPopup();
        }

        if (enterPressed || confirmed)
        {
            std::string name =
                m_AssetPopupBuffer;

            if (name.empty())
            {
                Logger::Warning(
                    "Script name cannot be empty."
                );
            }
            else if (
                name == "." ||
                name == ".." ||
                name.find_first_of(
                    "\\/:*?\"<>|"
                ) != std::string::npos)
            {
                Logger::Warning(
                    "Invalid script name."
                );
            }
            else
            {
                const std::string extension =
                    ".lua";

                if (name.size() <
                    extension.size() ||
                    name.substr(
                        name.size() -
                        extension.size()
                    ) != extension)
                {
                    name += extension;
                }

                const fs::path scriptPath =
                    fs::path(
                        m_AssetPopupPath
                    ) /
                    name;

                if (fs::exists(
                    scriptPath))
                {
                    Logger::Warning(
                        std::string(
                            "A script with that name already exists: "
                        ) +
                        scriptPath.filename().string()
                    );
                }
                else
                {
                    std::ofstream file(
                        scriptPath
                    );

                    if (!file.is_open())
                    {
                        Logger::Error(
                            std::string(
                                "Failed to create script: "
                            ) +
                            scriptPath.string()
                        );
                    }
                    else
                    {
                        file <<
                            R"(function OnCreate()
end

function OnUpdate(deltaTime)
end

function OnDestroy()
end
)";

                        file.close();

                        if (!file.good())
                        {
                            Logger::Error(
                                std::string(
                                    "Failed to write script: "
                                ) +
                                scriptPath.string()
                            );
                        }
                        else
                        {
                            m_SelectedAssetPath =
                                scriptPath.string();

                            Logger::Info(
                                std::string(
                                    "Created script: "
                                ) +
                                scriptPath.string()
                            );

                            m_AssetPopupPath.clear();

                            m_AssetPopupBuffer[0] =
                                '\0';

                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
            }
        }

        ImGui::EndPopup();
    }

    /*
     * Rename.
     */
    if (ImGui::BeginPopupModal(
        "RenameAssetPopup",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        const fs::path oldPath =
            m_AssetPopupPath;

        const std::string extension =
            oldPath.extension().string();

        ImGui::Text(
            "Rename Asset"
        );

        ImGui::Separator();

        ImGui::SetNextItemWidth(
            250.0f
        );

        const bool enterPressed =
            ImGui::InputText(
                "##AssetPopupInput",
                m_AssetPopupBuffer,
                sizeof(m_AssetPopupBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue
            );

        ImGui::SameLine();

        if (!extension.empty())
        {
            ImGui::TextUnformatted(
                extension.c_str()
            );
        }

        bool confirmed = false;

        if (ImGui::Button(
            "Rename"))
        {
            confirmed = true;
        }

        ImGui::SameLine();

        if (ImGui::Button(
            "Cancel"))
        {
            m_AssetPopupPath.clear();

            m_AssetPopupBuffer[0] =
                '\0';

            ImGui::CloseCurrentPopup();
        }

        if (enterPressed || confirmed)
        {
            const std::string newStem =
                m_AssetPopupBuffer;

            if (newStem.empty())
            {
                Logger::Warning(
                    "Asset name cannot be empty."
                );
            }
            else if (
                newStem == "." ||
                newStem == ".." ||
                newStem.find_first_of(
                    "\\/:*?\"<>|"
                ) != std::string::npos)
            {
                Logger::Warning(
                    "Invalid asset name."
                );
            }
            else
            {
                const fs::path newPath =
                    oldPath.parent_path() /
                    (
                        newStem +
                        oldPath.extension().string()
                        );

                if (fs::exists(
                    newPath))
                {
                    Logger::Warning(
                        std::string(
                            "An asset with that name already exists: "
                        ) +
                        newPath.filename().string()
                    );
                }
                else
                {
                    std::error_code error;

                    fs::rename(
                        oldPath,
                        newPath,
                        error
                    );

                    if (error)
                    {
                        Logger::Error(
                            std::string(
                                "Failed to rename asset: "
                            ) +
                            error.message()
                        );
                    }
                    else
                    {
                        if (m_SelectedAssetPath ==
                            oldPath.string())
                        {
                            m_SelectedAssetPath =
                                newPath.string();
                        }

                        if (m_SceneFilePath ==
                            oldPath.string())
                        {
                            m_SceneFilePath =
                                newPath.string();
                        }

                        Logger::Info(
                            std::string(
                                "Renamed asset: "
                            ) +
                            oldPath.filename().string() +
                            " -> " +
                            newPath.filename().string()
                        );

                        m_AssetPopupPath.clear();

                        m_AssetPopupBuffer[0] =
                            '\0';

                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }

        ImGui::EndPopup();
    }

    /*
     * Delete.
     */
    if (ImGui::BeginPopupModal(
        "DeleteAssetPopup",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        const fs::path deletePath =
            m_AssetPopupPath;

        ImGui::Text(
            "Are you sure you want to delete:"
        );

        ImGui::Spacing();

        ImGui::Text(
            "%s",
            deletePath.filename().string().c_str()
        );

        ImGui::Spacing();

        ImGui::TextWrapped(
            "This action cannot be undone."
        );

        ImGui::Separator();

        bool confirmed = false;

        if (ImGui::Button(
            "Delete",
            ImVec2(
                100.0f,
                0.0f
            )))
        {
            confirmed = true;
        }

        ImGui::SameLine();

        if (ImGui::Button(
            "Cancel",
            ImVec2(
                100.0f,
                0.0f
            )))
        {
            m_AssetPopupPath.clear();

            ImGui::CloseCurrentPopup();
        }

        if (confirmed)
        {
            std::error_code error;

            fs::remove_all(
                deletePath,
                error
            );

            if (error)
            {
                Logger::Error(
                    std::string(
                        "Failed to delete asset: "
                    ) +
                    error.message()
                );
            }
            else
            {
                if (m_SelectedAssetPath ==
                    deletePath.string())
                {
                    m_SelectedAssetPath.clear();
                }

                if (m_SceneFilePath ==
                    deletePath.string())
                {
                    m_SceneFilePath.clear();
                }

                Logger::Info(
                    std::string(
                        "Deleted asset: "
                    ) +
                    deletePath.filename().string()
                );

                m_AssetPopupPath.clear();

                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

std::string Editor::ConsumeOpenedUIAsset()
{
    std::string path = std::move(m_PendingUIAssetPath);
    m_PendingUIAssetPath.clear();
    return path;
}
