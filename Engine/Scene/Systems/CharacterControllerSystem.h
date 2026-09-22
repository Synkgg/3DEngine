#pragma once

class Scene;
class Renderer;
class Input;

class CharacterControllerSystem
{
public:
    void Update(
        Scene& scene,
        Renderer& renderer,
        Input& input,
        float deltaTime
    );
};