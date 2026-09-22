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
#include "../Runtime/Runtime.h"
#include "../Components/TransformComponent.h"
#include "../Components/CharacterControllerComponent.h"
#include "../Components/MeshComponent.h"
#include "../Components/ColliderComponent.h"
#include "../Components/InteractableComponent.h"

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
    const std::string& filepath)
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
    sol::table sceneApi = m_Lua->create_table();

    sceneApi.set_function("Load", [this](const std::string& path)
    {
        if (!m_Runtime || path.empty()) return false;
        return m_Runtime->RequestSceneLoad(path);
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