#include "../LuaScript.h"

#include "../../../Core/Logger.h"
#include "../../../Core/ProjectSettings.h"

#include "../../../Platform/SDL/Input.h"
#include "../../../Graphics/Renderer.h"
#include "../../../Graphics/Texture2D.h"
#include "../../../Audio/AudioEngine.h"
#include "../../../UI/UICanvas.h"
#include "../../../UI/UIWidget.h"
#include "../../../UI/UIText.h"
#include "../../../UI/UIButton.h"
#include "../../../UI/UITextInput.h"
#include "../../../UI/UISlider.h"
#include "../../../UI/UISerializer.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

#include "../../Scene.h"
#include "../../PrefabSerializer.h"
#include "../../Runtime/Runtime.h"
#include "../../Components/TransformComponent.h"
#include "../../Components/CharacterControllerComponent.h"
#include "../../Components/PawnComponent.h"
#include "../../Components/PlayerStartComponent.h"
#include "../../Components/MeshComponent.h"
#include "../../Components/ColliderComponent.h"
#include "../../Components/InteractableComponent.h"
#include "../../Components/LightComponent.h"
#include "../../Components/CameraComponent.h"
#include "../../Systems/CollisionSystem.h"

void LuaScript::BindEngineAPI()
{
    if (m_Lua == nullptr ||
        m_Environment == nullptr)
    {
        return;
    }

    /*
     * Transform
     */
    sol::table transform =
        m_Lua->create_table();

    transform.set_function(
        "GetPosition",
        [this]()
        {
            sol::table result =
                m_Lua->create_table();

            if (m_Scene == nullptr)
            {
                result["x"] = 0.0f;
                result["y"] = 0.0f;
                result["z"] = 0.0f;

                return result;
            }

            TransformComponent* component =
                m_Scene->GetComponent<
                TransformComponent
                >(m_Entity);

            if (component == nullptr)
            {
                result["x"] = 0.0f;
                result["y"] = 0.0f;
                result["z"] = 0.0f;

                return result;
            }

            const Vec3& position =
                component->transform.position;

            result["x"] =
                position.x;

            result["y"] =
                position.y;

            result["z"] =
                position.z;

            return result;
        }
    );

    transform.set_function(
        "SetPosition",
        [this](
            float x,
            float y,
            float z)
        {
            if (m_Scene == nullptr)
            {
                return;
            }

            TransformComponent* component =
                m_Scene->GetComponent<
                TransformComponent
                >(m_Entity);

            if (component == nullptr)
            {
                return;
            }

            component->transform.position.x =
                x;

            component->transform.position.y =
                y;

            component->transform.position.z =
                z;
        }
    );

    transform.set_function(
        "Translate",
        [this](
            float x,
            float y,
            float z)
        {
            if (m_Scene == nullptr)
            {
                return;
            }

            TransformComponent* component =
                m_Scene->GetComponent<
                TransformComponent
                >(m_Entity);

            if (component == nullptr)
            {
                return;
            }

            component->transform.position.x +=
                x;

            component->transform.position.y +=
                y;

            component->transform.position.z +=
                z;
        }
    );

    transform.set_function(
        "GetScale",
        [this]()
        {
            sol::table result =
                m_Lua->create_table();

            if (m_Scene == nullptr)
            {
                result["x"] = 1.0f;
                result["y"] = 1.0f;
                result["z"] = 1.0f;

                return result;
            }

            TransformComponent* component =
                m_Scene->GetComponent<
                TransformComponent
                >(m_Entity);

            if (component == nullptr)
            {
                result["x"] = 1.0f;
                result["y"] = 1.0f;
                result["z"] = 1.0f;

                return result;
            }

            const Vec3& scale =
                component->transform.scale;

            result["x"] =
                scale.x;

            result["y"] =
                scale.y;

            result["z"] =
                scale.z;

            return result;
        }
    );

    transform.set_function(
        "SetScale",
        [this](
            float x,
            float y,
            float z)
        {
            if (m_Scene == nullptr)
            {
                return;
            }

            TransformComponent* component =
                m_Scene->GetComponent<
                TransformComponent
                >(m_Entity);

            if (component == nullptr)
            {
                return;
            }

            component->transform.scale.x =
                x;

            component->transform.scale.y =
                y;

            component->transform.scale.z =
                z;
        }
    );

    transform.set_function(
        "GetRotation",
        [this]()
        {
            constexpr float RadiansToDegrees =
                57.2957795f;

            sol::table result =
                m_Lua->create_table();

            if (m_Scene == nullptr)
            {
                result["x"] = 0.0f;
                result["y"] = 0.0f;
                result["z"] = 0.0f;

                return result;
            }

            TransformComponent* component =
                m_Scene->GetComponent<
                TransformComponent
                >(m_Entity);

            if (component == nullptr)
            {
                result["x"] = 0.0f;
                result["y"] = 0.0f;
                result["z"] = 0.0f;

                return result;
            }

            const Vec3& rotation =
                component->transform.rotation;

            result["x"] =
                rotation.x *
                RadiansToDegrees;

            result["y"] =
                rotation.y *
                RadiansToDegrees;

            result["z"] =
                rotation.z *
                RadiansToDegrees;

            return result;
        }
    );

    transform.set_function(
        "SetRotation",
        [this](
            float x,
            float y,
            float z)
        {
            constexpr float DegreesToRadians =
                0.0174532925f;

            if (m_Scene == nullptr)
            {
                return;
            }

            TransformComponent* component =
                m_Scene->GetComponent<
                TransformComponent
                >(m_Entity);

            if (component == nullptr)
            {
                return;
            }

            component->transform.rotation.x =
                x *
                DegreesToRadians;

            component->transform.rotation.y =
                y *
                DegreesToRadians;

            component->transform.rotation.z =
                z *
                DegreesToRadians;
        }
    );

    (*m_Environment)["transform"] =
        transform;

    /*
     * Input
     */
    sol::table input =
        m_Lua->create_table();

    input.set_function(
        "IsKeyDown",
        [this](
            const std::string& keyName)
        {
            if (m_Input == nullptr)
            {
                return false;
            }

            SDL_Scancode key =
                SDL_GetScancodeFromName(
                    keyName.c_str()
                );

            if (key ==
                SDL_SCANCODE_UNKNOWN)
            {
                return false;
            }

            return m_Input->IsKeyDown(
                key
            );
        }
    );

    input.set_function(
        "IsKeyPressed",
        [this](
            const std::string& keyName)
        {
            if (m_Input == nullptr)
            {
                return false;
            }

            SDL_Scancode key =
                SDL_GetScancodeFromName(
                    keyName.c_str()
                );

            if (key ==
                SDL_SCANCODE_UNKNOWN)
            {
                return false;
            }

            return m_Input->IsKeyPressed(
                key
            );
        }
    );

    input.set_function(
        "IsMouseButtonDown",
        [this](
            int button)
        {
            if (m_Input == nullptr)
            {
                return false;
            }

            return m_Input->IsMouseButtonDown(
                static_cast<Uint8>(
                    button
                    )
            );
        }
    );

    input.set_function(
        "GetMouseDeltaX",
        [this]()
        {
            if (m_Input == nullptr)
            {
                return 0.0f;
            }

            return m_Input->GetMouseDeltaX();
        }
    );

    input.set_function(
        "GetMouseDeltaY",
        [this]()
        {
            if (m_Input == nullptr)
            {
                return 0.0f;
            }

            return m_Input->GetMouseDeltaY();
        }
    );

    input.set_function(
        "SetCursorVisible",
        [this](bool visible)
        {
            if (m_Runtime == nullptr)
            {
                return;
            }

            m_Runtime->SetWantsCursor(visible);
        }
    );

    input.set_function(
        "GetClipboardText",
        []()
        {
            char* text = SDL_GetClipboardText();
            if (text == nullptr)
                return std::string();

            std::string result(text);
            SDL_free(text);
            return result;
        }
    );

    input.set_function(
        "SetClipboardText",
        [](const std::string& text)
        {
            return SDL_SetClipboardText(text.c_str());
        }
    );

    (*m_Environment)["Input"] =
        input;

    /*
     * Preferences - tiny per-project string storage for runtime UI values
     * such as the last multiplayer address. Keys are sanitized so scripts
     * cannot escape the Saved directory.
     */
    sol::table preferences = m_Lua->create_table();

    preferences.set_function(
        "LoadString",
        [this](const std::string& key, const std::string& fallback)
        {
            std::string safeKey;
            for (char ch : key)
            {
                if ((ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '_' || ch == '-')
                {
                    safeKey += ch;
                }
            }

            if (safeKey.empty())
                return fallback;

            const std::filesystem::path savedPath = m_Runtime
                ? std::filesystem::path(m_Runtime->ResolveProjectPath("Saved"))
                : std::filesystem::path("Saved");
            std::ifstream file(savedPath / (safeKey + ".txt"));
            if (!file.is_open())
                return fallback;

            std::string value;
            std::getline(file, value);
            return value.empty() ? fallback : value;
        }
    );

    preferences.set_function(
        "SaveString",
        [this](const std::string& key, const std::string& value)
        {
            std::string safeKey;
            for (char ch : key)
            {
                if ((ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '_' || ch == '-')
                {
                    safeKey += ch;
                }
            }

            if (safeKey.empty())
                return false;

            const std::filesystem::path savedPath = m_Runtime
                ? std::filesystem::path(m_Runtime->ResolveProjectPath("Saved"))
                : std::filesystem::path("Saved");
            std::error_code error;
            std::filesystem::create_directories(savedPath, error);
            if (error)
                return false;

            std::ofstream file(savedPath / (safeKey + ".txt"), std::ios::trunc);
            if (!file.is_open())
                return false;

            file << value;
            return file.good();
        }
    );

    (*m_Environment)["Preferences"] = preferences;

    /*
     * Time
     */
    sol::table time =
        m_Lua->create_table();

    time.set_function(
        "GetDeltaTime",
        [this]()
        {
            return m_DeltaTime;
        }
    );

    (*m_Environment)["Time"] =
        time;

    /*
     * Tween - generic scalar interpolation owned by this script instance.
     * Starting another tween with the same id replaces the previous one.
     */
    sol::table tween = m_Lua->create_table();
    tween.set_function("Cancel", [this](const std::string& id)
    {
        const auto before=m_Tweens.size();
        m_Tweens.erase(std::remove_if(m_Tweens.begin(),m_Tweens.end(),
            [&](const TweenJob& job){return job.id==id;}),m_Tweens.end());
        return before!=m_Tweens.size();
    });
    tween.set_function("Value", [this](const std::string& id, float from, float to, float duration,
        const std::string& easing, sol::protected_function callback)
    {
        m_Tweens.erase(std::remove_if(m_Tweens.begin(),m_Tweens.end(),
            [&](const TweenJob& job){return job.id==id;}),m_Tweens.end());
        TweenJob job; job.id=id; job.duration=std::max(0.0f,duration); job.easing=easing;
        job.apply=[from,to,callback](float t) mutable
        {
            if(!callback.valid()) return;
            sol::protected_function_result result=callback(from+(to-from)*t);
            (void)result;
        };
        m_Tweens.push_back(std::move(job));
        return true;
    });
    (*m_Environment)["Tween"] = tween;

    /*
     * Camera
     */
    sol::table camera =
        m_Lua->create_table();

    camera.set_function(
        "Move",
        [this](
            float forward,
            float right,
            float up,
            float deltaTime)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->MoveCamera(
                forward,
                right,
                up,
                deltaTime
            );
        }
    );

    camera.set_function(
        "Rotate",
        [this](
            float yaw,
            float pitch)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->RotateCamera(
                yaw,
                pitch
            );
        }
    );

    camera.set_function(
        "GetPosition",
        [this]()
        {
            sol::table result =
                m_Lua->create_table();

            if (m_Renderer == nullptr)
            {
                result["x"] = 0.0f;
                result["y"] = 0.0f;
                result["z"] = 0.0f;

                return result;
            }

            const Vec3 position =
                m_Renderer->GetCameraPosition();

            result["x"] =
                position.x;

            result["y"] =
                position.y;

            result["z"] =
                position.z;

            return result;
        }
    );

    camera.set_function(
        "SetPosition",
        [this](
            float x,
            float y,
            float z)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->SetCameraPosition(
                Vec3(
                    x,
                    y,
                    z
                )
            );
        }
    );

    camera.set_function(
        "SetRotation",
        [this](
            float yaw,
            float pitch)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->SetCameraRotation(
                yaw,
                pitch
            );
        }
    );

    camera.set_function(
        "Reset",
        [this]()
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->ResetCamera();
        }
    );

    camera.set_function(
        "GetForward",
        [this]()
        {
            sol::table result =
                m_Lua->create_table();

            if (m_Renderer == nullptr)
            {
                result["x"] = 0.0f;
                result["y"] = 0.0f;
                result["z"] = -1.0f;

                return result;
            }

            const Vec3 forward =
                m_Renderer->GetCameraForward();

            result["x"] =
                forward.x;

            result["y"] =
                forward.y;

            result["z"] =
                forward.z;

            return result;
        }
    );

    camera.set_function(
        "GetRight",
        [this]()
        {
            sol::table result =
                m_Lua->create_table();

            if (m_Renderer == nullptr)
            {
                result["x"] = 1.0f;
                result["y"] = 0.0f;
                result["z"] = 0.0f;

                return result;
            }

            const Vec3 right =
                m_Renderer->GetCameraRight();

            result["x"] =
                right.x;

            result["y"] =
                right.y;

            result["z"] =
                right.z;

            return result;
        }
    );

    camera.set_function("SetActive", [this](std::uint32_t entityID)
    {
        if (!m_Scene) return false;
        CameraComponent* selected = m_Scene->GetComponent<CameraComponent>(Entity(entityID));
        if (!selected) return false;
        for (const Entity& entity : m_Scene->GetEntities())
            if (auto* component = m_Scene->GetComponent<CameraComponent>(entity))
                component->active = entity.GetID() == entityID;
        return true;
    });
    camera.set_function("GetActive", [this]()
    {
        if (!m_Scene) return std::uint32_t(0);
        for (const Entity& entity : m_Scene->GetEntities())
            if (auto* component = m_Scene->GetComponent<CameraComponent>(entity); component && component->active)
                return entity.GetID();
        return std::uint32_t(0);
    });
    camera.set_function("RotateEntity", [this](std::uint32_t entityID, float yawDelta, float pitchDelta)
    {
        if (!m_Scene) return false;
        TransformComponent* transform = m_Scene->GetComponent<TransformComponent>(Entity(entityID));
        CameraComponent* component = m_Scene->GetComponent<CameraComponent>(Entity(entityID));
        if (!transform || !component) return false;
        constexpr float pitchLimit = 1.553343034f;
        transform->transform.rotation.y += yawDelta;
        transform->transform.rotation.x = std::clamp(transform->transform.rotation.x + pitchDelta, -pitchLimit, pitchLimit);
        const Transform world = m_Scene->GetWorldTransform(Entity(entityID));
        if (component->active && m_Renderer)
        {
            m_Renderer->SetCameraPosition(world.position);
            m_Renderer->SetCameraRotation(world.rotation.y, world.rotation.x);
            m_Renderer->SetCameraFov(component->fieldOfView);
            m_Renderer->SetCameraNearPlane(component->nearClip);
            m_Renderer->SetCameraFarPlane(component->farClip);
        }
        return true;
    });
    camera.set_function("SetEntityFOV", [this](std::uint32_t entityID, float fov)
    {
        if (!m_Scene) return false;
        CameraComponent* component = m_Scene->GetComponent<CameraComponent>(Entity(entityID));
        if (!component) return false;
        component->fieldOfView = std::clamp(fov, 30.0f, 120.0f);
        if (component->active && m_Renderer) m_Renderer->SetCameraFov(component->fieldOfView);
        return true;
    });

    (*m_Environment)["Camera"] =
        camera;

    /*
     * Scene - scene changes are queued until the current Lua update finishes.
     * This avoids destroying the script/scene while its callback is executing.
     */
    // Object-oriented entity handles are represented as plain Lua tables.
    // This keeps method lookup local to each script environment and avoids
    // userdata/metatable conflicts when multiple LuaScript instances bind APIs.
    auto makeEntityHandle = [this](Scene* scene, std::uint32_t id)
    {
        sol::table handle = m_Lua->create_table();
        handle["id"] = id;

        handle.set_function("IsValid", [scene, id](sol::table) { return scene && scene->FindEntityByID(id).IsValid(); });
        handle.set_function("GetID", [id](sol::table) { return id; });
        handle.set_function("GetPosition", [this, scene, id](sol::table) { sol::table t=m_Lua->create_table(); Vec3 v; if(scene){ if(auto* c=scene->GetComponent<TransformComponent>(Entity(id)))v=c->transform.position; } t["x"]=v.x;t["y"]=v.y;t["z"]=v.z;return t; });
        handle.set_function("GetRotation", [this, scene, id](sol::table) { sol::table t=m_Lua->create_table(); Vec3 v; if(scene){ if(auto* c=scene->GetComponent<TransformComponent>(Entity(id)))v=c->transform.rotation; } constexpr float r=57.2957795f;t["x"]=v.x*r;t["y"]=v.y*r;t["z"]=v.z*r;return t; });
        handle.set_function("GetScale", [this, scene, id](sol::table) { sol::table t=m_Lua->create_table(); Vec3 v(1,1,1); if(scene){ if(auto* c=scene->GetComponent<TransformComponent>(Entity(id)))v=c->transform.scale; } t["x"]=v.x;t["y"]=v.y;t["z"]=v.z;return t; });
        handle.set_function("SetPosition", [scene,id](sol::table,float x,float y,float z){ if(scene){ if(auto* c=scene->GetComponent<TransformComponent>(Entity(id)))c->transform.position=Vec3(x,y,z); } });
        handle.set_function("Translate", [scene,id](sol::table,float x,float y,float z){ if(scene){ if(auto* c=scene->GetComponent<TransformComponent>(Entity(id)))c->transform.position=c->transform.position+Vec3(x,y,z); } });
        handle.set_function("SetRotation", [scene,id](sol::table,float x,float y,float z){ if(scene){ constexpr float d=0.0174532925f; if(auto* c=scene->GetComponent<TransformComponent>(Entity(id)))c->transform.rotation=Vec3(x*d,y*d,z*d); } });
        handle.set_function("SetScale", [scene,id](sol::table,float x,float y,float z){ if(scene){ if(auto* c=scene->GetComponent<TransformComponent>(Entity(id)))c->transform.scale=Vec3(x,y,z); } });
        handle.set_function("ClearParent", [scene,id](sol::table,bool keepWorld){ if(scene)scene->ClearParent(Entity(id),keepWorld); });
        handle.set_function("Destroy", [scene,id](sol::table){ if(scene)scene->QueueDestroyEntityHierarchy(Entity(id)); });
        handle.set_function("SetInteractableEnabled", [scene,id](sol::table,bool enabled){ if(scene){ if(auto* c=scene->GetComponent<InteractableComponent>(Entity(id)))c->enabled=enabled; } });
        handle.set_function("SetInteractablePrompt", [scene,id](sol::table,const std::string& prompt){ if(scene){ if(auto* c=scene->GetComponent<InteractableComponent>(Entity(id)))c->prompt=prompt; } });
        handle.set_function("SetLightIntensity", [scene,id](sol::table,float intensity){ if(scene){ if(auto* c=scene->GetComponent<LightComponent>(Entity(id)))c->intensity=intensity; } });
        handle.set_function("SetLightColor", [scene,id](sol::table,float r,float g,float b){ if(scene){ if(auto* c=scene->GetComponent<LightComponent>(Entity(id)))c->color=Vec3(r,g,b); } });
        return handle;
    };

    (*m_Environment)["self"] = makeEntityHandle(m_Scene, m_Entity.GetID());

    sol::table sceneApi = m_Lua->create_table();

    sceneApi.set_function("Load", [this](const std::string& path)
    {
        if (!m_Runtime || path.empty()) return false;
        return m_Runtime->RequestSceneLoad(path);
    });

    sceneApi.set_function("FindEntity", [this, makeEntityHandle](const std::string& name)
    {
        Entity found = m_Scene ? m_Scene->FindEntityByName(name) : Entity();
        return makeEntityHandle(m_Scene, found.GetID());
    });

    sceneApi.set_function("InstantiatePrefab", [this](const std::string& path, std::uint32_t parentID)
    {
        if (!m_Scene) return std::uint32_t(0);
        Entity parent = parentID == 0 ? Entity() : m_Scene->FindEntityByID(parentID);
        const std::string resolved = m_Runtime ? m_Runtime->ResolveProjectPath(path) : path;
        return PrefabSerializer::Instantiate(*m_Scene, resolved, parent).GetID();
    });

    sceneApi.set_function("DuplicateEntity", [this](std::uint32_t entityID, bool includeChildren)
    {
        if (!m_Scene) return std::uint32_t(0);
        return m_Scene->DuplicateEntity(Entity(entityID), includeChildren).GetID();
    });

    sceneApi.set_function("DestroyEntity", [this](std::uint32_t entityID)
    {
        if (m_Scene) m_Scene->DestroyEntityHierarchy(Entity(entityID));
    });

    sceneApi.set_function("GetParent", [this](std::uint32_t entityID)
    {
        if (!m_Scene) return std::uint32_t(0);
        return m_Scene->GetParent(Entity(entityID)).GetID();
    });

    sceneApi.set_function("SetParent", [this](std::uint32_t childID, std::uint32_t parentID, bool keepWorld)
    {
        if (!m_Scene) return false;
        return m_Scene->SetParent(Entity(childID), Entity(parentID), keepWorld);
    });

    sceneApi.set_function("ClearParent", [this](std::uint32_t childID, bool keepWorld)
    {
        if (m_Scene) m_Scene->ClearParent(Entity(childID), keepWorld);
    });

    // Resolve a named attachment point belonging to a specific prefab
    // instance. Unlike global FindEntity, this remains unambiguous when
    // multiple actors carry the same weapon.
    sceneApi.set_function("FindChild", [this](std::uint32_t parentID, const std::string& name)
    {
        if (!m_Scene || parentID == 0 || name.empty()) return std::uint32_t(0);
        for (Entity child : m_Scene->GetChildren(Entity(parentID)))
        {
            const NameComponent* named = m_Scene->GetComponent<NameComponent>(child);
            if (named && named->name == name) return child.GetID();
        }
        return std::uint32_t(0);
    });

    sceneApi.set_function("GetWorldPosition", [this](std::uint32_t entityID)
    {
        sol::table result = m_Lua->create_table();
        const Entity entity = m_Scene ? m_Scene->FindEntityByID(entityID) : Entity();
        const Vec3 value = entity.IsValid()
            ? m_Scene->GetWorldTransform(entity).position : Vec3(0.0f, 0.0f, 0.0f);
        result["x"] = value.x;
        result["y"] = value.y;
        result["z"] = value.z;
        return result;
    });

    sceneApi.set_function("GetPosition", [this](std::uint32_t entityID)
    {
        sol::table result = m_Lua->create_table();
        Entity entity(entityID);
        const TransformComponent* component = m_Scene ? m_Scene->GetComponent<TransformComponent>(entity) : nullptr;
        const Vec3 value = component ? component->transform.position : Vec3(0.0f, 0.0f, 0.0f);
        result["x"]=value.x; result["y"]=value.y; result["z"]=value.z;
        return result;
    });

    sceneApi.set_function("SetPosition", [this](std::uint32_t entityID, float x, float y, float z)
    {
        if (!m_Scene) return false;
        TransformComponent* component=m_Scene->GetComponent<TransformComponent>(Entity(entityID));
        if (!component) return false;
        component->transform.position=Vec3(x,y,z);
        return true;
    });

    sceneApi.set_function("SetRotation", [this](std::uint32_t entityID, float x, float y, float z)
    {
        if (!m_Scene) return false;
        TransformComponent* component=m_Scene->GetComponent<TransformComponent>(Entity(entityID));
        if (!component) return false;
        constexpr float d=0.0174532925f;
        component->transform.rotation=Vec3(x*d,y*d,z*d);
        return true;
    });

    sceneApi.set_function("SetScale", [this](std::uint32_t entityID, float x, float y, float z)
    {
        if (!m_Scene) return false;
        TransformComponent* component=m_Scene->GetComponent<TransformComponent>(Entity(entityID));
        if (!component) return false;
        component->transform.scale=Vec3(x,y,z);
        return true;
    });

    sceneApi.set_function("SetInteractableEnabled", [this](std::uint32_t entityID, bool enabled)
    {
        if (!m_Scene) return false;
        InteractableComponent* component=m_Scene->GetComponent<InteractableComponent>(Entity(entityID));
        if (!component) return false;
        component->enabled=enabled;
        return true;
    });

    sceneApi.set_function("SetInteractablePrompt", [this](std::uint32_t entityID, const std::string& prompt)
    {
        if (!m_Scene) return false;
        InteractableComponent* component=m_Scene->GetComponent<InteractableComponent>(Entity(entityID));
        if (!component) return false;
        component->prompt=prompt;
        return true;
    });

    sceneApi.set_function("SetLightIntensity", [this](std::uint32_t entityID, float intensity)
    {
        if (!m_Scene) return false;
        LightComponent* component=m_Scene->GetComponent<LightComponent>(Entity(entityID));
        if (!component) return false;
        component->intensity=intensity;
        return true;
    });

    sceneApi.set_function("SetLightColor", [this](std::uint32_t entityID, float r, float g, float b)
    {
        if (!m_Scene) return false;
        LightComponent* component=m_Scene->GetComponent<LightComponent>(Entity(entityID));
        if (!component) return false;
        component->color=Vec3(r,g,b);
        return true;
    });

    sceneApi.set_function("GetInteractionPrompt", [this]()
    {
        return m_Runtime ? m_Runtime->GetInteractionPrompt() : std::string();
    });

    sceneApi.set_function("SetPaused", [this](bool paused)
    {
        if (!m_Runtime) return;
        m_Runtime->SetPaused(paused);
    });

    sceneApi.set_function("IsPaused", [this]()
    {
        return m_Runtime ? m_Runtime->IsPaused() : false;
    });

    (*m_Environment)["Scene"] = sceneApi;

    /*
     * Physics queries are engine primitives. Projects decide whether a hit
     * means damage, interaction, selection, AI visibility, etc.
     */
    sol::table physics = m_Lua->create_table();
    physics.set_function("Raycast", [this](float ox,float oy,float oz,float dx,float dy,float dz,float maxDistance,sol::optional<std::uint32_t> ignoreID)
    {
        sol::table result=m_Lua->create_table();
        if(!m_Scene){result["hit"]=false;return result;}
        Entity ignore=ignoreID?Entity(*ignoreID):Entity();
        const RaycastHit hit=CollisionSystem::Raycast(*m_Scene,Vec3(ox,oy,oz),Vec3(dx,dy,dz),maxDistance,ignore);
        result["hit"]=hit.hit;result["entityID"]=hit.hit?hit.entity.GetID():0u;result["distance"]=hit.distance;
        result["x"]=hit.point.x;result["y"]=hit.point.y;result["z"]=hit.point.z;
        result["normalX"]=hit.normal.x;result["normalY"]=hit.normal.y;result["normalZ"]=hit.normal.z;
        return result;
    });
    (*m_Environment)["Physics"] = physics;

    sol::table debug = m_Lua->create_table();
    debug.set_function("DrawLine", [this](float sx,float sy,float sz,float ex,float ey,float ez,sol::optional<float> r,sol::optional<float> g,sol::optional<float> b,sol::optional<float> duration)
    {
        if(m_Renderer)m_Renderer->AddDebugLine(Vec3(sx,sy,sz),Vec3(ex,ey,ez),Vec3(r.value_or(1.0f),g.value_or(0.2f),b.value_or(0.2f)),duration.value_or(0.0f));
    });
    (*m_Environment)["Debug"] = debug;

    /*
     * Networking exposes transport/replication primitives only. Match rules,
     * health, weapons, teams and game state belong to project scripts.
     */
    sol::table network = m_Lua->create_table();
    network.set_function("Host", [this](int port,sol::optional<std::uint32_t> maxPlayers){return m_Runtime&&port>0&&port<=65535&&m_Runtime->GetNetwork().Host(static_cast<std::uint16_t>(port),maxPlayers.value_or(8));});
    network.set_function("Join", [this](const std::string& address,int port){return m_Runtime&&m_Runtime->GetNetwork().Join(address,static_cast<std::uint16_t>(port));});
    network.set_function("Disconnect", [this](){if(m_Runtime)m_Runtime->GetNetwork().Disconnect();});
    network.set_function("IsHost", [this](){return m_Runtime&&m_Runtime->GetNetwork().IsHost();});
    network.set_function("IsConnected", [this](){return m_Runtime&&m_Runtime->GetNetwork().IsConnected();});
    network.set_function("IsReady", [this](){return m_Runtime&&m_Runtime->GetNetwork().IsHandshakeComplete();});
    network.set_function("WasKickedByHost", [this](){return m_Runtime&&m_Runtime->GetNetwork().WasKickedByHost();});
    network.set_function("GetPlayerCount", [this](){return m_Runtime?m_Runtime->GetNetwork().GetPlayerCount():1;});
    network.set_function("GetLastError", [this](){return m_Runtime?m_Runtime->GetNetwork().GetLastError():std::string();});
    network.set_function("GetLocalPlayerID", [this](){return m_Runtime?m_Runtime->GetNetwork().GetLocalPlayerID():std::uint32_t(0);});
    network.set_function("SendMessage", [this](int channel,const std::string& payload){if(m_Runtime)m_Runtime->GetNetwork().SendNetworkMessage(static_cast<std::uint16_t>(std::max(0,std::min(channel,65535))),payload);});
    network.set_function("ConsumeMessages", [this](){sol::table out=m_Lua->create_table();if(!m_Runtime)return out;int i=1;for(const auto& m:m_Runtime->GetNetwork().ConsumeMessages()){sol::table item=m_Lua->create_table();item["senderID"]=m.senderID;item["channel"]=m.channel;item["payload"]=m.payload;out[i++]=item;}return out;});
    network.set_function("SendTransform", [this](float x,float y,float z,float rx,float ry,float rz){if(!m_Runtime)return;NetworkTransformState s{};s.x=x;s.y=y;s.z=z;s.rx=rx;s.ry=ry;s.rz=rz;m_Runtime->GetNetwork().SendLocalTransform(s);});
    network.set_function("GetRemoteTransforms", [this](){sol::table out=m_Lua->create_table();if(!m_Runtime)return out;int i=1;for(const auto& [id,s]:m_Runtime->GetNetwork().GetRemoteTransforms()){sol::table item=m_Lua->create_table();item["playerID"]=id;item["x"]=s.x;item["y"]=s.y;item["z"]=s.z;item["rx"]=s.rx;item["ry"]=s.ry;item["rz"]=s.rz;out[i++]=item;}return out;});
    (*m_Environment)["Network"] = network;

    sol::table controller = m_Lua->create_table();
    controller.set_function("GetLocalID", [this]() { return m_Runtime ? m_Runtime->GetLocalControllerID() : 0u; });
    controller.set_function("Possess", [this](std::uint32_t entityID, sol::optional<std::uint32_t> controllerID)
    {
        if (!m_Runtime || !m_Scene) return false;
        return m_Runtime->PossessPawn(*m_Scene, Entity(entityID), controllerID.value_or(m_Runtime->GetLocalControllerID()));
    });
    controller.set_function("Unpossess", [this](std::uint32_t entityID) { if (m_Runtime && m_Scene) m_Runtime->UnpossessPawn(*m_Scene, Entity(entityID)); });
    controller.set_function("GetPawnControllerID", [this](std::uint32_t entityID)
    {
        if (!m_Scene) return 0u;
        const PawnComponent* pawn = m_Scene->GetComponent<PawnComponent>(Entity(entityID));
        return pawn ? pawn->controllerID : 0u;
    });
    controller.set_function("IsLocallyControlled", [this](std::uint32_t entityID) { return m_Runtime && m_Scene && m_Runtime->IsPawnLocallyControlled(*m_Scene, Entity(entityID)); });
    controller.set_function("GetPlayerStart",[this](sol::optional<std::uint32_t> requestedSlot){sol::table out=m_Lua->create_table();out["valid"]=false;out["entityID"]=0u;out["slot"]=0u;out["x"]=0.0f;out["y"]=0.0f;out["z"]=0.0f;if(!m_Scene)return out;PlayerStartComponent* fallback=nullptr;Entity fallbackEntity;for(const Entity& entity:m_Scene->GetEntities()){auto* start=m_Scene->GetComponent<PlayerStartComponent>(entity);if(!start)continue;if(!fallback||start->slot==0){fallback=start;fallbackEntity=entity;}if(requestedSlot&&start->slot!=*requestedSlot)continue;if(auto* t=m_Scene->GetComponent<TransformComponent>(entity)){out["valid"]=true;out["entityID"]=entity.GetID();out["slot"]=start->slot;out["x"]=t->transform.position.x;out["y"]=t->transform.position.y;out["z"]=t->transform.position.z;return out;}}if(fallback)if(auto* t=m_Scene->GetComponent<TransformComponent>(fallbackEntity)){out["valid"]=true;out["entityID"]=fallbackEntity.GetID();out["slot"]=fallback->slot;out["x"]=t->transform.position.x;out["y"]=t->transform.position.y;out["z"]=t->transform.position.z;}return out;});
    (*m_Environment)["Controller"] = controller;

    /*
     * Graphics settings - intentionally exposed as a small stable API so
     * runtime menus do not need to know about OpenGL implementation details.
     */
    sol::table graphics = m_Lua->create_table();

    graphics.set_function("SetAntiAliasing", [this](bool enabled)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.antiAliasing = enabled;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetShadows", [this](bool enabled)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.shadows = enabled;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetAntiAliasingSamples", [this](int samples)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.antiAliasingSamples = samples;
        settings.antiAliasing = samples > 1;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetShadowQuality", [this](int quality)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.shadowQuality = quality;
        settings.shadows = quality > 0;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetShadowDistance", [this](float distance)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.shadowDistance = distance;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetFog", [this](bool enabled)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.fog = enabled;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetBloom", [this](bool enabled)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.bloom = enabled;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetViewDistance", [this](float distance)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.viewDistance = distance;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetExposure", [this](float exposure)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.exposure = exposure;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetFogDensity", [this](float density)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.fogDensity = density;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("SetBloomStrength", [this](float strength)
    {
        if (!m_Renderer) return;
        RenderSettings settings = m_Renderer->GetRenderSettings();
        settings.bloomStrength = strength;
        m_Renderer->SetRenderSettings(settings);
    });

    graphics.set_function("GetViewDistance", [this]()
    {
        return m_Renderer ? m_Renderer->GetRenderSettings().viewDistance : 1000.0f;
    });

    graphics.set_function("GetAntiAliasingSamples", [this]()
    {
        if (!m_Renderer) return 1;
        const RenderSettings& settings = m_Renderer->GetRenderSettings();
        return settings.antiAliasing ? settings.antiAliasingSamples : 1;
    });

    graphics.set_function("GetFog", [this]()
    {
        return m_Renderer ? m_Renderer->GetRenderSettings().fog : false;
    });

    graphics.set_function("GetBloom", [this]()
    {
        return m_Renderer ? m_Renderer->GetRenderSettings().bloom : false;
    });

    graphics.set_function("GetShadowQuality", [this]()
    {
        return m_Renderer ? m_Renderer->GetRenderSettings().shadowQuality : 0;
    });

    graphics.set_function("Save", [this]()
    {
        if (!m_Renderer || !m_ProjectSettings) return false;
        m_ProjectSettings->SetRenderSettings(m_Renderer->GetRenderSettings());
        return m_ProjectSettings->Save();
    });

    graphics.set_function("SetFOV", [this](float value){ if(m_Renderer)m_Renderer->SetCameraFov(value); });
    graphics.set_function("GetFOV", [this](){ return m_Renderer ? m_Renderer->GetCameraFov() : 60.0f; });

    (*m_Environment)["Graphics"] = graphics;

    // Generic runtime-local state. Projects assign meaning to keys.
    sol::table stateApi=m_Lua->create_table();
    stateApi.set_function("SetNumber",[this](const std::string& key,double value){if(m_Runtime)m_Runtime->SetStateNumber(key,value);});
    stateApi.set_function("GetNumber",[this](const std::string& key,double fallback){return m_Runtime?m_Runtime->GetStateNumber(key,fallback):fallback;});
    stateApi.set_function("SetBool",[this](const std::string& key,bool value){if(m_Runtime)m_Runtime->SetStateBool(key,value);});
    stateApi.set_function("GetBool",[this](const std::string& key,bool fallback){return m_Runtime?m_Runtime->GetStateBool(key,fallback):fallback;});
    (*m_Environment)["State"]=stateApi;

    sol::table audioApi=m_Lua->create_table();
    audioApi.set_function("SetMasterVolume",[this](float v){if(m_Runtime&&m_Runtime->GetAudioEngine())m_Runtime->GetAudioEngine()->SetMasterVolume(v);});
    audioApi.set_function("GetMasterVolume",[this](){return (m_Runtime&&m_Runtime->GetAudioEngine())?m_Runtime->GetAudioEngine()->GetMasterVolume():1.0f;});
    audioApi.set_function("SetSFXVolume",[this](float v){if(m_Runtime&&m_Runtime->GetAudioEngine())m_Runtime->GetAudioEngine()->SetSFXVolume(v);});
    audioApi.set_function("GetSFXVolume",[this](){return (m_Runtime&&m_Runtime->GetAudioEngine())?m_Runtime->GetAudioEngine()->GetSFXVolume():1.0f;});
    audioApi.set_function("PlayFootstep",[this](const std::string& surface,bool sprint){
        return m_Runtime && m_Runtime->GetAudioEngine() &&
               m_Runtime->GetAudioEngine()->PlayFootstep(surface,sprint);
    });
    audioApi.set_function("PlaySFX",[this](const std::string& path,float volume){
        if(!m_Runtime||!m_Runtime->GetAudioEngine()) return false;
        AudioEngine* audio=m_Runtime->GetAudioEngine();
        return audio->PlaySound(path,volume*audio->GetSFXVolume());
    });
    audioApi.set_function("SetUIVolume",[this](float v){if(m_Runtime&&m_Runtime->GetAudioEngine())m_Runtime->GetAudioEngine()->SetUIVolume(v);});
    audioApi.set_function("GetUIVolume",[this](){return (m_Runtime&&m_Runtime->GetAudioEngine())?m_Runtime->GetAudioEngine()->GetUIVolume():1.0f;});
    (*m_Environment)["Audio"]=audioApi;

    /*
     * UI - edits the same widget tree used by the editor and renderer.
     */
    sol::table ui = m_Lua->create_table();

    ui.set_function("Load", [this](const std::string& path)
    {
        if (!m_UICanvas) return false;
        const std::string resolved = m_Runtime ? m_Runtime->ResolveProjectPath(path) : path;
        return UISerializer::Load(*m_UICanvas, resolved);
    });

    ui.set_function("Clear", [this]()
    {
        if (!m_UICanvas) return;
        m_UICanvas->Clear();
    });

    ui.set_function("IsLoaded", [this](const std::string& name)
    {
        return m_UICanvas && m_UICanvas->GetRoot() &&
            m_UICanvas->GetRoot()->Find(name) != nullptr;
    });

    ui.set_function("SetVisible", [this](const std::string& name, bool visible)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return false;
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        if (!widget) return false;
        widget->SetVisible(visible);
        return true;
    });

    ui.set_function("SetEnabled", [this](const std::string& name, bool enabled)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return false;
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        if (!widget) return false;
        widget->SetEnabled(enabled);
        return true;
    });

    ui.set_function("SetText", [this](const std::string& name, const std::string& value)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return false;
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        if (!widget) return false;
        if (UIText* text = dynamic_cast<UIText*>(widget)) { text->SetText(value); return true; }
        if (UITextInput* input = dynamic_cast<UITextInput*>(widget)) { input->SetText(value); return true; }
        return false;
    });

    ui.set_function("GetText", [this](const std::string& name)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return std::string();
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        if (!widget) return std::string();
        if (UIText* text = dynamic_cast<UIText*>(widget)) return text->GetText();
        if (UITextInput* input = dynamic_cast<UITextInput*>(widget)) return input->GetText();
        return std::string();
    });

    ui.set_function("IsFocused", [this](const std::string& name)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return false;
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        UITextInput* input = widget ? dynamic_cast<UITextInput*>(widget) : nullptr;
        return input ? input->IsFocused() : false;
    });

    ui.set_function("SetColor", [this](const std::string& name, float r, float g, float b, float a)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return false;
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        if (!widget) return false;
        widget->SetColor(Vec4(r, g, b, a));
        return true;
    });

    ui.set_function("SetPosition", [this](const std::string& name, float x, float y)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return false;
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        if (!widget) return false;
        widget->SetPosition(Vec2(x, y));
        return true;
    });

    ui.set_function("SetValue", [this](const std::string& name, float value)
    {
        if(!m_UICanvas||!m_UICanvas->GetRoot())return false;
        UISlider* slider=dynamic_cast<UISlider*>(m_UICanvas->GetRoot()->Find(name));
        if(!slider)return false; slider->SetValue(value); return true;
    });
    ui.set_function("GetValue", [this](const std::string& name)
    {
        if(!m_UICanvas||!m_UICanvas->GetRoot())return 0.0f;
        UISlider* slider=dynamic_cast<UISlider*>(m_UICanvas->GetRoot()->Find(name));
        return slider?slider->GetValue():0.0f;
    });

    ui.set_function("WasClicked", [this](const std::string& name)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return false;
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        UIButton* button = widget ? dynamic_cast<UIButton*>(widget) : nullptr;
        return button ? button->ConsumeClick() : false;
    });

    ui.set_function("IsHovered", [this](const std::string& name)
    {
        if (!m_UICanvas || !m_UICanvas->GetRoot()) return false;
        UIWidget* widget = m_UICanvas->GetRoot()->Find(name);
        UIButton* button = widget ? dynamic_cast<UIButton*>(widget) : nullptr;
        return button ? button->IsHovered() : false;
    });

    (*m_Environment)["UI"] = ui;

    /*
     * Character Controller
     */
    sol::table characterController =
        m_Lua->create_table();

    characterController.set_function(
        "Move",
        [this](
            float x,
            float z)
        {
            if (m_Scene == nullptr)
            {
                return;
            }

            CharacterControllerComponent* controller =
                m_Scene->GetComponent<
                CharacterControllerComponent
                >(m_Entity);

            if (controller == nullptr)
            {
                return;
            }

            controller->horizontalVelocityX =
                x;

            controller->horizontalVelocityZ =
                z;
        }
    );

    characterController.set_function(
        "Jump",
        [this]()
        {
            if (m_Scene == nullptr)
            {
                return;
            }

            CharacterControllerComponent* controller =
                m_Scene->GetComponent<
                CharacterControllerComponent
                >(m_Entity);

            if (controller == nullptr)
            {
                return;
            }

            if (controller->grounded)
            {
                controller->jumpRequested =
                    true;
            }
        }
    );

    characterController.set_function(
        "IsGrounded",
        [this]()
        {
            if (m_Scene == nullptr)
            {
                return false;
            }

            CharacterControllerComponent* controller =
                m_Scene->GetComponent<
                CharacterControllerComponent
                >(m_Entity);

            if (controller == nullptr)
            {
                return false;
            }

            return controller->grounded;
        }
    );

    (*m_Environment)["CharacterController"] =
        characterController;

    /*
     * Collider
     */
    sol::table collider =
        m_Lua->create_table();

    collider.set_function(
        "IsEnabled",
        [this]()
        {
            if (m_Scene == nullptr)
            {
                return false;
            }

            ColliderComponent* component =
                m_Scene->GetComponent<
                ColliderComponent
                >(m_Entity);

            if (component == nullptr)
            {
                return false;
            }

            return component->enabled;
        }
    );

    collider.set_function(
        "SetEnabled",
        [this](bool enabled)
        {
            if (m_Scene == nullptr)
            {
                return;
            }

            ColliderComponent* component =
                m_Scene->GetComponent<
                ColliderComponent
                >(m_Entity);

            if (component == nullptr)
            {
                return;
            }

            component->enabled =
                enabled;
        }
    );

    (*m_Environment)["Collider"] =
        collider;

    /*
     * Mesh
     */
    sol::table mesh =
        m_Lua->create_table();

    mesh.set_function(
        "GetRotation",
        [this]()
        {
            sol::table result =
                m_Lua->create_table();

            if (m_Scene == nullptr)
            {
                result["x"] = 0.0f;
                result["y"] = 0.0f;
                result["z"] = 0.0f;

                return result;
            }

            MeshComponent* meshComponent =
                m_Scene->GetComponent<
                MeshComponent
                >(m_Entity);

            if (meshComponent == nullptr)
            {
                result["x"] = 0.0f;
                result["y"] = 0.0f;
                result["z"] = 0.0f;

                return result;
            }

            result["x"] =
                meshComponent->rotation.x;

            result["y"] =
                meshComponent->rotation.y;

            result["z"] =
                meshComponent->rotation.z;

            return result;
        }
    );

    mesh.set_function(
        "SetRotation",
        [this](
            float x,
            float y,
            float z)
        {
            if (m_Scene == nullptr)
            {
                return;
            }

            MeshComponent* meshComponent =
                m_Scene->GetComponent<
                MeshComponent
                >(m_Entity);

            if (meshComponent == nullptr)
            {
                return;
            }

            meshComponent->rotation.x =
                x;

            meshComponent->rotation.y =
                y;

            meshComponent->rotation.z =
                z;
        }
    );

    (*m_Environment)["Mesh"] =
        mesh;

    /*
     * Interactable
     */
    sol::table interactable =
        m_Lua->create_table();

    interactable.set_function(
        "SetPrompt",
        [this](
            const std::string& prompt)
        {
            if (m_Scene == nullptr)
            {
                return;
            }

            InteractableComponent* component =
                m_Scene->GetComponent<
                InteractableComponent
                >(m_Entity);

            if (component == nullptr)
            {
                return;
            }

            component->prompt =
                prompt;
        }
    );

    (*m_Environment)["Interactable"] =
        interactable;

    // Legacy renderer-driven UI API removed.
    // UI now refers exclusively to the shared UICanvas widget tree.

}
