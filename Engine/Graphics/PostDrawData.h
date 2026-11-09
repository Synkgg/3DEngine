#pragma once
struct PostDrawData {
    float cameraForward[4]{};
    float cameraRight[4]{};
    float cameraUp[4]{};
    float cameraPosition[4]{};
    float sunDirection[4]{};
    float sunColor[4]{};
    float params0[4]{};
    float params1[4]{};
    float params2[4]{};
    float params3[4]{};
    float previousForward[4]{};
    float previousRight[4]{};
    float previousUp[4]{};
    float previousPosition[4]{};
    float previousParams[4]{};
};
static_assert(sizeof(PostDrawData)==240);
