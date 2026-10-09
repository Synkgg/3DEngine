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

private:
    std::string m_SourcePath;
};
