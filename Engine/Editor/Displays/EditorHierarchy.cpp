#include "../Editor.h"
#include "../HierarchyFolder.h"

#include "../../Scene/Scene.h"

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
                        Entity duplicate =
                            scene.CreateEntity();

                        /*
                         * Transform.
                         */
                        TransformComponent*
                            sourceTransform =
                            scene.GetComponent<
                            TransformComponent
                            >(entityToRender);

                        TransformComponent*
                            duplicateTransform =
                            scene.GetComponent<
                            TransformComponent
                            >(duplicate);

                        if (sourceTransform != nullptr &&
                            duplicateTransform != nullptr)
                        {
                            duplicateTransform->transform =
                                sourceTransform->transform;

                            duplicateTransform->
                                transform.position.x +=
                                1.0f;
                        }

                        /*
                         * Mesh.
                         */
                        MeshComponent* sourceMesh =
                            scene.GetComponent<
                            MeshComponent
                            >(entityToRender);

                        if (sourceMesh != nullptr)
                        {
                            scene.AddComponent<
                                MeshComponent
                            >(
                                duplicate,
                                *sourceMesh
                            );
                        }

                        /*
                         * Color.
                         */
                        ColorComponent* sourceColor =
                            scene.GetComponent<
                            ColorComponent
                            >(entityToRender);

                        if (sourceColor != nullptr)
                        {
                            scene.AddComponent<
                                ColorComponent
                            >(
                                duplicate,
                                *sourceColor
                            );
                        }

                        /*
                         * Player.
                         */
                        PlayerComponent* sourcePlayer =
                            scene.GetComponent<
                            PlayerComponent
                            >(entityToRender);

                        if (sourcePlayer != nullptr)
                        {
                            scene.AddComponent<
                                PlayerComponent
                            >(
                                duplicate,
                                *sourcePlayer
                            );
                        }

                        /*
                         * Character Controller.
                         */
                        CharacterControllerComponent*
                            sourceController =
                            scene.GetComponent<
                            CharacterControllerComponent
                            >(entityToRender);

                        if (sourceController != nullptr)
                        {
                            scene.AddComponent<
                                CharacterControllerComponent
                            >(
                                duplicate,
                                *sourceController
                            );
                        }

                        /*
                         * Light.
                         */
                        LightComponent* sourceLight =
                            scene.GetComponent<
                            LightComponent
                            >(entityToRender);

                        if (sourceLight != nullptr)
                        {
                            scene.AddComponent<
                                LightComponent
                            >(
                                duplicate,
                                *sourceLight
                            );
                        }

                        /*
                         * Collider.
                         */
                        ColliderComponent* sourceCollider =
                            scene.GetComponent<
                            ColliderComponent
                            >(entityToRender);

                        if (sourceCollider != nullptr)
                        {
                            scene.AddComponent<
                                ColliderComponent
                            >(
                                duplicate,
                                *sourceCollider
                            );
                        }

                        /*
                         * Texture.
                         */
                        TextureComponent* sourceTexture =
                            scene.GetComponent<
                            TextureComponent
                            >(entityToRender);

                        if (sourceTexture != nullptr)
                        {
                            scene.AddComponent<
                                TextureComponent
                            >(
                                duplicate,
                                *sourceTexture
                            );
                        }

                        /*
                         * Name.
                         */
                        NameComponent* sourceName =
                            scene.GetComponent<
                            NameComponent
                            >(entityToRender);

                        NameComponent* duplicateName =
                            scene.GetComponent<
                            NameComponent
                            >(duplicate);

                        if (duplicateName != nullptr)
                        {
                            std::string baseName =
                                sourceName != nullptr &&
                                !sourceName->name.empty()
                                ? sourceName->name + " Copy"
                                : "Entity Copy";

                            duplicateName->name =
                                MakeUniqueName(
                                    scene,
                                    baseName,
                                    duplicate
                                );
                        }

                        /*
                         * Preserve folder membership.
                         */
                        HierarchyFolder* sourceFolder =
                            FindFolderContainingEntity(
                                allFolders,
                                entityToRender.GetID()
                            );

                        if (sourceFolder != nullptr)
                        {
                            sourceFolder->entities.push_back(
                                duplicate.GetID()
                            );
                        }

                        selectedEntity =
                            duplicate;

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

void Editor::RenderHierarchy(
    Scene& scene,
    ImFont* iconFont)
{
    ImGui::Begin("Hierarchy");

    /*
     * Create button.
     */
    if (ImGui::Button("Create"))
    {
        ImGui::OpenPopup(
            "CreateEntityPopup"
        );
    }

    /*
     * Create menu.
     */
    if (ImGui::BeginPopup(
        "CreateEntityPopup"))
    {
        /*
         * Folder.
         */
        if (ImGui::Selectable(
            "Folder"))
        {
            m_HierarchyFolders.push_back(
                HierarchyFolder{
                    "New Folder"
                }
            );
        }

        ImGui::Separator();

        /*
         * Empty Entity.
         */
        if (ImGui::Selectable(
            "Empty Entity"))
        {
            Entity entity =
                scene.CreateEntity();

            NameComponent* name =
                scene.GetComponent<NameComponent>(
                    entity
                );

            if (name != nullptr)
            {
                name->name =
                    MakeUniqueName(
                        scene,
                        "Empty Entity",
                        entity
                    );
            }

            m_SelectedEntity =
                entity;

            Logger::Info(
                std::string("Created ") +
                (
                    name != nullptr
                    ? name->name
                    : "Empty Entity"
                    )
            );
        }

        ImGui::Separator();

        /*
         * Cube.
         */
        if (ImGui::Selectable(
            "Cube"))
        {
            CreatePrimitiveEntity(
                scene,
                PrimitiveType::Cube,
                "Cube"
            );
        }

        /*
         * Sphere.
         */
        if (ImGui::Selectable(
            "Sphere"))
        {
            CreatePrimitiveEntity(
                scene,
                PrimitiveType::Sphere,
                "Sphere"
            );
        }

        /*
         * Plane.
         */
        if (ImGui::Selectable(
            "Plane"))
        {
            CreatePrimitiveEntity(
                scene,
                PrimitiveType::Plane,
                "Plane"
            );
        }

        /*
         * Cylinder.
         */
        if (ImGui::Selectable(
            "Cylinder"))
        {
            CreatePrimitiveEntity(
                scene,
                PrimitiveType::Cylinder,
                "Cylinder"
            );
        }

        ImGui::Separator();

        /*
         * Directional Light.
         */
        if (ImGui::Selectable(
            "Directional Light"))
        {
            Entity entity =
                scene.CreateEntity();

            scene.AddComponent<LightComponent>(
                entity
            );

            NameComponent* name =
                scene.GetComponent<NameComponent>(
                    entity
                );

            if (name != nullptr)
            {
                name->name =
                    MakeUniqueName(
                        scene,
                        "Directional Light",
                        entity
                    );
            }

            m_SelectedEntity =
                entity;

            Logger::Info(
                "Created Directional Light."
            );
        }

        ImGui::Separator();

        /*
         * Player.
         */
        if (ImGui::Selectable(
            "Player"))
        {
            Entity entity =
                scene.CreateEntity();

            scene.AddComponent<PlayerComponent>(
                entity
            );

            scene.AddComponent<
                CharacterControllerComponent
            >(
                entity
            );

            scene.AddComponent<ColliderComponent>(
                entity
            );

            NameComponent* name =
                scene.GetComponent<NameComponent>(
                    entity
                );

            if (name != nullptr)
            {
                name->name =
                    MakeUniqueName(
                        scene,
                        "Player",
                        entity
                    );
            }

            m_SelectedEntity =
                entity;

            Logger::Info(
                std::string("Created ") +
                (
                    name != nullptr
                    ? name->name
                    : "Player"
                    )
            );
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    /*
     * User-created folders.
     */
    for (auto it =
        m_HierarchyFolders.begin();
        it != m_HierarchyFolders.end();)
    {
        if (RenderHierarchyFolder(
            *it,
            scene,
            m_SelectedEntity,
            m_HierarchyFolders,
            iconFont
        ))
        {
            it =
                m_HierarchyFolders.erase(it);
        }
        else
        {
            ++it;
        }
    }

    /*
     * Root entities.
     */
    const std::vector<Entity> entities =
        scene.GetEntities();

    for (const Entity& entity :
        entities)
    {
        if (IsEntityInFolders(
            m_HierarchyFolders,
            entity.GetID()
        ))
        {
            continue;
        }

        NameComponent* name =
            scene.GetComponent<NameComponent>(
                entity
            );

        const char* displayName =
            (
                name != nullptr &&
                !name->name.empty()
                )
            ? name->name.c_str()
            : "(Unnamed)";

        const bool selected =
            m_SelectedEntity.GetID() ==
            entity.GetID();

        ImGui::PushID(
            static_cast<int>(
                entity.GetID()
                )
        );

        /*
         * Select.
         */
        if (ImGui::Selectable(
            displayName,
            selected
        ))
        {
            m_SelectedEntity =
                entity;
        }

        /*
         * Drag.
         */
        if (ImGui::BeginDragDropSource())
        {
            const std::uint32_t entityID =
                entity.GetID();

            ImGui::SetDragDropPayload(
                "HIERARCHY_ENTITY",
                &entityID,
                sizeof(entityID)
            );

            ImGui::Text(
                "%s",
                displayName
            );

            ImGui::EndDragDropSource();
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
                Entity duplicate =
                    scene.CreateEntity();

                /*
                 * Transform.
                 */
                TransformComponent*
                    sourceTransform =
                    scene.GetComponent<
                    TransformComponent
                    >(entity);

                TransformComponent*
                    duplicateTransform =
                    scene.GetComponent<
                    TransformComponent
                    >(duplicate);

                if (sourceTransform != nullptr &&
                    duplicateTransform != nullptr)
                {
                    duplicateTransform->transform =
                        sourceTransform->transform;

                    duplicateTransform->
                        transform.position.x +=
                        1.0f;
                }

                /*
                 * Mesh.
                 */
                MeshComponent* sourceMesh =
                    scene.GetComponent<
                    MeshComponent
                    >(entity);

                if (sourceMesh != nullptr)
                {
                    scene.AddComponent<
                        MeshComponent
                    >(
                        duplicate,
                        *sourceMesh
                    );
                }

                /*
                 * Color.
                 */
                ColorComponent* sourceColor =
                    scene.GetComponent<
                    ColorComponent
                    >(entity);

                if (sourceColor != nullptr)
                {
                    scene.AddComponent<
                        ColorComponent
                    >(
                        duplicate,
                        *sourceColor
                    );
                }

                /*
                 * Player.
                 */
                PlayerComponent* sourcePlayer =
                    scene.GetComponent<
                    PlayerComponent
                    >(entity);

                if (sourcePlayer != nullptr)
                {
                    scene.AddComponent<
                        PlayerComponent
                    >(
                        duplicate,
                        *sourcePlayer
                    );
                }

                /*
                 * Character Controller.
                 */
                CharacterControllerComponent*
                    sourceController =
                    scene.GetComponent<
                    CharacterControllerComponent
                    >(entity);

                if (sourceController != nullptr)
                {
                    scene.AddComponent<
                        CharacterControllerComponent
                    >(
                        duplicate,
                        *sourceController
                    );
                }

                /*
                 * Light.
                 */
                LightComponent* sourceLight =
                    scene.GetComponent<
                    LightComponent
                    >(entity);

                if (sourceLight != nullptr)
                {
                    scene.AddComponent<
                        LightComponent
                    >(
                        duplicate,
                        *sourceLight
                    );
                }

                /*
                 * Collider.
                 */
                ColliderComponent* sourceCollider =
                    scene.GetComponent<
                    ColliderComponent
                    >(entity);

                if (sourceCollider != nullptr)
                {
                    scene.AddComponent<
                        ColliderComponent
                    >(
                        duplicate,
                        *sourceCollider
                    );
                }

                /*
                 * Texture.
                 */
                TextureComponent* sourceTexture =
                    scene.GetComponent<
                    TextureComponent
                    >(entity);

                if (sourceTexture != nullptr)
                {
                    scene.AddComponent<
                        TextureComponent
                    >(
                        duplicate,
                        *sourceTexture
                    );
                }

                /*
                 * Name.
                 */
                NameComponent* sourceName =
                    scene.GetComponent<
                    NameComponent
                    >(entity);

                NameComponent* duplicateName =
                    scene.GetComponent<
                    NameComponent
                    >(duplicate);

                if (duplicateName != nullptr)
                {
                    std::string baseName =
                        sourceName != nullptr &&
                        !sourceName->name.empty()
                        ? sourceName->name + " Copy"
                        : "Entity Copy";

                    duplicateName->name =
                        MakeUniqueName(
                            scene,
                            baseName,
                            duplicate
                        );
                }

                /*
                 * Preserve folder membership.
                 */
                HierarchyFolder* sourceFolder =
                    FindFolderContainingEntity(
                        m_HierarchyFolders,
                        entity.GetID()
                    );

                if (sourceFolder != nullptr)
                {
                    sourceFolder->entities.push_back(
                        duplicate.GetID()
                    );
                }

                m_SelectedEntity =
                    duplicate;

                Logger::Info(
                    std::string(
                        "Duplicated Entity "
                    ) +
                    std::to_string(
                        entity.GetID()
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
                    entity.GetID();

                RemoveEntityFromFolders(
                    m_HierarchyFolders,
                    deletedID
                );

                scene.DestroyEntity(
                    entity
                );

                if (m_SelectedEntity.GetID() ==
                    deletedID)
                {
                    m_SelectedEntity =
                        Entity();

                    m_NameEditEntityID =
                        0;

                    m_NameEditBuffer[0] =
                        '\0';
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

        ImGui::PopID();
    }

    /*
     * Root drop target.
     */
    ImVec2 available =
        ImGui::GetContentRegionAvail();

    if (available.y < 20.0f)
    {
        available.y = 20.0f;
    }

    ImGui::InvisibleButton(
        "##RootDropArea",
        ImVec2(
            available.x,
            available.y
        )
    );

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
                    m_HierarchyFolders,
                    entityID
                );
            }
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}