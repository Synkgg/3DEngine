#include "VelcrynBrand.h"

namespace Velcryn::Editor::Brand
{
    void DrawLogoMark(ImDrawList* dl, ImVec2 p, float s)
    {
        // Source silhouette follows the approved Velcryn brand board: broad
        // outward shoulders, a deep split crown, crossed inner blades and a
        // long tapered lower point. Facets intentionally overlap like folded metal.
        auto P=[&](float x,float y){return ImVec2(p.x+x*s,p.y+y*s);};
        const ImU32 silver0=IM_COL32(239,243,247,255), silver1=IM_COL32(174,190,205,255);
        const ImU32 steel=IM_COL32(92,112,132,255), dark=IM_COL32(38,55,72,255);
        const ImU32 blue=IM_COL32(48,164,238,255), glow=IM_COL32(91,211,255,255);

        // Left blade: wide shoulder -> inner crossing -> lower point.
        dl->AddQuadFilled(P(.03,.08),P(.31,.17),P(.53,.60),P(.38,.45),silver0);
        dl->AddTriangleFilled(P(.03,.08),P(.38,.45),P(.24,.30),silver1);
        dl->AddTriangleFilled(P(.31,.17),P(.53,.60),P(.42,.28),steel);
        // Right blade is higher and sharper, matching the reference asymmetry.
        dl->AddQuadFilled(P(.97,.03),P(.69,.15),P(.47,.60),P(.61,.43),silver0);
        dl->AddTriangleFilled(P(.97,.03),P(.61,.43),P(.78,.27),silver1);
        dl->AddTriangleFilled(P(.69,.15),P(.47,.60),P(.59,.27),dark);
        // Central folded spear / long lower point.
        dl->AddTriangleFilled(P(.38,.45),P(.53,.60),P(.47,.96),steel);
        dl->AddTriangleFilled(P(.61,.43),P(.47,.96),P(.53,.60),IM_COL32(29,72,108,255));
        dl->AddTriangleFilled(P(.42,.28),P(.59,.27),P(.53,.60),blue);
        // Illuminated inner seams from the board, not an outline around the mark.
        dl->AddLine(P(.31,.17),P(.53,.60),glow,1.6f);
        dl->AddLine(P(.69,.15),P(.47,.60),glow,1.4f);
        dl->AddLine(P(.53,.60),P(.47,.96),blue,1.2f);
    }
}
