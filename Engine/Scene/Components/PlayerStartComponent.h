#pragma once
#include <cstdint>

// Generic Pawn spawn marker, analogous to Unreal Engine's PlayerStart.
// Projects decide when to spawn/respawn and how slots are assigned.
struct PlayerStartComponent { std::uint32_t slot = 0; };
