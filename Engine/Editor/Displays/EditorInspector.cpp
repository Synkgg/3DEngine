#include "../Editor.h"

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

    /*
     * Player
     */
    PlayerComponent* player =
        scene.GetComponent<
        PlayerComponent
        >(m_SelectedEntity);

    if (player != nullptr)
    {
        if (ImGui::CollapsingHeader(
            "Player",
            ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat(
                "Move Speed",
                &player->moveSpeed,
                0.1f,
                0.0f,
                100.0f
            );

            ImGui::DragFloat(
                "Look Sensitivity",
                &player->lookSensitivity,
                0.0001f,
                0.0001f,
                0.1f,
                "%.4f"
            );

            ImGui::Spacing();

            if (ImGui::Button(
                "Remove Player"))
            {
                scene.RemoveComponent<
                    PlayerComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Removed PlayerComponent."
                );
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

    /*
     * Add Component
     */
    ImGui::Separator();

    if (ImGui::Button(
        "Add Component"))
    {
        ImGui::OpenPopup(
            "AddComponentPopup"
        );
    }

    if (ImGui::BeginPopup(
        "AddComponentPopup"))
    {
        mesh =
            scene.GetComponent<
            MeshComponent
            >(
                m_SelectedEntity
            );

        color =
            scene.GetComponent<
            ColorComponent
            >(
                m_SelectedEntity
            );

        texture =
            scene.GetComponent<
            TextureComponent
            >(
                m_SelectedEntity
            );

        player =
            scene.GetComponent<
            PlayerComponent
            >(
                m_SelectedEntity
            );

        controller =
            scene.GetComponent<
            CharacterControllerComponent
            >(
                m_SelectedEntity
            );

        light =
            scene.GetComponent<
            LightComponent
            >(
                m_SelectedEntity
            );

        collider =
            scene.GetComponent<
            ColliderComponent
            >(
                m_SelectedEntity
            );

        script =
            scene.GetComponent<
            ScriptComponent
            >(
                m_SelectedEntity
            );

        interactable =
            scene.GetComponent<
            InteractableComponent
            >(
                m_SelectedEntity
            );

        /*
         * Mesh
         */
        if (mesh == nullptr)
        {
            if (ImGui::Selectable("Mesh"))
            {
                MeshComponent newMesh;

                newMesh.primitive =
                    PrimitiveType::None;

                scene.AddComponent<
                    MeshComponent
                >(
                    m_SelectedEntity,
                    newMesh
                );

                Logger::Info(
                    "Added MeshComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable("Mesh");
            ImGui::EndDisabled();
        }

        /*
         * Color
         */
        if (color == nullptr)
        {
            if (ImGui::Selectable("Color"))
            {
                scene.AddComponent<
                    ColorComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Added ColorComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable("Color");
            ImGui::EndDisabled();
        }

        /*
         * Texture
         */
        if (texture == nullptr)
        {
            if (ImGui::Selectable("Texture"))
            {
                scene.AddComponent<
                    TextureComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Added TextureComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable("Texture");
            ImGui::EndDisabled();
        }

        /*
         * Player
         */
        if (player == nullptr)
        {
            if (ImGui::Selectable("Player"))
            {
                scene.AddComponent<
                    PlayerComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Added PlayerComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable("Player");
            ImGui::EndDisabled();
        }

        /*
         * Character Controller
         */
        if (controller == nullptr)
        {
            if (ImGui::Selectable(
                "Character Controller"))
            {
                scene.AddComponent<
                    CharacterControllerComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Added CharacterControllerComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable(
                "Character Controller"
            );
            ImGui::EndDisabled();
        }

        /*
         * Directional Light
         */
        if (light == nullptr)
        {
            if (ImGui::Selectable(
                "Directional Light"))
            {
                scene.AddComponent<
                    LightComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Added LightComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable(
                "Directional Light"
            );
            ImGui::EndDisabled();
        }

        /*
         * Material
         */
        if (material == nullptr)
        {
            if (ImGui::Selectable("Material"))
            {
                scene.AddComponent<MaterialComponent>(m_SelectedEntity);
                Logger::Info("Added MaterialComponent.");
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable("Material");
            ImGui::EndDisabled();
        }

        /*
         * Collider
         */
        if (collider == nullptr)
        {
            if (ImGui::Selectable(
                "Collider"))
            {
                scene.AddComponent<
                    ColliderComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Added ColliderComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable(
                "Collider"
            );
            ImGui::EndDisabled();
        }

        /*
         * Interactable
         */
        if (interactable == nullptr)
        {
            if (ImGui::Selectable("Interactable"))
            {
                scene.AddComponent<
                    InteractableComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Added InteractableComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();

            ImGui::Selectable(
                "Interactable"
            );

            ImGui::EndDisabled();
        }

        /*
         * Script
         */
        if (script == nullptr)
        {
            if (ImGui::Selectable(
                "Script##AddScriptComponent"))
            {
                scene.AddComponent<
                    ScriptComponent
                >(
                    m_SelectedEntity
                );

                Logger::Info(
                    "Added ScriptComponent."
                );
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Selectable(
                "Script"
            );
            ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}