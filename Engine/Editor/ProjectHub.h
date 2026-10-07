#pragma once

#include <functional>
#include <string>
#include <vector>

class Project;

class ProjectHub
{
public:
    using OpenProjectCallback = std::function<bool(const std::string&)>;
    using CreateProjectCallback = std::function<bool(const std::string&, const std::string&)>;
    using ContinueCallback = std::function<void()>;

    ProjectHub();

    void Initialize();
    void Render(const OpenProjectCallback& openProject,
                const CreateProjectCallback& createProject,
                const ContinueCallback& continueLegacyWorkspace);

    void AddRecentProject(const Project& project);
    void SetError(std::string error);
    void ClearError();

private:
    struct RecentProject
    {
        std::string name;
        std::string descriptorPath;
    };

    void LoadRecentProjects();
    void SaveRecentProjects() const;
    void RemoveRecentProject(std::size_t index);
    std::string GetStatePath() const;

    char m_NewProjectName[128]{ "New Project" };
    char m_NewProjectLocation[512]{};
    std::string m_Error;
    std::vector<RecentProject> m_RecentProjects;
    bool m_CreateProjectOpen = false;
};
