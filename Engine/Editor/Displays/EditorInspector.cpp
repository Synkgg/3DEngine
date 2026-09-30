#include "../Editor.h"

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

}

void Editor::RenderInspector(
    Renderer& renderer,
    Scene& scene)
{
    ImGui::Begin("Details");

    ImGui::TextDisabled("DETAILS");
    ImGui::Separator();

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
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.185f,0.190f,0.198f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.220f,0.228f,0.238f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.240f,0.250f,0.265f,1.0f));

    /*
     * Transform
     */
    TransformComponent* transform =
        scene.GetComponent<
        TransformComponent
        >(m_SelectedEntity);

    if (transform != nullptr &&
        ImGui::CollapsingHeader(
            "Transform",
            ImGuiTreeNodeFlags_DefaultOpen))
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

        if (ImGui::DragFloat3(
            "Position",
            positionValues,
            0.05f))
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

        if (ImGui::DragFloat3(
            "Rotation",
            rotationValues,
            1.0f))
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

        if (ImGui::DragFloat3(
            "Scale",
            scaleValues,
            0.05f))
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
        if (ImGui::CollapsingHeader(
            "Mesh",
            ImGuiTreeNodeFlags_DefaultOpen))
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

            ImGui::Spacing();

            if (ImGui::Button(
                "Remove Mesh"))
            {
                scene.RemoveComponent<
                    MeshComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Removed MeshComponent."
                );
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
        if (ImGui::CollapsingHeader(
            "Color",
            ImGuiTreeNodeFlags_DefaultOpen))
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

            ImGui::Spacing();

            if (ImGui::Button(
                "Remove Color"))
            {
                scene.RemoveComponent<
                    ColorComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Removed ColorComponent."
                );
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
        if (ImGui::CollapsingHeader(
            "Texture",
            ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawAssetPicker("Asset", texture->path,
                { ".png", ".jpg", ".jpeg", ".bmp", ".tga" }))
            {
                if (!texture->path.empty())
                    Logger::Info(std::string("Assigned texture: ") + texture->path);
            }

            ImGui::Spacing();

            if (ImGui::Button(
                "Remove Texture"))
            {
                scene.RemoveComponent<
                    TextureComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Removed TextureComponent."
                );
            }
        }
    }

    /* Player Start */
    PlayerStartComponent* playerStart=scene.GetComponent<PlayerStartComponent>(m_SelectedEntity);
    if(playerStart&&ImGui::CollapsingHeader("Player Start",ImGuiTreeNodeFlags_DefaultOpen)){int slot=(int)playerStart->slot;if(ImGui::InputInt("Player Slot",&slot))playerStart->slot=(std::uint32_t)std::max(0,slot);ImGui::TextWrapped("Generic Pawn spawn location. Slot 0 is a default/any start.");if(ImGui::Button("Remove Player Start"))scene.RemoveComponent<PlayerStartComponent>(m_SelectedEntity);}
    /*
     * Pawn
     */
    PawnComponent* pawn = scene.GetComponent<PawnComponent>(m_SelectedEntity);
    if (pawn != nullptr)
    {
        if (ImGui::CollapsingHeader("Pawn", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextWrapped("Generic controllable entity. Input, camera and gameplay behavior are project-defined.");
            ImGui::Text("Controller ID: %u", pawn->controllerID);
            ImGui::Spacing();
            if (ImGui::Button("Remove Pawn"))
            {
                scene.RemoveComponent<PawnComponent>(m_SelectedEntity);
                Logger::Info("Removed PawnComponent.");
            }
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
        if (ImGui::CollapsingHeader(
            "Character Controller",
            ImGuiTreeNodeFlags_DefaultOpen))
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

            ImGui::Spacing();

            if (ImGui::Button(
                "Remove Character Controller"))
            {
                scene.RemoveComponent<
                    CharacterControllerComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Removed CharacterControllerComponent."
                );
            }
        }
    }

    CameraComponent* camera = scene.GetComponent<CameraComponent>(m_SelectedEntity);
    if (camera != nullptr)
    {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Field of View", &camera->fieldOfView, 30.0f, 120.0f);
            ImGui::DragFloat("Near Clip", &camera->nearClip, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Far Clip", &camera->farClip, 1.0f, 10.0f, 10000.0f);
            if (camera->farClip <= camera->nearClip) camera->farClip = camera->nearClip + 1.0f;
            if (ImGui::Checkbox("Active", &camera->active) && camera->active)
                for (const Entity& other : scene.GetEntities())
                    if (other.GetID() != m_SelectedEntity.GetID())
                        if (auto* otherCamera = scene.GetComponent<CameraComponent>(other)) otherCamera->active = false;
            ImGui::TextWrapped("The active Camera entity supplies the runtime view using its world transform.");
            if (ImGui::Button("Remove Camera")) scene.RemoveComponent<CameraComponent>(m_SelectedEntity);
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
        if (ImGui::CollapsingHeader(
            "Light",
            ImGuiTreeNodeFlags_DefaultOpen))
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

            ImGui::Spacing();

            if (ImGui::Button(
                "Remove Light"))
            {
                scene.RemoveComponent<
                    LightComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Removed LightComponent."
                );
            }
        }
    }

    MaterialComponent* material =
        scene.GetComponent<MaterialComponent>(m_SelectedEntity);

    if (material != nullptr)
    {
        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &material->roughness, 0.04f, 1.0f);
            ImGui::SliderFloat("Ambient Occlusion", &material->ambientOcclusion, 0.0f, 1.0f);
            ImGui::DragFloat("Emissive", &material->emissive, 0.05f, 0.0f, 20.0f);
            if (ImGui::Button("Remove Material"))
                scene.RemoveComponent<MaterialComponent>(m_SelectedEntity);
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
        if (ImGui::CollapsingHeader(
            "Collider",
            ImGuiTreeNodeFlags_DefaultOpen))
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

            ImGui::Spacing();

            if (ImGui::Button(
                "Remove Collider"))
            {
                scene.RemoveComponent<
                    ColliderComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Removed ColliderComponent."
                );
            }
        }
    }

    InteractableComponent* interactable =
        scene.GetComponent<InteractableComponent>(m_SelectedEntity);

    if (interactable != nullptr)
    {
        if (ImGui::CollapsingHeader(
            "Interactable",
            ImGuiTreeNodeFlags_DefaultOpen))
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
        if (ImGui::CollapsingHeader(
            "Script##ScriptComponent",
            ImGuiTreeNodeFlags_DefaultOpen))
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
                                auto trim=[](std::string value){ const auto first=value.find_first_not_of(" \t\r\n"); const auto last=value.find_last_not_of(" \t\r\n"); return first==std::string::npos?std::string():value.substr(first,last-first+1); };
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

            ImGui::Spacing();

            if (ImGui::Button(
                "Remove Script Component"))
            {
                scene.RemoveComponent<
                    ScriptComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Removed ScriptComponent."
                );
            }
        }
    }

    ImGui::PopStyleColor(3);
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

        ImGui::BeginChild("##ComponentList",ImVec2(0,0),false);
        if(!q.empty())
        {
            item("Mesh",mesh!=nullptr,meshAdd); item("Material",material!=nullptr,materialAdd);
            item("Color",color!=nullptr,colorAdd); item("Texture",texture!=nullptr,textureAdd);
            item("Camera",camera!=nullptr,cameraAdd); item("Directional Light",light!=nullptr,lightAdd);
            item("Collider",collider!=nullptr,colliderAdd); item("Character Controller",controller!=nullptr,controllerAdd);
            item("Pawn",pawn!=nullptr,pawnAdd); item("Player Start",playerStart!=nullptr,startAdd);
            item("Interactable",interactable!=nullptr,interactAdd); item("Script",script!=nullptr,scriptAdd);
        }
        else
        {
            if(ImGui::BeginMenu("Rendering")){item("Mesh",mesh!=nullptr,meshAdd);item("Material",material!=nullptr,materialAdd);item("Color",color!=nullptr,colorAdd);item("Texture",texture!=nullptr,textureAdd);item("Camera",camera!=nullptr,cameraAdd);item("Directional Light",light!=nullptr,lightAdd);ImGui::EndMenu();}
            if(ImGui::BeginMenu("Physics")){item("Collider",collider!=nullptr,colliderAdd);item("Character Controller",controller!=nullptr,controllerAdd);ImGui::EndMenu();}
            if(ImGui::BeginMenu("Gameplay")){item("Pawn",pawn!=nullptr,pawnAdd);item("Player Start",playerStart!=nullptr,startAdd);item("Interactable",interactable!=nullptr,interactAdd);ImGui::EndMenu();}
            if(ImGui::BeginMenu("Scripting")){item("Script",script!=nullptr,scriptAdd);ImGui::EndMenu();}
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::End();
}