#pragma once

#include <cstdint>

// Generic controllable/owned entity, analogous to a Pawn.
// Projects define input, camera, movement and gameplay behavior.
struct PawnComponent
{
    std::uint32_t controllerID = 0;
};
