#pragma once

struct CharacterControllerComponent
{
    float gravity = 9.81f;
    float jumpForce = 5.0f;

    float verticalVelocity = 0.0f;

    float horizontalVelocityX = 0.0f;
    float horizontalVelocityZ = 0.0f;

    bool jumpRequested = false;
    bool grounded = true;
};