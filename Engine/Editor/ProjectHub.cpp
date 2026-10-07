#include "ProjectHub.h"
#include "../Core/Project.h"

#include <SDL3/SDL.h>
#include <imgui.h>

#include "../Platform/Windows/FileDialog.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace
{
    // Compact Velcryn "V" mark inspired by the faceted identity concept.
    // Drawn natively so the Hub stays sharp at any DPI and does not depend on
    // an external branding texture.
    void DrawVelcrynMark(ImDrawList* drawList, const ImVec2 origin, const float size)
    {
        const ImU32 steel = IM_COL32(214, 225, 235, 255);
        const ImU32 ice = IM_COL32(32, 184, 255, 255);
        const ImU32 deep = IM_COL32(15, 84, 128, 255);

        const ImVec2 a(origin.x, origin.y);
        const ImVec2 b(origin.x + size * 0.28f, origin.y + size * 0.08f);
        const ImVec2 c(origin.x + size * 0.50f, origin.y + size * 0.76f);
        const ImVec2 d(origin.x + size * 0.72f, origin.y + size * 0.08f);
        const ImVec2 e(origin.x + size, origin.y);
        const ImVec2 f(origin.x + size * 0.50f, origin.y + size);

        drawList->AddTriangleFilled(a, b, c, steel);
        drawList->AddTriangleFilled(d, e, c, ice);
        drawList->AddTriangleFilled(c, d, f, deep);
        drawList->AddLine(a, c, IM_COL32(72, 202, 255, 210), 1.25f);
        drawList->AddLine(e, c, IM_COL32(110, 220, 255, 230), 1.25f);
    }
}

ProjectHub::ProjectHub() = default;

void ProjectHub::Initialize()
{
    LoadRecentProjects();
    const std::string defaultLocation = (std::filesystem::current_path() / "Projects").string();
    std::snprintf(m_NewProjectLocation, sizeof(m_NewProjectLocation), "%s", defaultLocation.c_str());
}

void ProjectHub::SetError(std::string error)
{
    m_Error = std::move(error);
}

void ProjectHub::ClearError()
{
    m_Error.clear();
}


std::string ProjectHub::GetStatePath() const
{
    char* prefPath = SDL_GetPrefPath("Velcryn", "Editor");
    if (prefPath == nullptr)
        return (std::filesystem::current_path() / "Saved" / "RecentProjects.txt").string();

    std::filesystem::path path(prefPath);
    SDL_free(prefPath);
    return (path / "RecentProjects.txt").string();
}

void ProjectHub::LoadRecentProjects()
{
    m_RecentProjects.clear();

    std::ifstream in(GetStatePath());
    if (!in)
        return;

    std::string name;
    std::string path;
    while (in >> std::quoted(name) >> std::quoted(path))
    {
        if (!path.empty())
            m_RecentProjects.push_back({ name, path });
    }
}

void ProjectHub::SaveRecentProjects() const
{
    const std::filesystem::path statePath(GetStatePath());
    std::error_code error;
    std::filesystem::create_directories(statePath.parent_path(), error);
    if (error)
        return;

    std::ofstream out(statePath, std::ios::trunc);
    if (!out)
        return;

    for (const RecentProject& recent : m_RecentProjects)
        out << std::quoted(recent.name) << ' ' << std::quoted(recent.descriptorPath) << '
';
}

void ProjectHub::AddRecentProject(const Project& project)
{
    if (project.descriptorPath.empty())
        return;

    const std::string path =
        std::filesystem::absolute(project.descriptorPath).lexically_normal().string();

    m_RecentProjects.erase(
        std::remove_if(
            m_RecentProjects.begin(),
            m_RecentProjects.end(),
            [&](const RecentProject& recent)
            {
                return std::filesystem::path(recent.descriptorPath).lexically_normal() ==
                       std::filesystem::path(path).lexically_normal();
            }),
        m_RecentProjects.end());

    m_RecentProjects.insert(m_RecentProjects.begin(), { project.name, path });
    if (m_RecentProjects.size() > 12)
        m_RecentProjects.resize(12);

    SaveRecentProjects();
}

void ProjectHub::RemoveRecentProject(std::size_t index)
{
    if (index >= m_RecentProjects.size())
        return;

    m_RecentProjects.erase(m_RecentProjects.begin() + static_cast<std::ptrdiff_t>(index));
    SaveRecentProjects();
}

void ProjectHub::Render(const OpenProjectCallback& openProject,
                        const CreateProjectCallback& createProject,
                        const ContinueCallback& continueLegacyWorkspace)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Hub-specific palette. Keep this local so opening a project restores the
    // editor's normal theme without any global style mutation.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.035f, 0.047f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.040f, 0.055f, 0.070f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.145f, 0.155f, 0.180f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.115f, 0.125f, 0.145f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.165f, 0.180f, 0.210f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.095f, 0.105f, 0.125f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.060f, 0.066f, 0.078f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.085f, 0.094f, 0.110f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.105f, 0.115f, 0.135f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.145f, 0.160f, 0.190f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.92f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.48f, 0.52f, 0.59f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));

    ImGui::Begin("Velcryn Hub", nullptr, flags);

    const float sidebarWidth = 238.0f;
    const float footerHeight = 42.0f;

    // Left rail: identity and primary actions.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.018f, 0.026f, 0.036f, 1.0f));
    ImGui::BeginChild("HubSidebar", ImVec2(sidebarWidth, 0), ImGuiChildFlags_None);
    ImGui::PopStyleColor();

    DrawVelcrynMark(ImGui::GetWindowDrawList(), ImVec2(ImGui::GetWindowPos().x + 22.0f, ImGui::GetWindowPos().y + 22.0f), 46.0f);
    ImGui::SetCursorPos(ImVec2(82, 24));
    ImGui::SetWindowFontScale(1.42f);
    ImGui::TextUnformatted("VELCRYN");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPosX(82);
    ImGui::TextDisabled("ENGINE  /  PROJECT HUB");

    ImGui::SetCursorPosY(100);
    ImGui::SetCursorPosX(16);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.055f, 0.42f, 0.68f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.075f, 0.55f, 0.86f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.035f, 0.32f, 0.54f, 1.0f));
    if (ImGui::Button("+  New Project", ImVec2(sidebarWidth - 32.0f, 42.0f)))
        ImGui::SetKeyboardFocusHere();
    ImGui::PopStyleColor(3);

    ImGui::SetCursorPosX(16);
    if (ImGui::Button("Open Project...", ImVec2(sidebarWidth - 32.0f, 42.0f)))
    {
        std::string path;
        if (FileDialog::OpenProject(path))
            openProject(path);
    }

    ImGui::SetCursorPosX(16);
    if (ImGui::Button("Refresh Projects", ImVec2(sidebarWidth - 32.0f, 38.0f)))
        LoadRecentProjects();

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 104.0f);
    ImGui::SetCursorPosX(24);
    ImGui::TextDisabled("VELCRYN ENGINE");
    ImGui::SetCursorPosX(16);
    if (ImGui::Button("Continue Legacy Workspace", ImVec2(sidebarWidth - 32.0f, 38.0f)))
    {
        continueLegacyWorkspace();
    }

    ImGui::EndChild();
    ImGui::SameLine(0, 0);

    // Main workspace.
    ImGui::BeginChild("HubMain", ImVec2(0, 0), ImGuiChildFlags_None);
    ImGui::SetCursorPos(ImVec2(34, 28));
    ImGui::SetWindowFontScale(1.62f);
    ImGui::TextUnformatted("Create. Render. Build.");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPosX(34);
    ImGui::TextDisabled("Welcome to Velcryn. Open a world or start something new.");

    const float contentTop = 94.0f;
    const float padding = 34.0f;
    const float mainWidth = ImGui::GetWindowWidth();
    const float createWidth = std::min(360.0f, std::max(300.0f, mainWidth * 0.32f));
    const float recentWidth = std::max(320.0f, mainWidth - createWidth - padding * 3.0f);
    const float panelHeight = std::max(320.0f, ImGui::GetWindowHeight() - contentTop - footerHeight - 22.0f);

    // Recent projects panel.
    ImGui::SetCursorPos(ImVec2(padding, contentTop));
    ImGui::BeginChild("RecentPanel", ImVec2(recentWidth, panelHeight), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(20, 18));
    ImGui::TextUnformatted("RECENT PROJECTS");
    ImGui::SameLine();
    ImGui::TextDisabled("  %d", static_cast<int>(m_RecentProjects.size()));
    ImGui::SetCursorPosX(20);
    ImGui::Separator();

    if (m_RecentProjects.empty())
    {
        const float centerY = std::max(90.0f, panelHeight * 0.36f);
        ImGui::SetCursorPosY(centerY);
        const char* emptyTitle = "No recent projects";
        const float titleWidth = ImGui::CalcTextSize(emptyTitle).x;
        ImGui::SetCursorPosX(std::max(20.0f, (recentWidth - titleWidth) * 0.5f));
        ImGui::TextUnformatted(emptyTitle);
        const char* emptyText = "Open an existing project or create a new one.";
        const float textWidth = ImGui::CalcTextSize(emptyText).x;
        ImGui::SetCursorPosX(std::max(20.0f, (recentWidth - textWidth) * 0.5f));
        ImGui::TextDisabled("%s", emptyText);
    }
    else
    {
        std::size_t removeIndex = static_cast<std::size_t>(-1);

        for (std::size_t i = 0; i < m_RecentProjects.size(); ++i)
        {
            const RecentProject& recent = m_RecentProjects[i];
            const bool exists = std::filesystem::is_regular_file(recent.descriptorPath);

            ImGui::PushID(static_cast<int>(i));
            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                exists ? ImVec4(0.090f, 0.098f, 0.114f, 1.0f)
                       : ImVec4(0.070f, 0.074f, 0.084f, 1.0f));
            ImGui::BeginChild("ProjectCard", ImVec2(-1, 76), ImGuiChildFlags_Borders);
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(16, 12));
            ImGui::SetWindowFontScale(1.08f);
            ImGui::TextUnformatted(recent.name.c_str());
            ImGui::SetWindowFontScale(1.0f);

            ImGui::SetCursorPos(ImVec2(16, 40));
            if (exists)
                ImGui::TextDisabled("%s", recent.descriptorPath.c_str());
            else
                ImGui::TextDisabled("Project file is missing");

            const float actionX = std::max(180.0f, ImGui::GetWindowWidth() - 174.0f);
            ImGui::SetCursorPos(ImVec2(actionX, 19));
            ImGui::BeginDisabled(!exists);
            if (ImGui::Button("Open", ImVec2(72, 34)))
                openProject(recent.descriptorPath);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Remove", ImVec2(76, 34)))
                removeIndex = i;

            ImGui::EndChild();
            ImGui::PopID();
        }

        if (removeIndex != static_cast<std::size_t>(-1))
            RemoveRecentProject(removeIndex);
    }
    ImGui::EndChild();

    // New project panel.
    ImGui::SetCursorPos(ImVec2(padding * 2.0f + recentWidth, contentTop));
    ImGui::BeginChild("CreatePanel", ImVec2(createWidth, panelHeight), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(22, 20));
    ImGui::SetWindowFontScale(1.18f);
    ImGui::TextUnformatted("Create Project");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPosX(22);
    ImGui::TextDisabled("Start with an isolated project workspace.");

    ImGui::SetCursorPos(ImVec2(22, 78));
    ImGui::TextDisabled("PROJECT NAME");
    ImGui::SetCursorPosX(22);
    ImGui::SetNextItemWidth(createWidth - 44.0f);
    ImGui::InputText("##ProjectName", m_NewProjectName, sizeof(m_NewProjectName));

    ImGui::SetCursorPosX(22);
    ImGui::TextDisabled("LOCATION");
    ImGui::SetCursorPosX(22);
    ImGui::SetNextItemWidth(createWidth - 116.0f);
    ImGui::InputText("##ProjectLocation", m_NewProjectLocation, sizeof(m_NewProjectLocation));
    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(66, 0)))
    {
        std::string folder;
        if (FileDialog::SelectFolder(folder))
            std::snprintf(m_NewProjectLocation, sizeof(m_NewProjectLocation), "%s", folder.c_str());
    }

    const std::filesystem::path preview =
        (std::filesystem::path(m_NewProjectLocation) / m_NewProjectName).lexically_normal();

    ImGui::SetCursorPosX(22);
    ImGui::TextDisabled("PROJECT FOLDER");
    ImGui::SetCursorPosX(22);
    ImGui::PushTextWrapPos(createWidth - 22.0f);
    ImGui::TextWrapped("%s", preview.string().c_str());
    ImGui::PopTextWrapPos();

    ImGui::SetCursorPosY(panelHeight - 72.0f);
    ImGui::SetCursorPosX(22);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.055f, 0.42f, 0.68f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.075f, 0.55f, 0.86f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.035f, 0.32f, 0.54f, 1.0f));
    if (ImGui::Button("Create Project", ImVec2(createWidth - 44.0f, 44.0f)))
        createProject(m_NewProjectLocation, m_NewProjectName);
    ImGui::PopStyleColor(3);

    ImGui::EndChild();

    // Bottom status strip.
    ImGui::SetCursorPos(ImVec2(padding, ImGui::GetWindowHeight() - 34.0f));
    if (!m_Error.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.48f, 0.46f, 1.0f));
        ImGui::TextUnformatted(m_Error.c_str());
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::TextDisabled("VELCRYN  //  Windows + Linux  //  Create | Render | Build");
    }

    ImGui::EndChild();
    ImGui::End();

    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(12);
}
