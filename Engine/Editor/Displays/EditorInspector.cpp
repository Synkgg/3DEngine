#include "../Editor.h"
#include "../UI/UIEditor.h"

#include "../../Scene/Scene.h"
#include "../../Scene/PrefabSerializer.h"

#include "../../Scene/Components/TransformComponent.h"
#include "../../Scene/Components/MeshComponent.h"
#include "../../Scene/Components/ColorComponent.h"
#include "../../Scene/Components/NameComponent.h"
#include "../../Scene/Components/PawnComponent.h"
#include "../../Scene/Components/PlayerStartComponent.h"
#include "../../Scene/Components/CharacterControllerComponent.h"
#include "../../Scene/Components/LightComponent.h"
#include "../../Scene/Components/CameraComponent.h"
#include "../../Scene/Components/ColliderComponent.h"
#include "../../Scene/Components/TextureComponent.h"
#include "../../Scene/Components/MaterialComponent.h"
#include "../../Scene/Components/ScriptComponent.h"
#include "../../SCene/Components/InteractableComponent.h"

#include "../../Graphics/PrimitiveType.h"
#include "../../Graphics/Renderer.h"
#include "../../Core/Logger.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include <fstream>
#include <sstream>
#include <iterator>
#include <unordered_map>

namespace
{
    constexpr float DegreesToRadians = 0.0174532925f;

    constexpr float RadiansToDegrees = 57.2957795f;

    std::vector<std::filesystem::path> FindLuaScripts()
    {
        namespace fs = std::filesystem;

        std::vector<fs::path> scripts;

        const fs::path assetsRoot =
            fs::current_path() / "Assets";

        std::error_code error;

        if (!fs::exists(
            assetsRoot,
            error))
        {
            return scripts;
        }

        for (const fs::directory_entry& entry :
            fs::recursive_directory_iterator(
                assetsRoot,
                error))
        {
            if (error)
            {
                break;
            }

            if (!entry.is_regular_file())
            {
                continue;
            }

            std::string extension =
                entry.path().extension().string();

            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(
                        std::tolower(character)
                        );
                }
            );

            if (extension == ".lua")
            {
                scripts.push_back(
                    entry.path()
                );
            }
        }

        std::sort(
            scripts.begin(),
            scripts.end()
        );

        return scripts;
    }
    std::vector<std::filesystem::path> FindAssets(
        std::initializer_list<const char*> extensions)
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> assets;
        const fs::path assetsRoot = fs::current_path() / "Assets";
        std::error_code error;

        if (!fs::exists(assetsRoot, error))
            return assets;

        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(assetsRoot, error))
        {
            if (error)
                break;
            if (!entry.is_regular_file())
                continue;

            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            bool accepted = false;
            for (const char* allowed : extensions)
            {
                if (extension == allowed)
                {
                    accepted = true;
                    break;
                }
            }

            if (accepted)
                assets.push_back(entry.path());
        }

        std::sort(assets.begin(), assets.end());
        return assets;
    }

    bool DrawAssetPicker(
        const char* label,
        std::string& value,
        std::initializer_list<const char*> extensions)
    {
        const std::vector<std::filesystem::path> assets = FindAssets(extensions);
        const std::string preview = value.empty()
            ? "None"
            : std::filesystem::path(value).filename().string();

        bool changed = false;
        if (ImGui::BeginCombo(label, preview.c_str()))
        {
            static char search[128] = {};
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##AssetSearch", "Search assets...", search, sizeof(search));
            ImGui::Separator();

            if (ImGui::Selectable("None", value.empty()))
            {
                value.clear();
                changed = true;
            }

            std::string query = search;
            std::transform(query.begin(), query.end(), query.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            for (const auto& path : assets)
            {
                std::error_code error;
                const std::string assetPath =
                    std::filesystem::relative(path, std::filesystem::current_path(), error).generic_string();
                std::string searchable = assetPath;
                std::transform(searchable.begin(), searchable.end(), searchable.begin(),
                    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (!query.empty() && searchable.find(query) == std::string::npos)
                    continue;

                const bool selected = value == assetPath;
                ImGui::PushID(assetPath.c_str());
                if (ImGui::Selectable(path.filename().string().c_str(), selected))
                {
                    value = assetPath;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", assetPath.c_str());
                ImGui::PopID();
            }

            if (assets.empty())
                ImGui::TextDisabled("No matching assets under Assets.");

            ImGui::EndCombo();
        }
        return changed;
    }

    bool DrawTransformVector3(const char* label, float values[3], float speed)
    {
        ImGui::PushID(label);
        const float labelWidth = 72.0f;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(labelWidth);

        const float spacing = 4.0f;
        const float axisLabelWidth = 12.0f;
        const float available = ImGui::GetContentRegionAvail().x;
        const float fieldWidth = std::max(36.0f, (available - (axisLabelWidth * 3.0f) - (spacing * 5.0f)) / 3.0f);
        const ImVec4 axisColors[3] = {
            ImVec4(0.90f, 0.28f, 0.28f, 1.0f),
            ImVec4(0.32f, 0.78f, 0.38f, 1.0f),
            ImVec4(0.30f, 0.52f, 0.95f, 1.0f)
        };
        const char* axes[3] = { "X", "Y", "Z" };

        bool changed = false;
        for (int i = 0; i < 3; ++i)
        {
            if (i > 0) ImGui::SameLine(0.0f, spacing);
            ImGui::TextColored(axisColors[i], "%s", axes[i]);
            ImGui::SameLine(0.0f, spacing);
            ImGui::SetNextItemWidth(fieldWidth);
            ImGui::PushID(i);
            changed |= ImGui::DragFloat("##Value", &values[i], speed, 0.0f, 0.0f, "%.3f");
            ImGui::PopID();
        }
        ImGui::PopID();
        return changed;
    }

    std::string g_RemoveComponentRequest;

    struct ComponentHeaderResult
    {
        bool open = true;
        bool removeRequested = false;
    };

    ComponentHeaderResult DrawComponentHeader(const char* label, bool removable = true, bool defaultOpen = true)
    {
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        ComponentHeaderResult result;
        ImGui::PushID(label);
        static std::unordered_map<ImGuiID, bool> openStates;
        const ImGuiID id = ImGui::GetID("##ComponentHeader");
        auto it = openStates.find(id);
        if (it == openStates.end()) it = openStates.emplace(id, defaultOpen).first;

        const float inset = 2.0f;
        const float height = ImGui::GetTextLineHeight() + 8.0f;
        ImVec2 p = ImGui::GetCursorScreenPos(); p.x += inset;
        const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x - inset * 2.0f);
        const float menuWidth = removable ? 26.0f : 0.0f;

        ImGui::SetCursorScreenPos(p);
        ImGui::InvisibleButton("##ComponentHeader", ImVec2(width - menuWidth, height));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) it->second = !it->second;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 bg = ImGui::GetColorU32(hovered ? ImVec4(0.205f,0.215f,0.228f,1.0f) : ImVec4(0.160f,0.168f,0.178f,1.0f));
        draw->AddRectFilled(p, ImVec2(p.x + width, p.y + height), bg);
        draw->AddLine(ImVec2(p.x,p.y+height),ImVec2(p.x+width,p.y+height),ImGui::GetColorU32(ImVec4(0.235f,0.245f,0.258f,1.0f)));

        const float cy=p.y+height*0.5f, ax=p.x+10.0f;
        if(it->second) draw->AddTriangleFilled(ImVec2(ax-4,cy-2),ImVec2(ax+4,cy-2),ImVec2(ax,cy+3),ImGui::GetColorU32(ImGuiCol_Text));
        else draw->AddTriangleFilled(ImVec2(ax-2,cy-4),ImVec2(ax-2,cy+4),ImVec2(ax+3,cy),ImGui::GetColorU32(ImGuiCol_Text));
        draw->AddText(ImVec2(p.x+24.0f,p.y+4.0f),ImGui::GetColorU32(ImVec4(0.84f,0.85f,0.87f,1.0f)),label);

        if(removable)
        {
            ImGui::SetCursorScreenPos(ImVec2(p.x+width-menuWidth,p.y));
            ImGui::InvisibleButton("##ComponentMenuButton",ImVec2(menuWidth,height));
            const bool menuHovered=ImGui::IsItemHovered();
            if(ImGui::IsItemClicked()) ImGui::OpenPopup("##ComponentMenu");

            // Borderless vertical ellipsis. Only the dots are visible; hover
            // gets a subtle background so the hit target remains discoverable.
            if(menuHovered)
                draw->AddRectFilled(
                    ImVec2(p.x+width-menuWidth+2.0f,p.y+2.0f),
                    ImVec2(p.x+width-2.0f,p.y+height-2.0f),
                    ImGui::GetColorU32(ImVec4(0.225f,0.235f,0.248f,1.0f)),2.0f);
            const float dotX=p.x+width-menuWidth*0.5f;
            const float dotY=p.y+height*0.5f;
            const ImU32 dotColor=ImGui::GetColorU32(ImGuiCol_Text);
            draw->AddCircleFilled(ImVec2(dotX,dotY-4.0f),1.3f,dotColor);
            draw->AddCircleFilled(ImVec2(dotX,dotY),1.3f,dotColor);
            draw->AddCircleFilled(ImVec2(dotX,dotY+4.0f),1.3f,dotColor);
            if(ImGui::BeginPopup("##ComponentMenu"))
            {
                if(ImGui::MenuItem("Remove Component")) { result.removeRequested=true; g_RemoveComponentRequest=label; }
                ImGui::EndPopup();
            }
        }
        result.open=it->second;
        ImGui::SetCursorScreenPos(ImVec2(p.x-inset,p.y+height+5.0f));
        ImGui::PopID();
        return result;
    }

    void EndComponentSection()
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p.x + 2.0f, p.y + 2.0f),
            ImVec2(p.x + ImGui::GetContentRegionAvail().x - 2.0f, p.y + 2.0f),
            ImGui::GetColorU32(ImVec4(0.20f, 0.21f, 0.23f, 1.0f)));
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
    }

}

void Editor::RenderInspector(
    Renderer& renderer,
    Scene& scene)
{
    ImGui::Begin("Details");

    ImGui::TextDisabled("DETAILS");
    ImGui::Separator();

    if (m_UIEditor && m_UIEditor->HasSelectedWidget())
    {
        m_UIEditor->DrawSelectedInspector();
        ImGui::End();
        return;
    }

    if (!m_SelectedEntity.IsValid())
    {
        m_NameEditEntityID = 0;
        m_NameEditBuffer[0] = '\0';

        ImGui::TextDisabled(
            "Nothing selected"
        );

        ImGui::End();

        return;
    }

    ImGui::Text(
        "Entity %u",
        m_SelectedEntity.GetID()
    );

    ImGui::Separator();

    /*
     * Name
     */
    NameComponent* name =
        scene.GetComponent<NameComponent>(
            m_SelectedEntity
        );

    if (name != nullptr)
    {
        if (m_NameEditEntityID !=
            m_SelectedEntity.GetID())
        {
            std::snprintf(
                m_NameEditBuffer,
                sizeof(m_NameEditBuffer),
                "%s",
                name->name.c_str()
            );

            m_NameEditEntityID =
                m_SelectedEntity.GetID();
        }

        if (ImGui::InputText(
            "Name",
            m_NameEditBuffer,
            sizeof(m_NameEditBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            name->name =
                m_NameEditBuffer;
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Relationship", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Entity parent = scene.GetParent(m_SelectedEntity);
        if (parent.IsValid())
        {
            NameComponent* parentName = scene.GetComponent<NameComponent>(parent);
            ImGui::Text("Parent: %s", parentName ? parentName->name.c_str() : "(Unnamed)");
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear Parent"))
                scene.ClearParent(m_SelectedEntity, true);
        }
        else
        {
            ImGui::TextDisabled("Parent: None");
        }
        ImGui::TextDisabled("Drag an entity onto another entity in Hierarchy to parent it.");
    }

    if (PrefabSerializer::IsInstanceRoot(scene, m_SelectedEntity))
    {
        if (ImGui::CollapsingHeader("Prefab", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::string source = PrefabSerializer::GetSource(scene, m_SelectedEntity);
            ImGui::TextDisabled("INSTANCE");
            ImGui::TextWrapped("%s", source.c_str());
            if (ImGui::Button("Apply"))
                PrefabSerializer::Apply(scene, m_SelectedEntity);
            ImGui::SameLine();
            if (ImGui::Button("Revert"))
            {
                PrefabSerializer::Revert(scene, m_SelectedEntity);
                m_SelectedEntity = Entity();
            }
            ImGui::SameLine();
            if (ImGui::Button("Unpack"))
                PrefabSerializer::Unpack(scene, m_SelectedEntity, true);
            ImGui::Separator();
        }
    }

    // Inspector-only component surfaces: strong headers, dark field bodies and
    // visible outlines create distinct cards without changing the global theme.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.225f,0.230f,0.238f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.255f,0.262f,0.272f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.275f,0.285f,0.300f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.105f,0.110f,0.118f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.245f,0.252f,0.265f,1.0f));

    /*
     * Transform
     */
    TransformComponent* transform =
        scene.GetComponent<
        TransformComponent
        >(m_SelectedEntity);

    if (transform != nullptr &&
        DrawComponentHeader("Transform", false).open)
    {
        Vec3& position =
            transform->transform.position;

        Vec3& rotation =
            transform->transform.rotation;

        Vec3& scale =
            transform->transform.scale;

        float positionValues[3] =
        {
            position.x,
            position.y,
            position.z
        };

        if (DrawTransformVector3("Position", positionValues, 0.05f))
        {
            position.x =
                positionValues[0];

            position.y =
                positionValues[1];

            position.z =
                positionValues[2];
        }

        float rotationValues[3] =
        {
            rotation.x *
                RadiansToDegrees,

            rotation.y *
                RadiansToDegrees,

            rotation.z *
                RadiansToDegrees
        };

        if (DrawTransformVector3("Rotation", rotationValues, 1.0f))
        {
            rotation.x =
                rotationValues[0] *
                DegreesToRadians;

            rotation.y =
                rotationValues[1] *
                DegreesToRadians;

            rotation.z =
                rotationValues[2] *
                DegreesToRadians;
        }

        float scaleValues[3] =
        {
            scale.x,
            scale.y,
            scale.z
        };

        if (DrawTransformVector3("Scale", scaleValues, 0.05f))
        {
            scale.x =
                scaleValues[0];

            scale.y =
                scaleValues[1];

            scale.z =
                scaleValues[2];
        }
    }

    /*
     * Mesh
     */
    MeshComponent* mesh =
        scene.GetComponent<
        MeshComponent
        >(m_SelectedEntity);

    if (mesh != nullptr)
    {
        if (DrawComponentHeader("Mesh").open)
        {
            const char* primitiveNames[] =
            {
                "None",
                "Cube",
                "Sphere",
                "Plane",
                "Cylinder"
            };

            int primitiveIndex =
                static_cast<int>(
                    mesh->primitive
                    );

            if (ImGui::Combo(
                "Primitive",
                &primitiveIndex,
                primitiveNames,
                IM_ARRAYSIZE(
                    primitiveNames
                )))
            {
                mesh->primitive =
                    static_cast<
                    PrimitiveType
                    >(
                        primitiveIndex
                        );
            }

            ImGui::Separator();
            ImGui::TextDisabled("External Model");

            if (DrawAssetPicker("Model", mesh->modelPath, { ".obj" }))
            {
                if (!mesh->modelPath.empty())
                {
                    mesh->primitive = PrimitiveType::None;
                    Logger::Info(std::string("Assigned model: ") + mesh->modelPath);
                }
            }

            ImGui::Checkbox("Owner No See", &mesh->ownerNoSee);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Hide this mesh for the local Player during runtime; remote players still see it.");

            ImGui::Separator();

            ImGui::Text("Mesh Offset");

            ImGui::DragFloat3(
                "Offset",
                &mesh->offset.x,
                0.01f
            );

            if (ImGui::Button("Reset Offset"))
            {
                mesh->offset = Vec3(0.0f, 0.0f, 0.0f);
            }

            float meshRotationDegrees[3] = {
                mesh->rotation.x * RadiansToDegrees,
                mesh->rotation.y * RadiansToDegrees,
                mesh->rotation.z * RadiansToDegrees
            };
            if (ImGui::DragFloat3("Mesh Rotation", meshRotationDegrees, 1.0f))
            {
                mesh->rotation.x = meshRotationDegrees[0] * DegreesToRadians;
                mesh->rotation.y = meshRotationDegrees[1] * DegreesToRadians;
                mesh->rotation.z = meshRotationDegrees[2] * DegreesToRadians;
            }
        }
    }

    /*
     * Color
     */
    ColorComponent* color =
        scene.GetComponent<
        ColorComponent
        >(m_SelectedEntity);

    if (color != nullptr)
    {
        if (DrawComponentHeader("Color").open)
        {
            float colorValues[4] =
            {
                color->r,
                color->g,
                color->b,
                color->a
            };

            if (ImGui::ColorEdit4(
                "Color",
                colorValues))
            {
                color->r =
                    colorValues[0];

                color->g =
                    colorValues[1];

                color->b =
                    colorValues[2];

                color->a =
                    colorValues[3];
            }
        }
    }

    /*
     * Texture
     */
    TextureComponent* texture =
        scene.GetComponent<
        TextureComponent
        >(m_SelectedEntity);

    if (texture != nullptr)
    {
        if (DrawComponentHeader("Texture").open)
        {
            if (DrawAssetPicker("Asset", texture->path,
                { ".png", ".jpg", ".jpeg", ".bmp", ".tga" }))
            {
                if (!texture->path.empty())
                    Logger::Info(std::string("Assigned texture: ") + texture->path);
            }
        }
    }

    /* Player Start */
    PlayerStartComponent* playerStart=scene.GetComponent<PlayerStartComponent>(m_SelectedEntity);
    if(playerStart&&DrawComponentHeader("Player Start").open){int slot=(int)playerStart->slot;if(ImGui::InputInt("Player Slot",&slot))playerStart->slot=(std::uint32_t)std::max(0,slot);ImGui::TextWrapped("Generic Pawn spawn location. Slot 0 is a default/any start.");}
    /*
     * Pawn
     */
    PawnComponent* pawn = scene.GetComponent<PawnComponent>(m_SelectedEntity);
    if (pawn != nullptr)
    {
        if (DrawComponentHeader("Pawn").open)
        {
            ImGui::TextWrapped("Generic controllable entity. Input, camera and gameplay behavior are project-defined.");
            ImGui::Text("Controller ID: %u", pawn->controllerID);
        }
    }

    /*
     * Character Controller
     */
    CharacterControllerComponent* controller =
        scene.GetComponent<
        CharacterControllerComponent
        >(m_SelectedEntity);

    if (controller != nullptr)
    {
        if (DrawComponentHeader("Character Controller").open)
        {
            ImGui::DragFloat(
                "Gravity",
                &controller->gravity,
                0.1f,
                0.0f,
                100.0f
            );

            ImGui::DragFloat(
                "Jump Force",
                &controller->jumpForce,
                0.1f,
                0.0f,
                100.0f
            );

            ImGui::Text(
                "Grounded: %s",
                controller->grounded
                ? "Yes"
                : "No"
            );

            ImGui::Text(
                "Vertical Velocity: %.2f",
                controller->verticalVelocity
            );
        }
    }

    CameraComponent* camera = scene.GetComponent<CameraComponent>(m_SelectedEntity);
    if (camera != nullptr)
    {
        if (DrawComponentHeader("Camera").open)
        {
            if (ImGui::InputFloat("Field of View", &camera->fieldOfView, 0.0f, 0.0f, "%.1f"))
                camera->fieldOfView = std::clamp(camera->fieldOfView, 30.0f, 120.0f);
            if (ImGui::InputFloat("Near Clip", &camera->nearClip, 0.0f, 0.0f, "%.3f"))
                camera->nearClip = std::clamp(camera->nearClip, 0.01f, 10.0f);
            if (ImGui::InputFloat("Far Clip", &camera->farClip, 0.0f, 0.0f, "%.1f"))
                camera->farClip = std::clamp(camera->farClip, 10.0f, 10000.0f);
            if (camera->farClip <= camera->nearClip) camera->farClip = camera->nearClip + 1.0f;
            if (ImGui::Checkbox("Active", &camera->active) && camera->active)
                for (const Entity& other : scene.GetEntities())
                    if (other.GetID() != m_SelectedEntity.GetID())
                        if (auto* otherCamera = scene.GetComponent<CameraComponent>(other)) otherCamera->active = false;
            ImGui::TextWrapped("The active Camera entity supplies the runtime view using its world transform.");
        }
    }

    /*
     * Directional Light
     */
    LightComponent* light =
        scene.GetComponent<
        LightComponent
        >(m_SelectedEntity);

    if (light != nullptr)
    {
        if (DrawComponentHeader("Light").open)
        {
            int lightType = static_cast<int>(light->type);
            const char* lightTypes[] = { "Directional", "Point", "Spot" };
            if (ImGui::Combo("Type", &lightType, lightTypes, 3))
                light->type = static_cast<LightType>(lightType);
            float colorValues[3] =
            {
                light->color.x,
                light->color.y,
                light->color.z
            };

            if (ImGui::ColorEdit3(
                "Color",
                colorValues))
            {
                light->color.x =
                    colorValues[0];

                light->color.y =
                    colorValues[1];

                light->color.z =
                    colorValues[2];
            }

            float directionValues[3] =
            {
                light->direction.x,
                light->direction.y,
                light->direction.z
            };

            if (ImGui::DragFloat3(
                "Direction",
                directionValues,
                0.05f))
            {
                light->direction.x =
                    directionValues[0];

                light->direction.y =
                    directionValues[1];

                light->direction.z =
                    directionValues[2];
            }

            ImGui::SliderFloat(
                "Intensity",
                &light->intensity,
                0.0f,
                10.0f
            );

            if (light->type != LightType::Directional)
                ImGui::DragFloat("Range", &light->range, 0.25f, 0.1f, 100.0f);

            if (light->type == LightType::Spot)
            {
                ImGui::SliderFloat("Inner Angle", &light->innerAngle, 1.0f, 85.0f);
                ImGui::SliderFloat("Outer Angle", &light->outerAngle, 1.0f, 89.0f);
                if (light->outerAngle < light->innerAngle)
                    light->outerAngle = light->innerAngle;
            }

            ImGui::Checkbox("Cast Shadows", &light->castShadows);
        }
    }

    MaterialComponent* material =
        scene.GetComponent<MaterialComponent>(m_SelectedEntity);

    if (material != nullptr)
    {
        if (DrawComponentHeader("Material").open)
        {
            ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &material->roughness, 0.04f, 1.0f);
            ImGui::SliderFloat("Ambient Occlusion", &material->ambientOcclusion, 0.0f, 1.0f);
            ImGui::DragFloat("Emissive", &material->emissive, 0.05f, 0.0f, 20.0f);
        }
    }

    /*
     * Collider
     */
    ColliderComponent* collider =
        scene.GetComponent<
        ColliderComponent
        >(m_SelectedEntity);

    if (collider != nullptr)
    {
        if (DrawComponentHeader("Collider").open)
        {
            ImGui::Checkbox(
                "Enabled",
                &collider->enabled
            );

            ImGui::Spacing();

            ImGui::DragFloat(
                "Width",
                &collider->width,
                0.05f,
                0.01f,
                100.0f
            );

            ImGui::DragFloat(
                "Height",
                &collider->height,
                0.05f,
                0.01f,
                100.0f
            );

            ImGui::DragFloat(
                "Depth",
                &collider->depth,
                0.05f,
                0.01f,
                100.0f
            );
        }
    }

    InteractableComponent* interactable =
        scene.GetComponent<InteractableComponent>(m_SelectedEntity);

    if (interactable != nullptr)
    {
        if (DrawComponentHeader("Interactable").open)
        {
            ImGui::PushID("InteractableComponent");

            ImGui::Checkbox(
                "Enabled",
                &interactable->enabled
            );

            char promptBuffer[256];

            std::strncpy(
                promptBuffer,
                interactable->prompt.c_str(),
                sizeof(promptBuffer)
            );

            promptBuffer[
                sizeof(promptBuffer) - 1
            ] = '\0';

            if (ImGui::InputText(
                "Prompt",
                promptBuffer,
                sizeof(promptBuffer)))
            {
                interactable->prompt =
                    promptBuffer;
            }

            ImGui::PopID();
        }
    }

    /*
  * Scripts
  */
    ScriptComponent* script =
        scene.GetComponent<
        ScriptComponent
        >(m_SelectedEntity);

    if (script != nullptr)
    {
        if (DrawComponentHeader("Script").open)
        {
            if (script->scriptNames.empty())
            {
                ImGui::TextDisabled(
                    "No scripts assigned"
                );
            }
            else
            {
                for (std::size_t i = 0;
                    i < script->scriptNames.size();
                    ++i)
                {
                    ImGui::PushID(
                        static_cast<int>(i)
                    );

                    std::filesystem::path scriptPath =
                        script->scriptNames[i];

                    ImGui::Text(
                        "%s",
                        scriptPath.filename().string().c_str()
                    );

                    if (!scriptPath.empty())
                    {
                        ImGui::SameLine();

                        ImGui::TextDisabled(
                            "%s",
                            script->scriptNames[i].c_str()
                        );
                    }

                    // Editor-facing Lua properties are declared in the script:
                    // Properties = { Speed = 2.0, Enabled = true, Target = Entity(0), Label = "..." }
                    // The Inspector stores per-entity overrides without modifying the Lua source.
                    {
                        std::ifstream propertyFile(script->scriptNames[i]);
                        std::string source((std::istreambuf_iterator<char>(propertyFile)), std::istreambuf_iterator<char>());
                        const std::size_t propertiesPos = source.find("Properties");
                        const std::size_t openBrace = propertiesPos == std::string::npos ? std::string::npos : source.find('{', propertiesPos);
                        const std::size_t closeBrace = openBrace == std::string::npos ? std::string::npos : source.find('}', openBrace);
                        if (openBrace != std::string::npos && closeBrace != std::string::npos)
                        {
                            std::string body = source.substr(openBrace + 1, closeBrace - openBrace - 1);
                            std::istringstream propertyStream(body);
                            std::string declaration;
                            while (std::getline(propertyStream, declaration, ','))
                            {
                                const std::size_t equals = declaration.find('=');
                                if (equals == std::string::npos) continue;
                                auto trim=[](std::string value){ const auto first=value.find_first_not_of(" \t\r
"); const auto last=value.find_last_not_of(" \t\r
"); return first==std::string::npos?std::string():value.substr(first,last-first+1); };
                                const std::string propertyName=trim(declaration.substr(0,equals));
                                const std::string defaultValue=trim(declaration.substr(equals+1));
                                if(propertyName.empty()||defaultValue.empty()) continue;
                                auto& values=script->properties[script->scriptNames[i]];
                                auto valueIt=values.find(propertyName);
                                if(valueIt==values.end())
                                {
                                    ScriptPropertyValue value;
                                    if(defaultValue=="true"||defaultValue=="false"){ value.type=ScriptPropertyType::Boolean; value.value=defaultValue; }
                                    else if(defaultValue.rfind("Entity(",0)==0){ value.type=ScriptPropertyType::Entity; value.value="0"; }
                                    else if(defaultValue.front()=='"'||defaultValue.front()=='\''){ value.type=ScriptPropertyType::String; value.value=defaultValue.substr(1,defaultValue.size()>1?defaultValue.size()-2:0); }
                                    else { value.type=ScriptPropertyType::Number; value.value=defaultValue; }
                                    valueIt=values.emplace(propertyName,value).first;
                                }
                                ScriptPropertyValue& value=valueIt->second;
                                ImGui::PushID(propertyName.c_str());
                                if(value.type==ScriptPropertyType::Boolean)
                                {
                                    bool v=value.value=="true"||value.value=="1";
                                    if(ImGui::Checkbox(propertyName.c_str(),&v)) value.value=v?"true":"false";
                                }
                                else if(value.type==ScriptPropertyType::Number)
                                {
                                    float v=0.0f; try{v=std::stof(value.value);}catch(...){}
                                    if(ImGui::DragFloat(propertyName.c_str(),&v,0.05f)) value.value=std::to_string(v);
                                }
                                else if(value.type==ScriptPropertyType::Entity)
                                {
                                    std::uint32_t current=0; try{current=(std::uint32_t)std::stoul(value.value);}catch(...){}
                                    const Entity currentEntity=scene.FindEntityByID(current);
                                    const NameComponent* currentName=currentEntity.IsValid()?scene.GetComponent<NameComponent>(currentEntity):nullptr;
                                    const std::string preview=currentName?currentName->name:"None";
                                    if(ImGui::BeginCombo(propertyName.c_str(),preview.c_str()))
                                    {
                                        if(ImGui::Selectable("None",current==0)) value.value="0";
                                        for(Entity candidate:scene.GetEntities())
                                        {
                                            const NameComponent* candidateName=scene.GetComponent<NameComponent>(candidate);
                                            if(candidateName && ImGui::Selectable(candidateName->name.c_str(),candidate.GetID()==current))
                                                value.value=std::to_string(candidate.GetID());
                                        }
                                        ImGui::EndCombo();
                                    }
                                }
                                else
                                {
                                    char valueBuffer[256]; std::snprintf(valueBuffer,sizeof(valueBuffer),"%s",value.value.c_str());
                                    if(ImGui::InputText(propertyName.c_str(),valueBuffer,sizeof(valueBuffer))) value.value=valueBuffer;
                                }
                                ImGui::PopID();
                            }
                        }
                    }

                    ImGui::SameLine();

                    if (ImGui::SmallButton(
                        "Remove##AttachedScript"))
                    {
                        Logger::Info(
                            std::string(
                                "Removed script: "
                            ) +
                            script->scriptNames[i]
                        );

                        script->scriptNames.erase(
                            script->scriptNames.begin() +
                            static_cast<std::ptrdiff_t>(i)
                        );

                        ImGui::PopID();

                        break;
                    }

                    ImGui::PopID();
                }
            }

            ImGui::Spacing();

            if (ImGui::Button(
                "Add Script"))
            {
                ImGui::OpenPopup(
                    "AddScriptPopup"
                );
            }

            if (ImGui::BeginPopup(
                "AddScriptPopup"))
            {
                const std::vector<std::filesystem::path>
                    luaScripts =
                    FindLuaScripts();

                if (luaScripts.empty())
                {
                    ImGui::TextDisabled(
                        "No Lua scripts found."
                    );
                }
                else
                {
                    for (const std::filesystem::path& path :
                        luaScripts)
                    {
                        const std::string assetPath =
                            std::filesystem::relative(
                                path,
                                std::filesystem::current_path()
                            ).generic_string();

                        const bool alreadyAssigned =
                            std::find(
                                script->scriptNames.begin(),
                                script->scriptNames.end(),
                                assetPath
                            ) != script->scriptNames.end();

                        ImGui::PushID(
                            assetPath.c_str()
                        );

                        if (alreadyAssigned)
                        {
                            ImGui::BeginDisabled();

                            ImGui::Selectable(
                                path.filename().string().c_str()
                            );

                            ImGui::EndDisabled();
                        }
                        else
                        {
                            if (ImGui::Selectable(
                                path.filename().string().c_str()))
                            {
                                script->scriptNames.push_back(
                                    assetPath
                                );

                                Logger::Info(
                                    std::string(
                                        "Added Lua script: "
                                    ) +
                                    assetPath
                                );

                                ImGui::CloseCurrentPopup();
                            }
                        }

                        ImGui::PopID();
                    }
                }

                ImGui::EndPopup();
            }
        }
    }

    if (!g_RemoveComponentRequest.empty())
    {
        const std::string remove = g_RemoveComponentRequest;
        g_RemoveComponentRequest.clear();
        if (remove == "Mesh") scene.RemoveComponent<MeshComponent>(m_SelectedEntity);
        else if (remove == "Color") scene.RemoveComponent<ColorComponent>(m_SelectedEntity);
        else if (remove == "Texture") scene.RemoveComponent<TextureComponent>(m_SelectedEntity);
        else if (remove == "Player Start") scene.RemoveComponent<PlayerStartComponent>(m_SelectedEntity);
        else if (remove == "Pawn") scene.RemoveComponent<PawnComponent>(m_SelectedEntity);
        else if (remove == "Character Controller") scene.RemoveComponent<CharacterControllerComponent>(m_SelectedEntity);
        else if (remove == "Camera") scene.RemoveComponent<CameraComponent>(m_SelectedEntity);
        else if (remove == "Light") scene.RemoveComponent<LightComponent>(m_SelectedEntity);
        else if (remove == "Material") scene.RemoveComponent<MaterialComponent>(m_SelectedEntity);
        else if (remove == "Collider") scene.RemoveComponent<ColliderComponent>(m_SelectedEntity);
        else if (remove == "Interactable") scene.RemoveComponent<InteractableComponent>(m_SelectedEntity);
        else if (remove == "Script") scene.RemoveComponent<ScriptComponent>(m_SelectedEntity);
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);

    /*
     * Add Component
     */
    const float addComponentWidth = 180.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - addComponentWidth) * 0.5f));
    if (ImGui::Button(
        "Add Component", ImVec2(addComponentWidth, 28.0f)))
    {
        ImGui::OpenPopup(
            "AddComponentPopup"
        );
    }

    ImGui::SetNextWindowSize(ImVec2(285.0f, 315.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        static char search[128] = {};
        if (ImGui::IsWindowAppearing()) { search[0] = '\0'; ImGui::SetKeyboardFocusHere(); }
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##ComponentSearch", "Search components...", search, sizeof(search));
        ImGui::Spacing();

        mesh=scene.GetComponent<MeshComponent>(m_SelectedEntity);
        color=scene.GetComponent<ColorComponent>(m_SelectedEntity);
        texture=scene.GetComponent<TextureComponent>(m_SelectedEntity);
        material=scene.GetComponent<MaterialComponent>(m_SelectedEntity);
        pawn=scene.GetComponent<PawnComponent>(m_SelectedEntity);
        camera=scene.GetComponent<CameraComponent>(m_SelectedEntity);
        playerStart=scene.GetComponent<PlayerStartComponent>(m_SelectedEntity);
        controller=scene.GetComponent<CharacterControllerComponent>(m_SelectedEntity);
        light=scene.GetComponent<LightComponent>(m_SelectedEntity);
        collider=scene.GetComponent<ColliderComponent>(m_SelectedEntity);
        script=scene.GetComponent<ScriptComponent>(m_SelectedEntity);
        interactable=scene.GetComponent<InteractableComponent>(m_SelectedEntity);

        std::string q=search;
        std::transform(q.begin(),q.end(),q.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        auto match=[&](const char* label){std::string v=label;std::transform(v.begin(),v.end(),v.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return q.empty()||v.find(q)!=std::string::npos;};
        auto item=[&](const char* label,bool exists,auto add){if(!match(label))return;ImGui::PushID(label);if(exists)ImGui::BeginDisabled();if(ImGui::Selectable(label,false,0,ImVec2(0,24))&&!exists){add();ImGui::CloseCurrentPopup();}if(exists)ImGui::EndDisabled();ImGui::PopID();};
        auto meshAdd=[&](){MeshComponent c;c.primitive=PrimitiveType::None;scene.AddComponent<MeshComponent>(m_SelectedEntity,c);Logger::Info("Added MeshComponent.");};
        auto materialAdd=[&](){scene.AddComponent<MaterialComponent>(m_SelectedEntity);Logger::Info("Added MaterialComponent.");};
        auto colorAdd=[&](){scene.AddComponent<ColorComponent>(m_SelectedEntity);Logger::Info("Added ColorComponent.");};
        auto textureAdd=[&](){scene.AddComponent<TextureComponent>(m_SelectedEntity);Logger::Info("Added TextureComponent.");};
        auto cameraAdd=[&](){scene.AddComponent<CameraComponent>(m_SelectedEntity);Logger::Info("Added CameraComponent.");};
        auto lightAdd=[&](){scene.AddComponent<LightComponent>(m_SelectedEntity);Logger::Info("Added LightComponent.");};
        auto colliderAdd=[&](){scene.AddComponent<ColliderComponent>(m_SelectedEntity);Logger::Info("Added ColliderComponent.");};
        auto controllerAdd=[&](){scene.AddComponent<CharacterControllerComponent>(m_SelectedEntity);Logger::Info("Added CharacterControllerComponent.");};
        auto pawnAdd=[&](){scene.AddComponent<PawnComponent>(m_SelectedEntity);Logger::Info("Added PawnComponent.");};
        auto startAdd=[&](){scene.AddComponent<PlayerStartComponent>(m_SelectedEntity);Logger::Info("Added PlayerStartComponent.");};
        auto interactAdd=[&](){scene.AddComponent<InteractableComponent>(m_SelectedEntity);Logger::Info("Added InteractableComponent.");};
        auto scriptAdd=[&](){scene.AddComponent<ScriptComponent>(m_SelectedEntity);Logger::Info("Added ScriptComponent.");};

        static int selectedCategory = 0;
        const char* categories[] = { "Rendering", "Physics", "Gameplay", "Scripting" };

        if(!q.empty())
        {
            ImGui::BeginChild("##SearchResults",ImVec2(0,0),false);
            item("Mesh",mesh!=nullptr,meshAdd); item("Material",material!=nullptr,materialAdd);
            item("Color",color!=nullptr,colorAdd); item("Texture",texture!=nullptr,textureAdd);
            item("Camera",camera!=nullptr,cameraAdd); item("Directional Light",light!=nullptr,lightAdd);
            item("Collider",collider!=nullptr,colliderAdd); item("Character Controller",controller!=nullptr,controllerAdd);
            item("Pawn",pawn!=nullptr,pawnAdd); item("Player Start",playerStart!=nullptr,startAdd);
            item("Interactable",interactable!=nullptr,interactAdd); item("Script",script!=nullptr,scriptAdd);
            ImGui::EndChild();
        }
        else
        {
            // Fixed two-pane picker: categories on the left, children/items on the right.
            // Unlike BeginMenu(), both panes are part of this popup and can never overlap it.
            const float gap = 6.0f;
            const float categoryWidth = 92.0f;
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float childWidth = std::max(100.0f, availableWidth - categoryWidth - gap);
            const float paneHeight = ImGui::GetContentRegionAvail().y;

            ImGui::BeginChild("##ComponentCategories",ImVec2(categoryWidth,paneHeight),true);
            for(int i=0;i<4;++i)
                if(ImGui::Selectable(categories[i],selectedCategory==i,0,ImVec2(0,24)))
                    selectedCategory=i;
            ImGui::EndChild();

            ImGui::SameLine(0.0f,gap);
            ImGui::BeginChild("##ComponentChildren",ImVec2(childWidth,paneHeight),true);
            switch(selectedCategory)
            {
                case 0:
                    item("Mesh",mesh!=nullptr,meshAdd); item("Material",material!=nullptr,materialAdd);
                    item("Color",color!=nullptr,colorAdd); item("Texture",texture!=nullptr,textureAdd);
                    item("Camera",camera!=nullptr,cameraAdd); item("Directional Light",light!=nullptr,lightAdd);
                    break;
                case 1:
                    item("Collider",collider!=nullptr,colliderAdd);
                    item("Character Controller",controller!=nullptr,controllerAdd);
                    break;
                case 2:
                    item("Pawn",pawn!=nullptr,pawnAdd); item("Player Start",playerStart!=nullptr,startAdd);
                    item("Interactable",interactable!=nullptr,interactAdd);
                    break;
                case 3:
                    item("Script",script!=nullptr,scriptAdd);
                    break;
            }
            ImGui::EndChild();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}