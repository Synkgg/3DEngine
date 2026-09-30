#include "Application.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include "../Scene/Entity.h"
#include "../Scene/Components/TransformComponent.h"
#include "../Scene/Components/MeshComponent.h"
#include "../Scene/Components/ColorComponent.h"
#include "../Scene/Components/LightComponent.h"
#include "../Scene/Components/ColliderComponent.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Scene/Components/MaterialComponent.h"
#include "../Graphics/PrimitiveType.h"
#include "../Core/Logger.h"
#include "../UI/UISerializer.h"
#include "../UI/UIText.h"

void Application::StartRuntime()
{
    m_Renderer.GetUIRenderer().Clear();

    // Runtime UI starts empty every time. Lua decides which UI asset is active.
    // This prevents an editor-opened UI from leaking into every scene.
    m_UICanvas.Clear();

    m_Runtime.SaveCameraState(m_Renderer);

    m_Runtime.Start(
        m_Scene,
        m_Renderer,
        m_Input,
        m_UICanvas
    );
}

void Application::StopRuntime()
{
    m_Renderer.GetUIRenderer().Clear();

    m_Runtime.Stop(
        m_Scene
    );

    m_Runtime.RestoreCameraState(
        m_Renderer
    );
}

void Application::UpdateLighting()
{
    LightComponent* directional = nullptr;
    m_Renderer.ClearLocalLights();

    for (const Entity& entity : m_Scene.GetEntities())
    {
        LightComponent* light = m_Scene.GetComponent<LightComponent>(entity);
        TransformComponent* transform = m_Scene.GetComponent<TransformComponent>(entity);
        if (!light) continue;

        if (light->type == LightType::Directional && directional == nullptr)
        {
            directional = light;
        }
        else if (light->type == LightType::Point && transform)
        {
            PointLightData data;
            data.position = m_Scene.GetWorldTransform(entity).position;
            data.color = light->color;
            data.intensity = light->intensity;
            data.range = light->range;
            m_Renderer.AddPointLight(data);
        }
        else if (light->type == LightType::Spot && transform)
        {
            SpotLightData data;
            data.position = m_Scene.GetWorldTransform(entity).position;
            data.direction = light->direction;
            data.color = light->color;
            data.intensity = light->intensity;
            data.range = light->range;
            data.innerCos = std::cos(light->innerAngle * 0.0174532925f);
            data.outerCos = std::cos(light->outerAngle * 0.0174532925f);
            m_Renderer.AddSpotLight(data);
        }
    }

    if (directional)
        m_Renderer.SetDirectionalLight(directional->direction, directional->color, directional->intensity);
    else
        m_Renderer.SetDirectionalLight(Vec3(-0.5f,-1.0f,-0.5f), Vec3(1.0f,0.95f,0.88f), 1.0f);
}
