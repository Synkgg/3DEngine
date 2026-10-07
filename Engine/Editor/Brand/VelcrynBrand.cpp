#include "VelcrynBrand.h"

namespace Velcryn::Editor::Brand
{
    void DrawLogoMark(ImDrawList* drawList, ImVec2 position, float size)
    {
        const ImU32 white = IM_COL32(255, 255, 255, 255);

        const auto point = [position, size](float x, float y)
        {
            return ImVec2(position.x + x * size, position.y + y * size);
        };

        // Monochrome Velcryn mark. These pieces follow the supplied white
        // V-wing emblem rather than the earlier metallic/cyan brand-board mark.
        const ImVec2 leftBlade[] =
        {
            point(0.03f, 0.08f),
            point(0.36f, 0.29f),
            point(0.51f, 0.64f),
            point(0.51f, 0.96f)
        };

        const ImVec2 rightBlade[] =
        {
            point(0.97f, 0.06f),
            point(0.68f, 0.27f),
            point(0.51f, 0.64f),
            point(0.55f, 0.78f),
            point(0.73f, 0.57f)
        };

        const ImVec2 lowerFacet[] =
        {
            point(0.55f, 0.69f),
            point(0.72f, 0.59f),
            point(0.62f, 0.80f),
            point(0.56f, 0.75f)
        };

        const ImVec2 lowerTip[] =
        {
            point(0.52f, 0.67f),
            point(0.59f, 0.82f),
            point(0.52f, 0.94f)
        };

        drawList->AddConvexPolyFilled(leftBlade, 4, white);
        drawList->AddConvexPolyFilled(rightBlade, 5, white);
        drawList->AddConvexPolyFilled(lowerFacet, 4, white);
        drawList->AddTriangleFilled(lowerTip[0], lowerTip[1], lowerTip[2], white);
    }
}
