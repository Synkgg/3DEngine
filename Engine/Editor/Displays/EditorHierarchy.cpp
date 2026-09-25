#include "../Editor.h"
#include "../HierarchyFolder.h"

#include "../../Scene/Scene.h"
#include "../../Scene/PrefabSerializer.h"

#include "../../Scene/Components/TransformComponent.h"
#include "../../Scene/Components/MeshComponent.h"
#include "../../Scene/Components/ColorComponent.h"
#include "../../Scene/Components/NameComponent.h"
#include "../../Scene/Components/PlayerComponent.h"
#include "../../Scene/Components/CharacterControllerComponent.h"
#include "../../Scene/Components/LightComponent.h"
#include "../../Scene/Components/ColliderComponent.h"
#include "../../Scene/Components/TextureComponent.h"

#include "../../Graphics/PrimitiveType.h"
#include "../../Core/Logger.h"

#include "../Fonts/IconsFontAwesome6.h"

#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>
#include <filesystem>

namespace
{
    char g_RenameFolderBuffer[256] = {};

    HierarchyFolder* g_RenameFolder = nullptr;

    std::string MakeUniqueName(
        Scene& scene,
        const std::string& baseName,
        Entity excludedEntity)
    {
        std::string name =
            baseName;

        int suffix = 2;

        while (true)
        {
            bool exists = false;

            for (const Entity& entity :
                scene.GetEntities())
            {
                if (entity.GetID() ==
                    excludedEntity.GetID())
                {
                    continue;
                }

                NameComponent* existing =
                    scene.GetComponent<NameComponent>(
                        entity
                    );

                if (existing != nullptr &&
                    existing->name == name)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
            {
                return name;
            }

            name =
                baseName +
                " " +
                std::to_string(suffix++);
        }
    }

    bool RemoveEntityFromFolders(
        std::vector<HierarchyFolder>& folders,
        std::uint32_t entityID)
    {
        bool removed = false;

        for (HierarchyFolder& folder :
            folders)
        {
            auto it =
                std::remove(
                    folder.entities.begin(),
                    folder.entities.end(),
                    entityID
                );

            if (it != folder.entities.end())
            {
                folder.entities.erase(
                    it,
                    folder.entities.end()
                );

                removed = true;
            }

            if (RemoveEntityFromFolders(
                folder.children,
                entityID))
            {
                removed = true;
            }
        }

        return removed;
    }

    bool IsEntityInFolders(
        const std::vector<HierarchyFolder>& folders,
        std::uint32_t entityID)
    {
        for (const HierarchyFolder& folder :
            folders)
        {
            if (std::find(
                folder.entities.begin(),
                folder.entities.end(),
                entityID
            ) != folder.entities.end())
            {
                return true;
            }

            if (IsEntityInFolders(
                folder.children,
                entityID))
            {
                return true;
            }
        }

        return false;
    }

    HierarchyFolder* FindFolderContainingEntity(
        std::vector<HierarchyFolder>& folders,
        std::uint32_t entityID)
    {
        for (HierarchyFolder& folder :
            folders)
        {
            if (std::find(
                folder.entities.begin(),
                folder.entities.end(),
                entityID
            ) != folder.entities.end())
            {
                return &folder;
            }

            HierarchyFolder* result =
                FindFolderContainingEntity(
                    folder.children,
                    entityID
                );

            if (result != nullptr)
            {
                return result;
            }
        }

        return nullptr;
    }

    bool RenderHierarchyFolder(
        HierarchyFolder& folder,
        Scene& scene,
        Entity& selectedEntity,
        std::vector<HierarchyFolder>& allFolders,
        ImFont* iconFont)
    {
        ImGui::PushID(&folder);

        bool deleteFolder = false;
        bool openRenamePopup = false;

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (folder.expanded)
        {
            flags |=
                ImGuiTreeNodeFlags_DefaultOpen;
        }

        /*
         * Tree node.
         */
        bool open =
            ImGui::TreeNodeEx(
                "##FolderTreeNode",
                flags
            );

        /*
         * Folder icon.
         */
        ImGui::SameLine();

        if (iconFont != nullptr)
        {
            const float iconSize =
                ImGui::GetFontSize();

            const float lineHeight =
                ImGui::GetTextLineHeight();

            ImVec2 iconPosition =
                ImGui::GetCursorScreenPos();

            iconPosition.y +=
                (lineHeight - iconSize) * 0.5f;

            ImGui::GetWindowDrawList()->AddText(
                iconFont,
                iconSize,
                iconPosition,
                ImGui::GetColorU32(
                    ImVec4(
                        1.0f,
                        0.75f,
                        0.20f,
                        1.0f
                    )
                ),
                ICON_FA_FOLDER
            );

            ImGui::Dummy(
                ImVec2(
                    iconSize,
                    lineHeight
                )
            );
        }

        /*
         * Folder name.
         */
        ImGui::SameLine();

        ImGui::TextUnformatted(
            folder.name.c_str()
        );

        /*
         * Folder context menu.
         */
        if (ImGui::BeginPopupContextItem(
            "FolderContextMenu"))
        {
            /*
             * Create child folder.
             */
            if (ImGui::MenuItem(
                "Create Child Folder"))
            {
                folder.children.push_back(
                    HierarchyFolder{
                        "New Folder"
                    }
                );

                folder.expanded = true;
            }

            /*
             * Rename folder.
             *
             * Do NOT open the rename popup here.
             * We wait until this popup closes.
             */
            if (ImGui::MenuItem(
                "Rename"))
            {
                std::strncpy(
                    g_RenameFolderBuffer,
                    folder.name.c_str(),
                    sizeof(g_RenameFolderBuffer) - 1
                );

                g_RenameFolderBuffer[
                    sizeof(g_RenameFolderBuffer) - 1
                ] = '\0';

                g_RenameFolder =
                    &folder;

                openRenamePopup = true;
            }

            ImGui::Separator();

            /*
             * Delete folder.
             */
            if (ImGui::MenuItem(
                "Delete Folder"))
            {
                deleteFolder = true;

                if (g_RenameFolder == &folder)
                {
                    g_RenameFolder = nullptr;
                }
            }

            ImGui::EndPopup();
        }

        /*
         * Open rename popup AFTER the
         * context menu has closed.
         */
        if (openRenamePopup)
        {
            ImGui::OpenPopup(
                "RenameFolderPopup"
            );
        }

        /*
         * Rename popup.
         */
        if (ImGui::BeginPopup(
            "RenameFolderPopup"))
        {
            ImGui::TextUnformatted(
                "Rename Folder"
            );

            ImGui::Spacing();

            ImGui::SetNextItemWidth(
                220.0f
            );

            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }

            const bool enterPressed =
                ImGui::InputText(
                    "##FolderName",
                    g_RenameFolderBuffer,
                    sizeof(g_RenameFolderBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue
                );

            ImGui::Spacing();

            /*
             * Rename button.
             */
            if (ImGui::Button("Rename") ||
                enterPressed)
            {
                if (g_RenameFolder != nullptr &&
                    g_RenameFolderBuffer[0] != '\0')
                {
                    g_RenameFolder->name =
                        g_RenameFolderBuffer;
                }

                g_RenameFolder =
                    nullptr;

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            /*
             * Cancel button.
             */
            if (ImGui::Button("Cancel"))
            {
                g_RenameFolder =
                    nullptr;

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        /*
         * Folder drop target.
         */
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload(
                    "HIERARCHY_ENTITY"))
            {
                if (payload->DataSize ==
                    sizeof(std::uint32_t))
                {
                    const std::uint32_t entityID =
                        *static_cast<
                        const std::uint32_t*
                        >(
                            payload->Data
                            );

                    RemoveEntityFromFolders(
                        allFolders,
                        entityID
                    );

                    folder.entities.push_back(
                        entityID
                    );
                }
            }

            ImGui::EndDragDropTarget();
        }

        folder.expanded =
            open;

        if (open)
        {
            /*
             * Entities inside folder.
             */
            for (std::uint32_t entityID :
            folder.entities)
            {
                Entity entityToRender;

                bool found = false;

                for (const Entity& entity :
                    scene.GetEntities())
                {
                    if (entity.GetID() ==
                        entityID)
                    {
                        entityToRender =
                            entity;

                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    continue;
                }

                NameComponent* name =
                    scene.GetComponent<NameComponent>(
                        entityToRender
                    );

                const char* displayName =
                    (
                        name != nullptr &&
                        !name->name.empty()
                        )
                    ? name->name.c_str()
                    : "(Unnamed)";

                const bool selected =
                    selectedEntity.GetID() ==
                    entityToRender.GetID();

                ImGui::PushID(
                    static_cast<int>(
                        entityToRender.GetID()
                        )
                );

                /*
                 * Entity.
                 */
                if (ImGui::Selectable(
                    displayName,
                    selected
                ))
                {
                    selectedEntity =
                        entityToRender;
                }

                /*
                 * Context menu.
                 */
                if (ImGui::BeginPopupContextItem())
                {
                    /*
                     * Duplicate.
                     */
                    if (ImGui::MenuItem(
                        "Duplicate"))
                    {
                        Entity duplicate = scene.DuplicateEntity(entityToRender, true);
                        if (duplicate.IsValid())
                        {
                            if (TransformComponent* transform = scene.GetComponent<TransformComponent>(duplicate))
                                transform->transform.position.x += 1.0f;
                            selectedEntity = duplicate;
                        }

                        Logger::Info(
                            std::string(
                                "Duplicated Entity "
                            ) +
                            std::to_string(
                                entityToRender.GetID()
                            )
                        );
                    }

                    /*
                     * Delete.
                     */
                    if (ImGui::MenuItem(
                        "Delete"))
                    {
                        const std::uint32_t deletedID =
                            entityToRender.GetID();

                        RemoveEntityFromFolders(
                            allFolders,
                            deletedID
                        );

                        scene.DestroyEntity(
                            entityToRender
                        );

                        if (selectedEntity.GetID() ==
                            deletedID)
                        {
                            selectedEntity =
                                Entity();
                        }

                        Logger::Info(
                            std::string(
                                "Deleted Entity "
                            ) +
                            std::to_string(
                                deletedID
                            )
                        );
                    }

                    ImGui::EndPopup();
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
                    {
                        const std::uint32_t childID = *static_cast<const std::uint32_t*>(payload->Data);
                        Entity child;
                        for (const Entity& candidate : scene.GetEntities())
                            if (candidate.GetID() == childID) { child = candidate; break; }
                        if (child.IsValid() && scene.SetParent(child, entityToRender, true))
                            Logger::Info("Parented entity " + std::to_string(childID) + " to " + std::to_string(entityToRender.GetID()));
                    }
                    ImGui::EndDragDropTarget();
                }

                /*
                 * Drag entity.
                 */
                if (ImGui::BeginDragDropSource())
                {
                    const std::uint32_t draggedID =
                        entityToRender.GetID();

                    ImGui::SetDragDropPayload(
                        "HIERARCHY_ENTITY",
                        &draggedID,
                        sizeof(draggedID)
                    );

                    ImGui::Text(
                        "%s",
                        displayName
                    );

                    ImGui::EndDragDropSource();
                }

                ImGui::PopID();
            }

            /*
             * Child folders.
             */
            for (auto it =
                folder.children.begin();
                it != folder.children.end();)
            {
                if (RenderHierarchyFolder(
                    *it,
                    scene,
                    selectedEntity,
                    allFolders,
                    iconFont
                ))
                {
                    it =
                        folder.children.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            ImGui::TreePop();
        }

        /*
         * Delete folder.
         */
        if (deleteFolder)
        {
            ImGui::PopID();

            return true;
        }

        ImGui::PopID();

        return false;
    }
}

Entity Editor::CreatePrimitiveEntity(
    Scene& scene,
    PrimitiveType primitive,
    const char* name)
{
    Entity entity =
        scene.CreateEntity();

    MeshComponent mesh;

    mesh.primitive =
        primitive;

    scene.AddComponent<MeshComponent>(
        entity,
        mesh
    );

    scene.AddComponent<ColorComponent>(
        entity
    );

    NameComponent* nameComponent =
        scene.GetComponent<NameComponent>(
            entity
        );

    if (nameComponent != nullptr)
    {
        nameComponent->name =
            MakeUniqueName(
                scene,
                name,
                entity
            );
    }

    m_SelectedEntity =
        entity;

    Logger::Info(
        std::string("Created ") +
        (
            nameComponent != nullptr
            ? nameComponent->name
            : name
            )
    );

    return entity;
}

void Editor::RenderHierarchy(Scene& scene, ImFont* iconFont)
{
    ImGui::Begin("Hierarchy");

    // One-time migration: legacy editor folders become ordinary empty entities.
    // From this point forward the scene graph is the hierarchy source of truth.
    if (!m_HierarchyFolders.empty())
    {
        std::function<void(const HierarchyFolder&, Entity)> migrateFolder;
        migrateFolder = [&](const HierarchyFolder& folder, Entity parent)
        {
            Entity group = scene.CreateEntity();
            if (NameComponent* name = scene.GetComponent<NameComponent>(group))
                name->name = MakeUniqueName(scene, folder.name.empty() ? "Group" : folder.name, group);
            if (parent.IsValid()) scene.SetParent(group, parent, false);

            for (std::uint32_t id : folder.entities)
            {
                Entity member = scene.FindEntityByID(id);
                if (member.IsValid()) scene.SetParent(member, group, true);
            }
            for (const HierarchyFolder& child : folder.children)
                migrateFolder(child, group);
        };

        for (const HierarchyFolder& folder : m_HierarchyFolders)
            migrateFolder(folder, Entity());
        m_HierarchyFolders.clear();
        Logger::Info("Migrated legacy hierarchy folders to scene graph groups.");
    }

    ImGui::SetNextItemWidth(-86.0f);
    ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", m_HierarchySearchBuffer, sizeof(m_HierarchySearchBuffer));
    ImGui::SameLine();

    if (ImGui::Button("Create"))
        ImGui::OpenPopup("CreateEntityPopup");

    if (ImGui::BeginPopup("CreateEntityPopup"))
    {
        auto createEmpty = [&](const char* baseName)
        {
            Entity entity = scene.CreateEntity();
            if (NameComponent* name = scene.GetComponent<NameComponent>(entity))
                name->name = MakeUniqueName(scene, baseName, entity);
            if (m_SelectedEntity.IsValid())
                scene.SetParent(entity, m_SelectedEntity, false);
            m_SelectedEntity = entity;
        };

        if (ImGui::Selectable("Group / Empty")) createEmpty("Group");
        ImGui::Separator();
        if (ImGui::Selectable("Cube")) CreatePrimitiveEntity(scene, PrimitiveType::Cube, "Cube");
        if (ImGui::Selectable("Sphere")) CreatePrimitiveEntity(scene, PrimitiveType::Sphere, "Sphere");
        if (ImGui::Selectable("Plane")) CreatePrimitiveEntity(scene, PrimitiveType::Plane, "Plane");
        if (ImGui::Selectable("Cylinder")) CreatePrimitiveEntity(scene, PrimitiveType::Cylinder, "Cylinder");
        ImGui::Separator();
        if (ImGui::Selectable("Directional Light"))
        {
            Entity entity = scene.CreateEntity();
            scene.AddComponent<LightComponent>(entity);
            if (NameComponent* name = scene.GetComponent<NameComponent>(entity))
                name->name = MakeUniqueName(scene, "Directional Light", entity);
            m_SelectedEntity = entity;
        }
        if (ImGui::Selectable("Player"))
        {
            Entity entity = scene.CreateEntity();
            scene.AddComponent<PlayerComponent>(entity);
            scene.AddComponent<CharacterControllerComponent>(entity);
            scene.AddComponent<ColliderComponent>(entity);
            if (NameComponent* name = scene.GetComponent<NameComponent>(entity))
                name->name = MakeUniqueName(scene, "Player", entity);
            m_SelectedEntity = entity;
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    std::string search = m_HierarchySearchBuffer;
    std::transform(search.begin(), search.end(), search.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    std::function<bool(Entity)> matchesSearch;
    matchesSearch = [&](Entity entity)
    {
        if (search.empty()) return true;
        const NameComponent* name = scene.GetComponent<NameComponent>(entity);
        std::string value = name ? name->name : std::string();
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (value.find(search) != std::string::npos) return true;
        for (Entity child : scene.GetChildren(entity))
            if (matchesSearch(child)) return true;
        return false;
    };

    std::function<void(Entity)> drawEntity;
    drawEntity = [&](Entity entity)
    {
        if (!matchesSearch(entity)) return;

        const std::vector<Entity> children = scene.GetChildren(entity);
        NameComponent* name = scene.GetComponent<NameComponent>(entity);
        const std::string label = (name && !name->name.empty()) ? name->name : "(Unnamed)";

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
        if (m_SelectedEntity.GetID() == entity.GetID()) flags |= ImGuiTreeNodeFlags_Selected;
        if (!search.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;

        ImGui::PushID(static_cast<int>(entity.GetID()));

        // Empty scene-graph entities act as hierarchy folders/groups.
        const bool isGroup =
            !scene.HasComponent<MeshComponent>(entity) &&
            !scene.HasComponent<PlayerComponent>(entity) &&
            !scene.HasComponent<CharacterControllerComponent>(entity) &&
            !scene.HasComponent<LightComponent>(entity) &&
            !scene.HasComponent<ColliderComponent>(entity) &&
            !scene.HasComponent<TextureComponent>(entity);

        bool open = false;
        if (isGroup)
        {
            const ImVec4 folderColor(1.0f, 0.75f, 0.20f, 1.0f);

            // Draw the icon with the icon font, but keep one invisible TreeNode
            // as the row's interaction owner.
            // "##" in the formatted label still gets rendered by TreeNodeEx,
            // so use an empty visible label and draw the folder contents ourselves.
            open = ImGui::TreeNodeEx("##EntityNode", flags, "%s", "");
            const ImVec2 rowMin = ImGui::GetItemRectMin();
            ImVec2 textPos(rowMin.x + ImGui::GetTreeNodeToLabelSpacing(), rowMin.y);

            if (iconFont != nullptr)
            {
                const float iconSize = ImGui::GetFontSize();
                ImGui::GetWindowDrawList()->AddText(iconFont, iconSize, textPos,
                    ImGui::GetColorU32(folderColor), ICON_FA_FOLDER);
                textPos.x += iconSize + ImGui::GetStyle().ItemInnerSpacing.x;
            }

            ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(folderColor), label.c_str());
        }
        else
        {
            open = ImGui::TreeNodeEx("##EntityNode", flags, "%s", label.c_str());
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            m_SelectedEntity = entity;

        if (ImGui::BeginDragDropSource())
        {
            const std::uint32_t id = entity.GetID();
            ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &id, sizeof(id));
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
            {
                if (payload->DataSize == sizeof(std::uint32_t))
                {
                    Entity child = scene.FindEntityByID(*static_cast<const std::uint32_t*>(payload->Data));
                    if (child.IsValid()) scene.SetParent(child, entity, true);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("EntityContext"))
        {
            if (ImGui::MenuItem("Create Child Group"))
            {
                Entity child = scene.CreateEntity();
                if (NameComponent* childName = scene.GetComponent<NameComponent>(child))
                    childName->name = MakeUniqueName(scene, "Group", child);
                scene.SetParent(child, entity, false);
                m_SelectedEntity = child;
            }
            if (ImGui::MenuItem("Duplicate Hierarchy"))
            {
                Entity copy = scene.DuplicateEntity(entity, true);
                if (copy.IsValid())
                {
                    if (NameComponent* copyName = scene.GetComponent<NameComponent>(copy))
                        copyName->name = MakeUniqueName(scene, copyName->name, copy);
                    m_SelectedEntity = copy;
                }
            }
            if (ImGui::MenuItem("Create Prefab"))
            {
                namespace fs = std::filesystem;
                fs::path directory = fs::current_path() / "Assets" / "Prefabs";
                std::error_code error;
                fs::create_directories(directory, error);
                std::string filename = label;
                for (char& c : filename)
                    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
                const fs::path prefabPath = directory / (filename + ".prefab");
                if (!error) PrefabSerializer::Save(scene, entity, prefabPath.string());
            }
            if (PrefabSerializer::IsInstanceRoot(scene, entity))
            {
                ImGui::Separator();
                ImGui::TextDisabled("PREFAB INSTANCE");
                const std::string prefabSource = PrefabSerializer::GetSource(scene, entity);
                ImGui::TextWrapped("%s", prefabSource.c_str());
                if (ImGui::MenuItem("Apply Prefab"))
                    PrefabSerializer::Apply(scene, entity);
                if (ImGui::MenuItem("Revert Prefab"))
                {
                    PrefabSerializer::Revert(scene, entity);
                    m_SelectedEntity = Entity();
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return;
                }
                if (ImGui::MenuItem("Unpack Prefab"))
                    PrefabSerializer::Unpack(scene, entity, true);
                ImGui::Separator();
            }
            if (scene.GetParent(entity).IsValid() && ImGui::MenuItem("Unparent"))
                scene.ClearParent(entity, true);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Hierarchy"))
            {
                const std::uint32_t deleted = entity.GetID();
                scene.DestroyEntityHierarchy(entity);
                if (m_SelectedEntity.GetID() == deleted) m_SelectedEntity = Entity();
                ImGui::EndPopup();
                if (open) ImGui::TreePop();
                ImGui::PopID();
                return;
            }
            ImGui::EndPopup();
        }

        if (open)
        {
            for (Entity child : children)
                if (scene.FindEntityByID(child.GetID()).IsValid()) drawEntity(child);
            ImGui::TreePop();
        }
        ImGui::PopID();
    };

    const std::vector<Entity> roots = scene.GetRootEntities();
    for (Entity root : roots)
        drawEntity(root);

    ImVec2 available = ImGui::GetContentRegionAvail();
    if (available.y < 28.0f) available.y = 28.0f;
    ImGui::InvisibleButton("##HierarchyRootDrop", available);
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY"))
        {
            if (payload->DataSize == sizeof(std::uint32_t))
            {
                Entity entity = scene.FindEntityByID(*static_cast<const std::uint32_t*>(payload->Data));
                if (entity.IsValid()) scene.ClearParent(entity, true);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}
