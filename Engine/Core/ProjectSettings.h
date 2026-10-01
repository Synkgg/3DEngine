#pragma once
#include "../Graphics/Renderer.h"
#include <string>

class ProjectSettings
{
public:
    bool Load(const std::string& path = "ProjectSettings.cfg");
    bool Save(const std::string& path = "ProjectSettings.cfg") const;
    void EnsureLoaded();
    const RenderSettings& GetRenderSettings() const { return m_RenderSettings; }
    void SetRenderSettings(const RenderSettings& settings) { m_RenderSettings = settings; }
private:
    RenderSettings m_RenderSettings;
    bool m_Loaded = false;
};
