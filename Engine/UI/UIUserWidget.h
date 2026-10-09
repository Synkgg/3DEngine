#pragma once
#include "UIWidget.h"
#include <string>

// A reusable Widget Blueprint instance. The source .ui file is expanded into
// live children by UISerializer at load time. Instances retain only the asset
// reference when saved, so changes to the source propagate to every screen.
class UIUserWidget final : public UIWidget
{
public:
    UIUserWidget() : UIWidget(UIWidgetType::UserWidget)
    {
        SetColor(Vec4(0.0f, 0.0f, 0.0f, 0.0f));
        SetHitTestVisible(false);
    }

    const std::string& GetSourcePath() const { return m_SourcePath; }
    void SetSourcePath(const std::string& path) { m_SourcePath = path; }

    // Optional per-instance event target: reuse the same visual asset with
    // different controller scripts (e.g. lobby vs. in-game loadout).
    const std::string& GetEventScriptOverride() const { return m_EventScriptOverride; }
    void SetEventScriptOverride(const std::string& path) { m_EventScriptOverride = path; }

private:
    std::string m_SourcePath;
    std::string m_EventScriptOverride;
};
