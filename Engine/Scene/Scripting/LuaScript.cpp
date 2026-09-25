#include "LuaScript.h"

#include "../../Core/Logger.h"

#include "../../Platform/SDL/Input.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"
#include "../../UI/UICanvas.h"
#include "../../UI/UIWidget.h"
#include "../../UI/UIText.h"
#include "../../UI/UIButton.h"
#include "../../UI/UISerializer.h"

#include "../Scene.h"
#include "../PrefabSerializer.h"
#include "../Runtime/Runtime.h"
#include "../Components/TransformComponent.h"
#include "../Components/CharacterControllerComponent.h"
#include "../Components/MeshComponent.h"
#include "../Components/ColliderComponent.h"
#include "../Components/InteractableComponent.h"
#include "../Components/LightComponent.h"

LuaScript::LuaScript()
{
}

void LuaScript::Initialize(
    Entity entity,
    Scene& scene,
    Input& input,
    Renderer& renderer,
    UICanvas& uiCanvas,
    sol::state& lua,
    Runtime* runtime)
{
    m_Entity = entity;
    m_Scene = &scene;
    m_Input = &input;
    m_Renderer = &renderer;
    m_UICanvas = &uiCanvas;
    m_Lua = &lua;
    m_Runtime = runtime;

    m_Environment =
        std::make_unique<
        sol::environment
        >(
            lua,
            sol::create,
            lua.globals()
        );

    BindEngineAPI();
}

bool LuaScript::Load(
    const std::string& filepath,
    const std::unordered_map<std::string, ScriptPropertyValue>* propertyOverrides)
{
    if (m_Lua == nullptr ||
        m_Environment == nullptr)
    {
        Logger::Error(
            "LuaScript is not initialized."
        );

        return false;
    }

    sol::load_result result =
        m_Lua->load_file(filepath);

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Failed to load Lua script: "
            ) +
            filepath +
            " - " +
            error.what()
        );

        return false;
    }

    sol::protected_function script =
        result;

    sol::set_environment(
        *m_Environment,
        script
    );

    sol::protected_function_result execution =
        script();

    if (!execution.valid())
    {
        sol::error error =
            execution;

        Logger::Error(
            std::string(
                "Failed to execute Lua script: "
            ) +
            filepath +
            " - " +
            error.what()
        );

        return false;
    }

    // Scripts declare editor-facing defaults in a global Properties table.
    // Instance overrides are injected after the script executes but before OnCreate.
    if (propertyOverrides != nullptr)
    {
        sol::object propertiesObject = (*m_Environment)["Properties"];
        if (propertiesObject.is<sol::table>())
        {
            sol::table properties = propertiesObject.as<sol::table>();
            for (const auto& [name, property] : *propertyOverrides)
            {
                switch (property.type)
                {
                case ScriptPropertyType::Number:
                    try { properties[name] = std::stod(property.value); } catch (...) {}
                    break;
                case ScriptPropertyType::Boolean:
                    properties[name] = (property.value == "1" || property.value == "true");
                    break;
                case ScriptPropertyType::Entity:
                    try { properties[name] = static_cast<std::uint32_t>(std::stoul(property.value)); } catch (...) { properties[name] = std::uint32_t(0); }
                    break;
                case ScriptPropertyType::String:
                default:
                    properties[name] = property.value;
                    break;
                }
            }
        }
    }

    sol::object onCreate =
        (*m_Environment)["OnCreate"];

    if (onCreate.is<
        sol::protected_function>())
    {
        m_OnCreate =
            onCreate.as<
            sol::protected_function
            >();
    }

    sol::object onUpdate =
        (*m_Environment)["OnUpdate"];

    if (onUpdate.is<
        sol::protected_function>())
    {
        m_OnUpdate =
            onUpdate.as<
            sol::protected_function
            >();
    }

    sol::object onDestroy =
        (*m_Environment)["OnDestroy"];

    if (onDestroy.is<
        sol::protected_function>())
    {
        m_OnDestroy =
            onDestroy.as<
            sol::protected_function
            >();
    }

    sol::object onInteract =
        (*m_Environment)["OnInteract"];

    if (onInteract.is<
        sol::protected_function>())
    {
        m_OnInteract =
            onInteract.as<
            sol::protected_function
            >();
    }

    return true;
}

bool LuaScript::Create()
{
    if (!m_OnCreate.valid())
    {
        return true;
    }

    sol::protected_function_result result =
        m_OnCreate();

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Lua OnCreate error: "
            ) +
            error.what()
        );

        return false;
    }

    return true;
}

bool LuaScript::Update(
    float deltaTime)
{
    m_DeltaTime =
        deltaTime;

    if (!m_OnUpdate.valid())
    {
        return true;
    }

    sol::protected_function_result result =
        m_OnUpdate(deltaTime);

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Lua OnUpdate error: "
            ) +
            error.what()
        );

        return false;
    }

    return true;
}

void LuaScript::Interact()
{
    if (!m_OnInteract.valid())
    {
        return;
    }

    sol::protected_function_result result =
        m_OnInteract();

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Lua OnInteract error: "
            ) +
            error.what()
        );
    }
}

bool LuaScript::Destroy()
{
    if (!m_OnDestroy.valid())
    {
        return true;
    }

    sol::protected_function_result result =
        m_OnDestroy();

    if (!result.valid())
    {
        sol::error error =
            result;

        Logger::Error(
            std::string(
                "Lua OnDestroy error: "
            ) +
            error.what()
        );

        return false;
    }

    return true;
}

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

    (*m_Environment)["Input"] =
        input;

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

    (*m_Environment)["Camera"] =
        camera;

    /*
     * Scene - scene changes are queued until the current Lua update finishes.
     * This avoids destroying the script/scene while its callback is executing.
     */
    // Object-oriented entity handle for readable project Lua:
    // local cube = Scene.FindEntity("Cube"); cube:SetPosition(1, 2, 3)
    struct LuaEntityHandle { Scene* scene=nullptr; std::uint32_t id=0; };
    m_Lua->new_usertype<LuaEntityHandle>("Entity",
        sol::constructors<LuaEntityHandle()>(),
        "IsValid", [](const LuaEntityHandle& e){ return e.scene && e.scene->FindEntityByID(e.id).IsValid(); },
        "GetID", [](const LuaEntityHandle& e){ return e.id; },
        "GetPosition", [this](const LuaEntityHandle& e){ sol::table t=m_Lua->create_table(); Vec3 v; if(e.scene){ if(auto* c=e.scene->GetComponent<TransformComponent>(Entity(e.id)))v=c->transform.position; } t["x"]=v.x;t["y"]=v.y;t["z"]=v.z;return t; },
        "GetRotation", [this](const LuaEntityHandle& e){ sol::table t=m_Lua->create_table(); Vec3 v; if(e.scene){ if(auto* c=e.scene->GetComponent<TransformComponent>(Entity(e.id)))v=c->transform.rotation; } constexpr float r=57.2957795f;t["x"]=v.x*r;t["y"]=v.y*r;t["z"]=v.z*r;return t; },
        "GetScale", [this](const LuaEntityHandle& e){ sol::table t=m_Lua->create_table(); Vec3 v(1,1,1); if(e.scene){ if(auto* c=e.scene->GetComponent<TransformComponent>(Entity(e.id)))v=c->transform.scale; } t["x"]=v.x;t["y"]=v.y;t["z"]=v.z;return t; },
        "GetParent", [](const LuaEntityHandle& e){ if(!e.scene)return LuaEntityHandle{}; Entity p=e.scene->GetParent(Entity(e.id)); return LuaEntityHandle{e.scene,p.GetID()}; },
        "Duplicate", [](const LuaEntityHandle& e,bool children){ if(!e.scene)return LuaEntityHandle{}; Entity copy=e.scene->DuplicateEntity(Entity(e.id),children); return LuaEntityHandle{e.scene,copy.GetID()}; },
        "HasLight", [](const LuaEntityHandle& e){ return e.scene && e.scene->HasComponent<LightComponent>(Entity(e.id)); },
        "HasInteractable", [](const LuaEntityHandle& e){ return e.scene && e.scene->HasComponent<InteractableComponent>(Entity(e.id)); },
        "SetPosition", [](LuaEntityHandle& e,float x,float y,float z){ if(!e.scene)return; if(auto* c=e.scene->GetComponent<TransformComponent>(Entity(e.id))) c->transform.position=Vec3(x,y,z); },
        "Translate", [](LuaEntityHandle& e,float x,float y,float z){ if(!e.scene)return; if(auto* c=e.scene->GetComponent<TransformComponent>(Entity(e.id))) c->transform.position=c->transform.position+Vec3(x,y,z); },
        "SetRotation", [](LuaEntityHandle& e,float x,float y,float z){ if(!e.scene)return; constexpr float d=0.0174532925f; if(auto* c=e.scene->GetComponent<TransformComponent>(Entity(e.id))) c->transform.rotation=Vec3(x*d,y*d,z*d); },
        "SetScale", [](LuaEntityHandle& e,float x,float y,float z){ if(!e.scene)return; if(auto* c=e.scene->GetComponent<TransformComponent>(Entity(e.id))) c->transform.scale=Vec3(x,y,z); },
        "SetParent", [](LuaEntityHandle& e,const LuaEntityHandle& p,bool keepWorld){ return e.scene && p.scene==e.scene && e.scene->SetParent(Entity(e.id),Entity(p.id),keepWorld); },
        "ClearParent", [](LuaEntityHandle& e,bool keepWorld){ if(e.scene)e.scene->ClearParent(Entity(e.id),keepWorld); },
        "Destroy", [](LuaEntityHandle& e){ if(e.scene)e.scene->DestroyEntityHierarchy(Entity(e.id)); e.id=0; },
        "SetInteractableEnabled", [](LuaEntityHandle& e,bool enabled){ if(!e.scene)return; if(auto* c=e.scene->GetComponent<InteractableComponent>(Entity(e.id)))c->enabled=enabled; },
        "SetInteractablePrompt", [](LuaEntityHandle& e,const std::string& prompt){ if(!e.scene)return; if(auto* c=e.scene->GetComponent<InteractableComponent>(Entity(e.id)))c->prompt=prompt; },
        "SetLightIntensity", [](LuaEntityHandle& e,float intensity){ if(!e.scene)return; if(auto* c=e.scene->GetComponent<LightComponent>(Entity(e.id)))c->intensity=intensity; },
        "SetLightColor", [](LuaEntityHandle& e,float r,float g,float b){ if(!e.scene)return; if(auto* c=e.scene->GetComponent<LightComponent>(Entity(e.id)))c->color=Vec3(r,g,b); }
    );

    sol::table sceneApi = m_Lua->create_table();

    sceneApi.set_function("Load", [this](const std::string& path)
    {
        if (!m_Runtime || path.empty()) return false;
        return m_Runtime->RequestSceneLoad(path);
    });

    sceneApi.set_function("FindEntity", [this](const std::string& name) -> LuaEntityHandle
    {
        if (!m_Scene) return LuaEntityHandle{};
        Entity found = m_Scene->FindEntityByName(name);
        return LuaEntityHandle{ m_Scene, found.GetID() };
    });

    sceneApi.set_function("InstantiatePrefab", [this](const std::string& path, std::uint32_t parentID)
    {
        if (!m_Scene) return std::uint32_t(0);
        Entity parent = parentID == 0 ? Entity() : m_Scene->FindEntityByID(parentID);
        return PrefabSerializer::Instantiate(*m_Scene, path, parent).GetID();
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

    (*m_Environment)["Graphics"] = graphics;

    /*
     * UI - edits the same widget tree used by the editor and renderer.
     */
    sol::table ui = m_Lua->create_table();

    ui.set_function("Load", [this](const std::string& path)
    {
        if (!m_UICanvas) return false;
        return UISerializer::Load(*m_UICanvas, path);
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
        UIText* text = widget ? dynamic_cast<UIText*>(widget) : nullptr;
        if (!text) return false;
        text->SetText(value);
        return true;
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