#include "../SceneSerializer.h"
#include "../Scene.h"

#include "../../Editor/HierarchyFolder.h"

#include "../../Graphics/PrimitiveType.h"

#include "../Components/TransformComponent.h"
#include "../Components/MeshComponent.h"
#include "../Components/ColorComponent.h"
#include "../Components/NameComponent.h"
#include "../Components/PawnComponent.h"
#include "../Components/PlayerStartComponent.h"
#include "../Components/CharacterControllerComponent.h"
#include "../Components/LightComponent.h"
#include "../Components/CameraComponent.h"
#include "../Components/ColliderComponent.h"
#include "../Components/TextureComponent.h"
#include "../Components/MaterialComponent.h"
#include "../Components/ScriptComponent.h"
#include "../Components/InteractableComponent.h"

#include "../../Core/Logger.h"

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
                << " " << (mesh->ownerNoSee ? 1 : 0)
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
                << material->emissive << " "
                << std::quoted(material->normalMap) << " "
                << std::quoted(material->metallicMap) << " "
                << std::quoted(material->roughnessMap) << " "
                << std::quoted(material->aoMap) << " "
                << std::quoted(material->emissiveMap) << '\n';
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
         * Pawn. Keep the legacy Player slot until the scene format is versioned.
         */
        PawnComponent* pawn = m_Scene.GetComponent<PawnComponent>(entity);
        if (pawn != nullptr)
            file << "Player 1 0 0\n";
        else
            file << "Player 0\n";

        /*
         * Player Start
         */
        if(auto* start=m_Scene.GetComponent<PlayerStartComponent>(entity)) file<<"PlayerStart 1 "<<start->slot<<'\n'; else file<<"PlayerStart 0\n";
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
         * Camera
         */
        if (CameraComponent* camera = m_Scene.GetComponent<CameraComponent>(entity))
            file << "Camera 1 " << camera->fieldOfView << " " << camera->nearClip << " "
                 << camera->farClip << " " << (camera->active ? 1 : 0) << '\n';
        else
            file << "Camera 0\n";

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
