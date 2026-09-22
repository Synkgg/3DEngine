#include "LuaScript.h"

#include "../../Core/Logger.h"

#include "../../Platform/SDL/Input.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"

#include "../Scene.h"
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
    sol::state& lua)
{
    m_Entity = entity;
    m_Scene = &scene;
    m_Input = &input;
    m_Renderer = &renderer;
    m_Lua = &lua;

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

    /*
 * UI
 */
    sol::table ui =
        m_Lua->create_table();

    ui.set_function(
        "SetRect",
        [this](
            const std::string& id,
            float x,
            float y,
            float width,
            float height,
            float red,
            float green,
            float blue,
            float alpha)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->GetUIRenderer().SetRect(
                id,
                x,
                y,
                width,
                height,
                red,
                green,
                blue,
                alpha
            );
        }
    );

    ui.set_function(
        "SetText",
        [this](
            const std::string& id,
            const std::string& text,
            float x,
            float y,
            float scale,
            float red,
            float green,
            float blue,
            float alpha)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->GetUIRenderer().SetText(
                id,
                text,
                x,
                y,
                scale,
                red,
                green,
                blue,
                alpha
            );
        }
    );

    ui.set_function(
        "SetImage",
        [this](
            const std::string& id,
            const std::string& path,
            float x,
            float y,
            float width,
            float height,
            float red,
            float green,
            float blue,
            float alpha)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            Texture2D* texture =
                m_Renderer->LoadTexture(
                    path
                );

            if (texture == nullptr ||
                !texture->IsLoaded())
            {
                return;
            }

            m_Renderer->GetUIRenderer().SetImage(
                id,
                texture,
                x,
                y,
                width,
                height,
                red,
                green,
                blue,
                alpha
            );
        }
    );

    ui.set_function(
        "SetVisible",
        [this](
            const std::string& id,
            bool visible)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->GetUIRenderer().SetVisible(
                id,
                visible
            );
        }
    );

    ui.set_function(
        "SetParent",
        [this](
            const std::string& id,
            const std::string& parentId)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->GetUIRenderer().SetParent(
                id,
                parentId
            );
        }
    );

    ui.set_function(
        "Remove",
        [this](
            const std::string& id)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->GetUIRenderer().Remove(
                id
            );
        }
    );

    ui.set_function(
        "Clear",
        [this]()
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->GetUIRenderer().Clear();
        }
    );

    ui.set_function(
        "SetMouseInteractionEnabled",
        [this](bool enabled)
        {
            if (m_Renderer == nullptr)
            {
                return;
            }

            m_Renderer->GetUIRenderer()
                .SetMouseInteractionEnabled(
                    enabled
                );
        }
    );

    ui.set_function(
        "IsMouseInteractionEnabled",
        [this]()
        {
            if (m_Renderer == nullptr)
            {
                return false;
            }

            return m_Renderer->GetUIRenderer()
                .IsMouseInteractionEnabled();
        }
    );

    (*m_Environment)["UI"] =
        ui;
}