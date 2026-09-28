#pragma once
#include "Project.h"
#include <string>

class ProjectManager
{
public:
    bool Load(const std::string& descriptorPath);
    bool Create(const std::string& directory, const std::string& name);
    void UseLegacyWorkspace();
    bool HasProject() const { return m_HasProject; }
    const Project& GetActiveProject() const { return m_Project; }
    std::string ResolveAssetPath(const std::string& path) const;
    std::string ResolveProjectPath(const std::string& path) const;
private:
    Project m_Project;
    bool m_HasProject = false;
};
