#include "RotatorScript.h"

#include "../../Scene.h"
#include "../../Components/TransformComponent.h"

void RotatorScript::OnUpdate(float deltaTime)
{
    Scene* scene =
        GetScene();

    if (scene == nullptr)
    {
        return;
    }

    TransformComponent* transform =
        scene->GetComponent<TransformComponent>(
            GetEntity()
        );

    if (transform == nullptr)
    {
        return;
    }

    transform->transform.rotation.y +=
        m_RotationSpeed * deltaTime;
}