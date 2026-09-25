#include "SceneSerializer.h"
#include "Scene.h"

#include "../Editor/HierarchyFolder.h"

#include "../Graphics/PrimitiveType.h"

#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include "Components/ColorComponent.h"
#include "Components/NameComponent.h"
#include "Components/PlayerComponent.h"
#include "Components/CharacterControllerComponent.h"
#include "Components/LightComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/TextureComponent.h"
#include "Components/MaterialComponent.h"
#include "Components/ScriptComponent.h"
#include "Components/InteractableComponent.h"

#include "../Core/Logger.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <utility>

namespace
{
    bool ReadLine(
        std::ifstream& file,
        std::string& line,
        const std::string& what,
        std::uint32_t entityID)
    {
        if (std::getline(file, line))
        {
            return true;
        }

        Logger::Error(
            "Missing " +
            what +
            " for entity " +
            std::to_string(entityID)
        );

        return false;
    }

    bool ReadVec3(
        const std::string& line,
        const char* expectedToken,
        Vec3& value,
        std::uint32_t entityID)
    {
        std::istringstream stream(line);

        std::string token;

        stream >>
            token >>
            value.x >>
            value.y >>
            value.z;

        if (token != expectedToken)
        {
            Logger::Error(
                std::string("Expected ") +
                expectedToken +
                ", got: " +
                line
            );

            return false;
        }

        if (stream.fail())
        {
            Logger::Error(
                std::string("Invalid ") +
                expectedToken +
                " data for entity " +
                std::to_string(entityID)
            );

            return false;
        }

        return true;
    }

    void SaveHierarchyFolder(
        std::ofstream& file,
        const HierarchyFolder& folder)
    {
        file << "Folder "
            << std::quoted(folder.name)
            << " "
            << (folder.expanded ? 1 : 0)
            << '\n';

        file << "Entities "
            << folder.entities.size();

        for (std::uint32_t entityID :
        folder.entities)
        {
            file << " "
                << entityID;
        }

        file << '\n';

        file << "Children "
            << folder.children.size()
            << '\n';

        for (const HierarchyFolder& child :
            folder.children)
        {
            SaveHierarchyFolder(
                file,
                child
            );
        }

        file << "EndFolder\n";
    }

    bool LoadHierarchyFolder(
        std::ifstream& file,
        HierarchyFolder& folder)
    {
        std::string line;

        /*
         * Folder
         */
        if (!std::getline(file, line))
        {
            return false;
        }

        std::istringstream folderLine(line);

        std::string token;
        int expanded = 1;

        folderLine >>
            token >>
            std::quoted(folder.name) >>
            expanded;

        if (token != "Folder")
        {
            Logger::Error(
                "Expected Folder, got: " +
                line
            );

            return false;
        }

        if (folderLine.fail())
        {
            Logger::Error(
                "Invalid Folder data: " +
                line
            );

            return false;
        }

        folder.expanded =
            expanded != 0;

        /*
         * Entities
         */
        if (!std::getline(file, line))
        {
            return false;
        }

        std::istringstream entityLine(line);

        std::size_t entityCount = 0;

        entityLine >>
            token >>
            entityCount;

        if (token != "Entities")
        {
            Logger::Error(
                "Expected Entities, got: " +
                line
            );

            return false;
        }

        folder.entities.clear();

        for (std::size_t i = 0;
            i < entityCount;
            ++i)
        {
            std::uint32_t entityID = 0;

            entityLine >>
                entityID;

            if (entityLine.fail())
            {
                Logger::Error(
                    "Invalid hierarchy entity ID."
                );

                return false;
            }

            folder.entities.push_back(
                entityID
            );
        }

        /*
         * Children
         */
        if (!std::getline(file, line))
        {
            return false;
        }

        std::istringstream childrenLine(line);

        std::size_t childCount = 0;

        childrenLine >>
            token >>
            childCount;

        if (token != "Children")
        {
            Logger::Error(
                "Expected Children, got: " +
                line
            );

            return false;
        }

        folder.children.clear();

        for (std::size_t i = 0;
            i < childCount;
            ++i)
        {
            HierarchyFolder child;

            if (!LoadHierarchyFolder(
                file,
                child))
            {
                return false;
            }

            folder.children.push_back(
                std::move(child)
            );
        }

        /*
         * EndFolder
         */
        if (!std::getline(file, line))
        {
            return false;
        }

        if (line != "EndFolder")
        {
            Logger::Error(
                "Expected EndFolder, got: " +
                line
            );

            return false;
        }

        return true;
    }
}

SceneSerializer::SceneSerializer(
    Scene& scene)
    : m_Scene(scene)
{
}

bool SceneSerializer::Save(
    const std::string& filepath,
    const std::vector<HierarchyFolder>& hierarchyFolders)
{
    std::ofstream file(filepath);

    if (!file.is_open())
    {
        Logger::Error(
            "Could not open scene for saving: " +
            filepath
        );

        return false;
    }

    /*
     * Header
     */
    file << "MyEngineScene\n";

    /*
     * Entities
     */
    file << "Entities "
        << m_Scene.GetEntities().size()
        << '\n';

    for (const Entity& entity :
        m_Scene.GetEntities())
    {
        TransformComponent* transform =
            m_Scene.GetComponent<
            TransformComponent
            >(entity);

        if (transform == nullptr)
        {
            Logger::Warning(
                "Skipping entity " +
                std::to_string(
                    entity.GetID()
                ) +
                " because it has no TransformComponent."
            );

            continue;
        }

        file << "Entity "
            << entity.GetID()
            << '\n';

        /*
         * Name
         */
        NameComponent* name =
            m_Scene.GetComponent<
            NameComponent
            >(entity);

        if (name != nullptr)
        {
            file << "Name "
                << name->name
                << '\n';
        }
        else
        {
            file << "Name Entity "
                << entity.GetID()
                << '\n';
        }

        /*
         * Transform
         */
        const Vec3& position =
            transform->transform.position;

        const Vec3& rotation =
            transform->transform.rotation;

        const Vec3& scale =
            transform->transform.scale;

        file << "Position "
            << position.x << " "
            << position.y << " "
            << position.z
            << '\n';

        file << "Rotation "
            << rotation.x << " "
            << rotation.y << " "
            << rotation.z
            << '\n';

        file << "Scale "
            << scale.x << " "
            << scale.y << " "
            << scale.z
            << '\n';

        Entity parent = m_Scene.GetParent(entity);
        file << "Parent " << (parent.IsValid() ? parent.GetID() : 0) << '\n';
        const std::string prefabSource = m_Scene.GetPrefabSource(entity);
        file << "PrefabSource " << std::quoted(prefabSource) << '\n';

        /*
         * Mesh
         */
        MeshComponent* mesh =
            m_Scene.GetComponent<
            MeshComponent
            >(entity);

        if (mesh != nullptr)
        {
            file << "Mesh 1 "
                << static_cast<int>(mesh->primitive)
                << " " << std::quoted(mesh->modelPath)
                << '\n';
        }
        else
        {
            file << "Mesh 0\n";
        }

        /*
         * Color
         */
        ColorComponent* color =
            m_Scene.GetComponent<
            ColorComponent
            >(entity);

        if (color != nullptr)
        {
            file << "Color 1 "
                << color->r << " "
                << color->g << " "
                << color->b << " "
                << color->a
                << '\n';
        }
        else
        {
            file << "Color 0\n";
        }

        /*
         * Texture
         */
        TextureComponent* texture =
            m_Scene.GetComponent<
            TextureComponent
            >(entity);

        if (texture != nullptr &&
            !texture->path.empty())
        {
            file << "Texture 1 "
                << texture->path
                << '\n';
        }
        else
        {
            file << "Texture 0\n";
        }

        MaterialComponent* material =
            m_Scene.GetComponent<MaterialComponent>(entity);
        if (material != nullptr)
        {
            file << "Material 1 "
                << material->metallic << " "
                << material->roughness << " "
                << material->ambientOcclusion << " "
                << material->emissive << '\n';
        }
        else
        {
            file << "Material 0\n";
        }

        /*
         * Interactable
         */
        InteractableComponent* interactable =
            m_Scene.GetComponent<
            InteractableComponent
            >(entity);

        if (interactable != nullptr)
        {
            file << "Interactable 1 "
                << (interactable->enabled ? 1 : 0)
                << " "
                << std::quoted(
                    interactable->prompt
                )
                << '\n';
        }
        else
        {
            file << "Interactable 0\n";
        }

        /*
         * Player
         */
        PlayerComponent* player =
            m_Scene.GetComponent<
            PlayerComponent
            >(entity);

        if (player != nullptr)
        {
            file << "Player 1 "
                << player->moveSpeed << " "
                << player->lookSensitivity
                << '\n';
        }
        else
        {
            file << "Player 0\n";
        }

        /*
         * Character Controller
         */
        CharacterControllerComponent*
            controller =
            m_Scene.GetComponent<
            CharacterControllerComponent
            >(entity);

        if (controller != nullptr)
        {
            file << "CharacterController 1 "
                << controller->gravity << " "
                << controller->jumpForce << " "
                << controller->verticalVelocity << " "
                << controller->grounded
                << '\n';
        }
        else
        {
            file << "CharacterController 0\n";
        }

        /*
         * Light
         */
        LightComponent* light =
            m_Scene.GetComponent<
            LightComponent
            >(entity);

        if (light != nullptr)
        {
            file << "Light 1 "
                << light->color.x << " " << light->color.y << " " << light->color.z << " "
                << light->direction.x << " " << light->direction.y << " " << light->direction.z << " "
                << light->intensity << " " << static_cast<int>(light->type) << " "
                << light->range << " " << light->innerAngle << " " << light->outerAngle << " "
                << (light->castShadows ? 1 : 0) << '\n';
        }
        else
        {
            file << "Light 0\n";
        }

        /*
         * Collider
         */
        ColliderComponent* collider =
            m_Scene.GetComponent<
            ColliderComponent
            >(entity);

        if (collider != nullptr)
        {
            file << "Collider 1 "
                << collider->width << " "
                << collider->height << " "
                << collider->depth
                << '\n';
        }
        else
        {
            file << "Collider 0\n";
        }

        /*
         * Scripts
         */
        ScriptComponent* scripts =
            m_Scene.GetComponent<
            ScriptComponent
            >(entity);

        if (scripts != nullptr)
        {
            file << "Scripts "
                << scripts->scriptNames.size()
                << '\n';

            for (const std::string& scriptName :
                scripts->scriptNames)
            {
                file << "Script "
                    << scriptName
                    << '\n';
            }
            std::size_t propertyCount=0;
            for(const auto& scriptProperties:scripts->properties) propertyCount+=scriptProperties.second.size();
            file << "ScriptProperties " << propertyCount << '\n';
            for(const auto& scriptProperties:scripts->properties)
                for(const auto& property:scriptProperties.second)
                    file << "ScriptProperty " << std::quoted(scriptProperties.first) << " "
                        << std::quoted(property.first) << " " << static_cast<int>(property.second.type) << " "
                        << std::quoted(property.second.value) << '\n';
        }
        else
        {
            file << "Scripts 0\n";
        }
    }

    /*
     * Hierarchy folders
     */
    file << "HierarchyFolders "
        << hierarchyFolders.size()
        << '\n';

    for (const HierarchyFolder& folder :
        hierarchyFolders)
    {
        SaveHierarchyFolder(
            file,
            folder
        );
    }

    /*
     * File check
     */
    if (!file.good())
    {
        Logger::Error(
            "Failed while writing scene: " +
            filepath
        );

        return false;
    }

    return true;
}

bool SceneSerializer::Load(
    const std::string& filepath,
    std::vector<HierarchyFolder>& hierarchyFolders)
{
    std::ifstream file(filepath);

    if (!file.is_open())
    {
        Logger::Error(
            "Could not open scene file: " +
            filepath
        );

        return false;
    }

    std::string line;

    /*
     * Header
     */
    if (!std::getline(file, line))
    {
        Logger::Error(
            "Scene file is empty."
        );

        return false;
    }

    if (line != "MyEngineScene")
    {
        Logger::Error(
            "Invalid scene header: " +
            line
        );

        return false;
    }

    /*
     * Scene environment is optional for backward compatibility.
     */
    if (!ReadLine(file, line, "environment or entity count", 0))
        return false;

    if (line.rfind("Environment ", 0) == 0)
    {
        // Backward compatibility: consume and ignore legacy per-scene render settings.
        if (!ReadLine(file, line, "entity count", 0)) return false;
    }

    std::istringstream entityHeader(line);

    std::string entitiesToken;
    std::size_t entityCount = 0;

    entityHeader >>
        entitiesToken >>
        entityCount;

    if (entitiesToken != "Entities")
    {
        Logger::Error(
            "Expected 'Entities', got: " +
            line
        );

        return false;
    }

    Logger::Info(
        "Loading " +
        std::to_string(entityCount) +
        " entities."
    );

    /*
     * Clear current scene.
     */
    m_Scene.Clear();

    /*
     * Load entities.
     */
    for (std::size_t i = 0;
        i < entityCount;
        ++i)
    {
        /*
         * Entity
         */
        if (!ReadLine(
            file,
            line,
            "Entity",
            0))
        {
            return false;
        }

        std::istringstream entityLine(line);

        std::string entityToken;
        std::uint32_t entityID = 0;

        entityLine >>
            entityToken >>
            entityID;

        if (entityToken != "Entity")
        {
            Logger::Error(
                "Expected 'Entity', got: " +
                line
            );

            return false;
        }

        /*
         * Create entity with original ID.
         */
        Entity entity =
            m_Scene.CreateEntityWithID(
                entityID
            );

        if (!entity.IsValid())
        {
            Logger::Error(
                "Failed to create entity with ID " +
                std::to_string(entityID)
            );

            return false;
        }

        /*
         * Name
         */
        if (!ReadLine(
            file,
            line,
            "Name",
            entityID))
        {
            return false;
        }

        if (line.rfind("Name ", 0) != 0)
        {
            Logger::Error(
                "Expected Name, got: " +
                line
            );

            return false;
        }

        NameComponent* name =
            m_Scene.GetComponent<
            NameComponent
            >(entity);

        if (name == nullptr)
        {
            Logger::Error(
                "Entity " +
                std::to_string(entityID) +
                " has no NameComponent."
            );

            return false;
        }

        name->name =
            line.substr(5);

        /*
         * Transform
         */
        TransformComponent* transform =
            m_Scene.GetComponent<
            TransformComponent
            >(entity);

        if (transform == nullptr)
        {
            Logger::Error(
                "Entity " +
                std::to_string(entityID) +
                " has no TransformComponent."
            );

            return false;
        }

        if (!ReadLine(
            file,
            line,
            "Position",
            entityID))
        {
            return false;
        }

        if (!ReadVec3(
            line,
            "Position",
            transform->transform.position,
            entityID))
        {
            return false;
        }

        if (!ReadLine(
            file,
            line,
            "Rotation",
            entityID))
        {
            return false;
        }

        if (!ReadVec3(
            line,
            "Rotation",
            transform->transform.rotation,
            entityID))
        {
            return false;
        }

        if (!ReadLine(
            file,
            line,
            "Scale",
            entityID))
        {
            return false;
        }

        if (!ReadVec3(
            line,
            "Scale",
            transform->transform.scale,
            entityID))
        {
            return false;
        }

        /*
         * Parent is optional for backward compatibility with older scenes.
         */
        if (!ReadLine(file, line, "Mesh or Parent", entityID))
        {
            return false;
        }

        if (line.rfind("Parent ", 0) == 0)
        {
            std::istringstream parentLine(line);
            std::string parentToken;
            std::uint32_t parentID = 0;
            parentLine >> parentToken >> parentID;
            if (parentID != 0)
                m_Scene.SetParent(entity, Entity(parentID));

            if (!ReadLine(file, line, "Mesh or PrefabSource", entityID))
                return false;
        }

        // PrefabSource is optional so existing scenes continue to load.
        if (line.rfind("PrefabSource ", 0) == 0)
        {
            std::istringstream prefabLine(line);
            std::string token;
            std::string source;
            prefabLine >> token >> std::quoted(source);
            if (!source.empty()) m_Scene.SetPrefabSource(entity, source);
            if (!ReadLine(file, line, "Mesh", entityID))
                return false;
        }

        /*
         * Mesh
         */
        {
            std::istringstream meshLine(line);

            std::string token;
            int hasMesh = 0;
            int primitiveValue = 0;

            meshLine >>
                token >>
                hasMesh;

            if (token != "Mesh")
            {
                Logger::Error(
                    "Expected Mesh, got: " +
                    line
                );

                return false;
            }

            if (hasMesh != 0 &&
                hasMesh != 1)
            {
                Logger::Error(
                    "Invalid Mesh flag for entity " +
                    std::to_string(entityID)
                );

                return false;
            }

            if (hasMesh == 1)
            {
                meshLine >>
                    primitiveValue;

                if (meshLine.fail())
                {
                    Logger::Error(
                        "Invalid Mesh data for entity " +
                        std::to_string(entityID)
                    );

                    return false;
                }

                if (primitiveValue < 0 ||
                    primitiveValue > 4)
                {
                    Logger::Error(
                        "Invalid primitive value for entity " +
                        std::to_string(entityID)
                    );

                    return false;
                }

                MeshComponent mesh;

                mesh.primitive =
                    static_cast<PrimitiveType>(
                        primitiveValue
                        );

                if (!(meshLine >> std::quoted(mesh.modelPath)))
                {
                    meshLine.clear();
                    mesh.modelPath.clear();
                }

                m_Scene.AddComponent<
                    MeshComponent
                >(
                    entity,
                    mesh
                );
            }
        }

        /*
         * Color
         */
        if (!ReadLine(
            file,
            line,
            "Color",
            entityID))
        {
            return false;
        }

        {
            std::istringstream colorLine(line);

            std::string token;
            int hasColor = 0;

            colorLine >>
                token >>
                hasColor;

            if (token != "Color")
            {
                Logger::Error(
                    "Expected Color, got: " +
                    line
                );

                return false;
            }

            if (hasColor != 0 &&
                hasColor != 1)
            {
                Logger::Error(
                    "Invalid Color flag for entity " +
                    std::to_string(entityID)
                );

                return false;
            }

            if (hasColor == 1)
            {
                ColorComponent color;

                colorLine >>
                    color.r >>
                    color.g >>
                    color.b >>
                    color.a;

                if (colorLine.fail())
                {
                    Logger::Error(
                        "Invalid Color data for entity " +
                        std::to_string(entityID)
                    );

                    return false;
                }

                m_Scene.AddComponent<
                    ColorComponent
                >(
                    entity,
                    color
                );
            }
        }

        /*
         * Texture
         *
         * Optional for older scene files.
         */
        {
            const std::streampos texturePosition =
                file.tellg();

            if (std::getline(file, line))
            {
                if (line.rfind("Texture ", 0) == 0)
                {
                    std::istringstream textureLine(line);

                    std::string token;
                    int hasTexture = 0;

                    textureLine >>
                        token >>
                        hasTexture;

                    if (token != "Texture")
                    {
                        Logger::Error(
                            "Expected Texture, got: " +
                            line
                        );

                        return false;
                    }

                    if (hasTexture != 0 &&
                        hasTexture != 1)
                    {
                        Logger::Error(
                            "Invalid Texture flag for entity " +
                            std::to_string(entityID)
                        );

                        return false;
                    }

                    if (hasTexture == 1)
                    {
                        std::string texturePath;

                        std::getline(
                            textureLine >> std::ws,
                            texturePath
                        );

                        if (texturePath.empty())
                        {
                            Logger::Error(
                                "Invalid Texture data for entity " +
                                std::to_string(entityID)
                            );

                            return false;
                        }

                        TextureComponent texture;

                        texture.path =
                            texturePath;

                        m_Scene.AddComponent<
                            TextureComponent
                        >(
                            entity,
                            texture
                        );
                    }
                }
                else
                {
                    file.clear();

                    file.seekg(
                        texturePosition
                    );
                }
            }
        }

        /*
         * Material - optional for backwards compatibility.
         */
        {
            const std::streampos materialPosition = file.tellg();
            if (std::getline(file, line))
            {
                if (line.rfind("Material ", 0) == 0)
                {
                    std::istringstream materialLine(line);
                    std::string token; int hasMaterial = 0;
                    materialLine >> token >> hasMaterial;
                    if (hasMaterial == 1)
                    {
                        MaterialComponent material;
                        materialLine >> material.metallic >> material.roughness
                            >> material.ambientOcclusion >> material.emissive;
                        if (materialLine.fail()) return false;
                        m_Scene.AddComponent<MaterialComponent>(entity, material);
                    }
                }
                else
                {
                    file.clear();
                    file.seekg(materialPosition);
                }
            }
        }

        /*
         * Interactable
         *
         * Optional for older scene files.
         */
        {
            const std::streampos interactablePosition =
                file.tellg();

            if (std::getline(file, line))
            {
                if (line.rfind(
                    "Interactable ",
                    0) == 0)
                {
                    std::istringstream interactableLine(
                        line
                    );

                    std::string token;
                    int hasInteractable = 0;
                    int enabled = 0;
                    std::string prompt;

                    interactableLine >>
                        token >>
                        hasInteractable;

                    if (token != "Interactable")
                    {
                        Logger::Error(
                            "Expected Interactable, got: " +
                            line
                        );

                        return false;
                    }

                    if (hasInteractable != 0 &&
                        hasInteractable != 1)
                    {
                        Logger::Error(
                            "Invalid Interactable flag for entity " +
                            std::to_string(entityID)
                        );

                        return false;
                    }

                    if (hasInteractable == 1)
                    {
                        interactableLine >>
                            enabled >>
                            std::quoted(prompt);

                        if (interactableLine.fail())
                        {
                            Logger::Error(
                                "Invalid Interactable data for entity " +
                                std::to_string(entityID)
                            );

                            return false;
                        }

                        if (enabled != 0 &&
                            enabled != 1)
                        {
                            Logger::Error(
                                "Invalid Interactable enabled value for entity " +
                                std::to_string(entityID)
                            );

                            return false;
                        }

                        InteractableComponent interactable;

                        interactable.enabled =
                            enabled != 0;

                        interactable.prompt =
                            prompt;

                        m_Scene.AddComponent<
                            InteractableComponent
                        >(
                            entity,
                            interactable
                        );
                    }
                }
                else
                {
                    file.clear();

                    file.seekg(
                        interactablePosition
                    );
                }
            }
        }

        /*
         * Player
         */
        if (!ReadLine(
            file,
            line,
            "Player",
            entityID))
        {
            return false;
        }

        {
            std::istringstream playerLine(line);

            std::string token;
            int hasPlayer = 0;

            playerLine >>
                token >>
                hasPlayer;

            if (token != "Player")
            {
                Logger::Error(
                    "Expected Player, got: " +
                    line
                );

                return false;
            }

            if (hasPlayer != 0 &&
                hasPlayer != 1)
            {
                Logger::Error(
                    "Invalid Player flag for entity " +
                    std::to_string(entityID)
                );

                return false;
            }

            if (hasPlayer == 1)
            {
                PlayerComponent player;

                playerLine >>
                    player.moveSpeed >>
                    player.lookSensitivity;

                if (playerLine.fail())
                {
                    Logger::Error(
                        "Invalid Player data for entity " +
                        std::to_string(entityID)
                    );

                    return false;
                }

                m_Scene.AddComponent<
                    PlayerComponent
                >(
                    entity,
                    player
                );
            }
        }

        /*
         * Character Controller
         */
        if (!ReadLine(
            file,
            line,
            "CharacterController",
            entityID))
        {
            return false;
        }

        {
            std::istringstream controllerLine(line);

            std::string token;
            int hasController = 0;

            controllerLine >>
                token >>
                hasController;

            if (token != "CharacterController")
            {
                Logger::Error(
                    "Expected CharacterController, got: " +
                    line
                );

                return false;
            }

            if (hasController != 0 &&
                hasController != 1)
            {
                Logger::Error(
                    "Invalid CharacterController flag for entity " +
                    std::to_string(entityID)
                );

                return false;
            }

            if (hasController == 1)
            {
                CharacterControllerComponent controller;

                int grounded = 0;

                controllerLine >>
                    controller.gravity >>
                    controller.jumpForce >>
                    controller.verticalVelocity >>
                    grounded;

                if (controllerLine.fail())
                {
                    Logger::Error(
                        "Invalid CharacterController data for entity " +
                        std::to_string(entityID)
                    );

                    return false;
                }

                if (grounded != 0 &&
                    grounded != 1)
                {
                    Logger::Error(
                        "Invalid grounded value for entity " +
                        std::to_string(entityID)
                    );

                    return false;
                }

                controller.grounded =
                    grounded != 0;

                m_Scene.AddComponent<
                    CharacterControllerComponent
                >(
                    entity,
                    controller
                );
            }
        }

        /*
         * Light
         */
        if (!ReadLine(
            file,
            line,
            "Light",
            entityID))
        {
            return false;
        }

        {
            std::istringstream lightLine(line);

            std::string token;
            int hasLight = 0;

            lightLine >>
                token >>
                hasLight;

            if (token != "Light")
            {
                Logger::Error(
                    "Expected Light, got: " +
                    line
                );

                return false;
            }

            if (hasLight != 0 &&
                hasLight != 1)
            {
                Logger::Error(
                    "Invalid Light flag for entity " +
                    std::to_string(entityID)
                );

                return false;
            }

            if (hasLight == 1)
            {
                LightComponent light;

                lightLine >>
                    light.color.x >>
                    light.color.y >>
                    light.color.z >>
                    light.direction.x >>
                    light.direction.y >>
                    light.direction.z >>
                    light.intensity;

                int lightType = 0;
                int castShadows = 1;
                if (lightLine >> lightType >> light.range >> light.innerAngle >> light.outerAngle >> castShadows)
                {
                    if (lightType >= 0 && lightType <= 2)
                        light.type = static_cast<LightType>(lightType);
                    light.castShadows = castShadows != 0;
                }
                else
                {
                    lightLine.clear();
                }

                if (lightLine.fail())
                {
                    Logger::Error(
                        "Invalid Light data for entity " +
                        std::to_string(entityID)
                    );

                    return false;
                }

                m_Scene.AddComponent<
                    LightComponent
                >(
                    entity,
                    light
                );
            }
        }

        /*
         * Collider
         */
        if (!ReadLine(
            file,
            line,
            "Collider",
            entityID))
        {
            return false;
        }

        {
            std::istringstream colliderLine(line);

            std::string token;
            int hasCollider = 0;

            colliderLine >>
                token >>
                hasCollider;

            if (token != "Collider")
            {
                Logger::Error(
                    "Expected Collider, got: " +
                    line
                );

                return false;
            }

            if (hasCollider != 0 &&
                hasCollider != 1)
            {
                Logger::Error(
                    "Invalid Collider flag for entity " +
                    std::to_string(entityID)
                );

                return false;
            }

            if (hasCollider == 1)
            {
                ColliderComponent collider;

                colliderLine >>
                    collider.width >>
                    collider.height >>
                    collider.depth;

                if (colliderLine.fail())
                {
                    Logger::Error(
                        "Invalid Collider data for entity " +
                        std::to_string(entityID)
                    );

                    return false;
                }

                m_Scene.AddComponent<
                    ColliderComponent
                >(
                    entity,
                    collider
                );
            }
        }

        /*
         * Scripts
         *
         * Optional for older scene files.
         */
        {
            const std::streampos scriptsPosition =
                file.tellg();

            if (std::getline(file, line))
            {
                if (line.rfind(
                    "Scripts ",
                    0) == 0)
                {
                    std::istringstream scriptsLine(line);

                    std::string token;
                    std::size_t scriptCount = 0;

                    scriptsLine >>
                        token >>
                        scriptCount;

                    if (token != "Scripts")
                    {
                        Logger::Error(
                            "Invalid Scripts header for entity " +
                            std::to_string(entityID)
                        );

                        return false;
                    }

                    ScriptComponent scripts;

                    for (std::size_t scriptIndex = 0;
                        scriptIndex < scriptCount;
                        ++scriptIndex)
                    {
                        if (!ReadLine(
                            file,
                            line,
                            "Script",
                            entityID))
                        {
                            return false;
                        }

                        if (line.rfind(
                            "Script ",
                            0) != 0)
                        {
                            Logger::Error(
                                "Expected Script, got: " +
                                line
                            );

                            return false;
                        }

                        const std::string scriptName =
                            line.substr(7);

                        if (scriptName.empty())
                        {
                            Logger::Error(
                                "Empty script name for entity " +
                                std::to_string(entityID)
                            );

                            return false;
                        }

                        scripts.scriptNames.push_back(
                            scriptName
                        );
                    }

                    const std::streampos propertiesPosition=file.tellg();
                    if(std::getline(file,line) && line.rfind("ScriptProperties ",0)==0)
                    {
                        std::istringstream header(line); std::string token; std::size_t count=0; header>>token>>count;
                        for(std::size_t propertyIndex=0;propertyIndex<count;++propertyIndex)
                        {
                            if(!std::getline(file,line)) return false;
                            std::istringstream propertyLine(line); std::string propertyToken,scriptPath,propertyName,value; int type=0;
                            propertyLine>>propertyToken>>std::quoted(scriptPath)>>std::quoted(propertyName)>>type>>std::quoted(value);
                            if(propertyToken!="ScriptProperty"||propertyLine.fail()) return false;
                            scripts.properties[scriptPath][propertyName]={static_cast<ScriptPropertyType>(type),value};
                        }
                    }
                    else
                    {
                        file.clear(); file.seekg(propertiesPosition);
                    }

                    if (!scripts.scriptNames.empty())
                    {
                        m_Scene.AddComponent<
                            ScriptComponent
                        >(
                            entity,
                            scripts
                        );
                    }
                }
                else
                {
                    file.clear();

                    file.seekg(
                        scriptsPosition
                    );
                }
            }
        }
    }

    /*
 * Hierarchy folders.
 *
 * This MUST be after all entities
 * have been loaded.
 */
    hierarchyFolders.clear();

    if (std::getline(file, line))
    {
        std::istringstream hierarchyHeader(line);

        std::string hierarchyToken;
        std::size_t folderCount = 0;

        hierarchyHeader >>
            hierarchyToken >>
            folderCount;

        if (hierarchyToken != "HierarchyFolders")
        {
            Logger::Error(
                "Expected HierarchyFolders, got: " +
                line
            );

            return false;
        }

        for (std::size_t i = 0;
            i < folderCount;
            ++i)
        {
            HierarchyFolder folder;

            if (!LoadHierarchyFolder(
                file,
                folder))
            {
                Logger::Error(
                    "Failed to load hierarchy folder."
                );

                return false;
            }

            hierarchyFolders.push_back(
                std::move(folder)
            );
        }
    }

    Logger::Info(
        "Scene loaded successfully: " +
        filepath
    );

    return true;
}