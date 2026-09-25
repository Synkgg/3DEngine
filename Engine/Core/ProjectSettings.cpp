#include "ProjectSettings.h"
#include <fstream>
#include <sstream>

bool ProjectSettings::Load(const std::string& path)
{
    m_Loaded = true;
    std::ifstream file(path);
    if (!file) return false;
    std::string key;
    while (file >> key)
    {
        if(key=="AntiAliasing") file>>m_RenderSettings.antiAliasing;
        else if(key=="AntiAliasingSamples") file>>m_RenderSettings.antiAliasingSamples;
        else if(key=="Shadows") file>>m_RenderSettings.shadows;
        else if(key=="Fog") file>>m_RenderSettings.fog;
        else if(key=="Bloom") file>>m_RenderSettings.bloom;
        else if(key=="ViewDistance") file>>m_RenderSettings.viewDistance;
        else if(key=="Exposure") file>>m_RenderSettings.exposure;
        else if(key=="FogDensity") file>>m_RenderSettings.fogDensity;
        else if(key=="BloomStrength") file>>m_RenderSettings.bloomStrength;
        else if(key=="ShadowQuality") file>>m_RenderSettings.shadowQuality;
        else if(key=="ShadowDistance") file>>m_RenderSettings.shadowDistance;
        else { std::string ignored; std::getline(file,ignored); }
    }
    return true;
}

bool ProjectSettings::Save(const std::string& path) const
{
    std::ofstream file(path,std::ios::trunc); if(!file) return false;
    file << "Version 1\n"
         << "AntiAliasing " << m_RenderSettings.antiAliasing << '\n'
         << "AntiAliasingSamples " << m_RenderSettings.antiAliasingSamples << '\n'
         << "Shadows " << m_RenderSettings.shadows << '\n'
         << "Fog " << m_RenderSettings.fog << '\n'
         << "Bloom " << m_RenderSettings.bloom << '\n'
         << "ViewDistance " << m_RenderSettings.viewDistance << '\n'
         << "Exposure " << m_RenderSettings.exposure << '\n'
         << "FogDensity " << m_RenderSettings.fogDensity << '\n'
         << "BloomStrength " << m_RenderSettings.bloomStrength << '\n'
         << "ShadowQuality " << m_RenderSettings.shadowQuality << '\n'
         << "ShadowDistance " << m_RenderSettings.shadowDistance << '\n';
    return file.good();
}

void ProjectSettings::EnsureLoaded()
{
    if(m_Loaded) return;
    if(!Load()) { m_Loaded=true; Save(); }
}
