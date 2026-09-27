#pragma once
#include "../../Math/Vec3.h"
struct PointLightData { Vec3 position; Vec3 color; float intensity=1.0f; float range=10.0f; };
struct SpotLightData { Vec3 position; Vec3 direction; Vec3 color; float intensity=1.0f; float range=15.0f; float innerCos=0.92f; float outerCos=0.82f; };
