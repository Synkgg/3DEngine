#pragma once

#include "../Script.h"

class RotatorScript : public Script
{
public:
    void OnUpdate(float deltaTime) override;

private:
    float m_RotationSpeed = 2.0f;
};