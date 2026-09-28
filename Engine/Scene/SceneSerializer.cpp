#include "SceneSerializer.h"
#include "Scene.h"

#include "../Editor/HierarchyFolder.h"

#include "../Graphics/PrimitiveType.h"

#include "Components/TransformComponent.h"
#include "Components/MeshComponent.h"
#include "Components/ColorComponent.h"
#include "Components/NameComponent.h"
#include "Components/PawnComponent.h"
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

SceneSerializer::SceneSerializer(
    Scene& scene)
    : m_Scene(scene)
{
}


