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
    constexpr ImU32 kBg = IM_COL32(8, 13, 20, 255);
    constexpr ImU32 kRail = IM_COL32(10, 17, 26, 255);
    constexpr ImU32 kPanel = IM_COL32(14, 23, 35, 255);
    constexpr ImU32 kPanelHover = IM_COL32(19, 32, 47, 255);
    constexpr ImU32 kLine = IM_COL32(37, 54, 72, 255);
    constexpr ImU32 kText = IM_COL32(232, 238, 245, 255);
    constexpr ImU32 kMuted = IM_COL32(116, 134, 153, 255);
    constexpr ImU32 kBlue = IM_COL32(61, 166, 255, 255);
    constexpr ImU32 kCyan = IM_COL32(0, 225, 255, 255);

    void DrawVelcrynMark(ImDrawList* dl, ImVec2 p, float s)
    {
        // Faceted, split-metal V matching the Velcryn identity board.
        const ImVec2 l0(p.x, p.y + s * .04f), l1(p.x + s * .27f, p.y + s * .12f);
        const ImVec2 lc(p.x + s * .50f, p.y + s * .82f), lb(p.x + s * .50f, p.y + s);
        const ImVec2 r0(p.x + s, p.y), r1(p.x + s * .73f, p.y + s * .12f);
        const ImVec2 notch(p.x + s * .50f, p.y + s * .43f);
        dl->AddTriangleFilled(l0, l1, lc, IM_COL32(202, 216, 230, 255));
        dl->AddTriangleFilled(l1, notch, lc, IM_COL32(94, 125, 154, 255));
        dl->AddTriangleFilled(r0, r1, notch, IM_COL32(235, 242, 248, 255));
        dl->AddTriangleFilled(r1, lc, notch, IM_COL32(91, 132, 170, 255));
        dl->AddTriangleFilled(notch, lc, lb, IM_COL32(21, 93, 148, 255));
        dl->AddLine(l0, lc, kBlue, 1.5f);
        dl->AddLine(r0, notch, kCyan, 1.2f);
        dl->AddLine(notch, lb, kBlue, 1.3f);
    }

    bool NavItem(const char* id, const char* label, bool active, ImVec2 size)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (hovered || active)
            dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), hovered ? kPanelHover : kPanel, 5.0f);
        if (active)
            dl->AddRectFilled(ImVec2(p.x, p.y + 8), ImVec2(p.x + 2, p.y + size.y - 8), kCyan, 1.0f);
        dl->AddText(ImVec2(p.x + 18, p.y + (size.y - ImGui::GetFontSize()) * .5f), active ? kText : kMuted, label);
        return ImGui::IsItemClicked();
    }

    bool AccentButton(const char* id, const char* label, ImVec2 size)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), hovered ? IM_COL32(20, 117, 178, 255) : IM_COL32(14, 82, 130, 255), 5.0f);
        dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), hovered ? kCyan : kBlue, 5.0f, ImDrawFlags_None, 1.0f);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(p.x + (size.x-ts.x)*.5f, p.y + (size.y-ts.y)*.5f), kText, label);
        return ImGui::IsItemClicked();
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
        out << std::quoted(recent.name) << ' ' << std::quoted(recent.descriptorPath) << '\n';
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(kBg));
    ImGui::Begin("Velcryn Hub", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    const float margin = 44.0f;

    // Product header: no permanent sidebar. The Hub opens as a dashboard.
    DrawVelcrynMark(dl, ImVec2(wp.x + margin, wp.y + 28.0f), 58.0f);
    dl->AddText(ImVec2(wp.x + margin + 78.0f, wp.y + 35.0f), kText, "V E L C R Y N");
    dl->AddText(ImVec2(wp.x + margin + 78.0f, wp.y + 58.0f), kBlue, "E N G I N E");
    dl->AddText(ImVec2(wp.x + ws.x - 255.0f, wp.y + 48.0f), kMuted, "H U B   //   PROJECTS");
    dl->AddLine(ImVec2(wp.x + margin, wp.y + 108.0f), ImVec2(wp.x + ws.x - margin, wp.y + 108.0f), kLine);

    // Hero copy.
    dl->AddText(ImVec2(wp.x + margin, wp.y + 142.0f), kMuted, "WELCOME BACK");
    ImGui::SetCursorPos(ImVec2(margin, 170.0f));
    ImGui::SetWindowFontScale(1.75f);
    ImGui::TextUnformatted("Build something remarkable.");
    ImGui::SetWindowFontScale(1.0f);
    dl->AddText(ImVec2(wp.x + margin, wp.y + 210.0f), kMuted,
        "Open a world, continue where you left off, or create a new Velcryn project.");

    // Quick actions are product tiles, not a navigation rail.
    const float actionY = 250.0f;
    const float tileGap = 16.0f;
    const float tileW = 190.0f;
    auto actionTile = [&](const char* id, const char* eyebrow, const char* title, float x, bool accent)
    {
        ImGui::SetCursorPos(ImVec2(x, actionY));
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(id, ImVec2(tileW, 78.0f));
        const bool hover = ImGui::IsItemHovered();
        dl->AddRectFilled(p, ImVec2(p.x + tileW, p.y + 78.0f), hover ? kPanelHover : kPanel, 7.0f);
        dl->AddRect(p, ImVec2(p.x + tileW, p.y + 78.0f), accent ? (hover ? kCyan : kBlue) : kLine,
            7.0f, ImDrawFlags_None, 1.0f);
        if (accent) dl->AddRectFilled(ImVec2(p.x, p.y + 14.0f), ImVec2(p.x + 3.0f, p.y + 64.0f), kCyan);
        dl->AddText(ImVec2(p.x + 18.0f, p.y + 15.0f), kMuted, eyebrow);
        dl->AddText(ImVec2(p.x + 18.0f, p.y + 42.0f), kText, title);
        return ImGui::IsItemClicked();
    };

    float ax = margin;
    if (actionTile("##new", "START", "NEW PROJECT", ax, true)) m_CreateProjectOpen = true;
    ax += tileW + tileGap;
    if (actionTile("##open", "FROM DISK", "OPEN PROJECT", ax, false)) {
        std::string path; if (FileDialog::OpenProject(path)) openProject(path);
    }
    ax += tileW + tileGap;
    if (actionTile("##refresh", "LIBRARY", "REFRESH", ax, false)) LoadRecentProjects();
    ax += tileW + tileGap;
    if (actionTile("##legacy", "TOOLS", "LEGACY WORKSPACE", ax, false)) continueLegacyWorkspace();

    // Recent workspace.
    const float sectionY = 374.0f;
    dl->AddText(ImVec2(wp.x + margin, wp.y + sectionY), kText, "RECENT PROJECTS");
    char countText[32]{};
    std::snprintf(countText, sizeof(countText), "%zu PROJECTS", m_RecentProjects.size());
    dl->AddText(ImVec2(wp.x + margin + 130.0f, wp.y + sectionY), kMuted, countText);
    dl->AddLine(ImVec2(wp.x + margin, wp.y + sectionY + 27.0f),
                ImVec2(wp.x + ws.x - margin, wp.y + sectionY + 27.0f), kLine);

    float py = sectionY + 47.0f;
    const float cardW = ws.x - margin * 2.0f;
    std::size_t removeIndex = static_cast<std::size_t>(-1);
    if (m_RecentProjects.empty()) {
        ImVec2 a(wp.x + margin, wp.y + py), b(wp.x + margin + cardW, wp.y + py + 105.0f);
        dl->AddRectFilled(a, b, kPanel, 7.0f);
        dl->AddRect(a, b, kLine, 7.0f, ImDrawFlags_None, 1.0f);
        dl->AddText(ImVec2(a.x + 24.0f, a.y + 25.0f), kText, "YOUR PROJECTS WILL APPEAR HERE");
        dl->AddText(ImVec2(a.x + 24.0f, a.y + 55.0f), kMuted,
            "Create a new project or open an existing .project file to get started.");
    } else {
        for (std::size_t i = 0; i < m_RecentProjects.size() && i < 3; ++i) {
            const auto& recent = m_RecentProjects[i];
            const bool exists = std::filesystem::is_regular_file(recent.descriptorPath);
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetCursorPos(ImVec2(margin, py));
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##project", ImVec2(cardW, 72.0f));
            const bool hover = ImGui::IsItemHovered();
            dl->AddRectFilled(p, ImVec2(p.x + cardW, p.y + 72.0f), hover ? kPanelHover : kPanel, 7.0f);
            dl->AddRect(p, ImVec2(p.x + cardW, p.y + 72.0f), hover ? kBlue : kLine, 7.0f, ImDrawFlags_None, 1.0f);
            dl->AddRectFilled(ImVec2(p.x, p.y + 12.0f), ImVec2(p.x + 3.0f, p.y + 60.0f), exists ? kCyan : kMuted);
            dl->AddText(ImVec2(p.x + 22.0f, p.y + 14.0f), exists ? kText : kMuted, recent.name.c_str());
            const std::string detail = exists ? recent.descriptorPath : "Project file is missing";
            dl->AddText(ImVec2(p.x + 22.0f, p.y + 42.0f), kMuted, detail.c_str());
            dl->AddText(ImVec2(p.x + cardW - 122.0f, p.y + 27.0f), hover ? kCyan : kMuted, exists ? "OPEN  >" : "MISSING");
            if (hover && exists && ImGui::IsItemClicked()) openProject(recent.descriptorPath);
            ImGui::SetCursorPos(ImVec2(margin + cardW - 58.0f, py + 22.0f));
            if (NavItem("##remove", "X", false, ImVec2(36.0f, 28.0f))) removeIndex = i;
            ImGui::PopID();
            py += 84.0f;
        }
    }
    if (removeIndex != static_cast<std::size_t>(-1)) RemoveRecentProject(removeIndex);

    // Footer is deliberately quiet.
    dl->AddLine(ImVec2(wp.x + margin, wp.y + ws.y - 48.0f), ImVec2(wp.x + ws.x - margin, wp.y + ws.y - 48.0f), kLine);
    dl->AddText(ImVec2(wp.x + margin, wp.y + ws.y - 31.0f), kMuted, "VELCRYN ENGINE");
    dl->AddText(ImVec2(wp.x + ws.x - 235.0f, wp.y + ws.y - 31.0f), kMuted, "WINDOWS + LINUX   //   v0.1");

    if (!m_Error.empty())
        dl->AddText(ImVec2(wp.x + margin + 150.0f, wp.y + ws.y - 31.0f), IM_COL32(245,112,108,255), m_Error.c_str());

    // New-project flow is modal so the dashboard remains clean.
    if (m_CreateProjectOpen) ImGui::OpenPopup("Create Velcryn Project");
    ImGui::SetNextWindowSize(ImVec2(520.0f, 330.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(wp.x + ws.x * .5f, wp.y + ws.y * .5f), ImGuiCond_Always, ImVec2(.5f,.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(kPanel));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.035f,.055f,.078f,1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(kLine));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::BeginPopupModal("Create Velcryn Project", nullptr, ImGuiWindowFlags_NoResize)) {
        m_CreateProjectOpen = false;
        ImGui::TextUnformatted("NEW VELCRYN PROJECT");
        ImGui::TextDisabled("Create an isolated workspace for scenes, scripts, assets and settings.");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextDisabled("PROJECT NAME");
        ImGui::SetNextItemWidth(-1); ImGui::InputText("##ProjectName", m_NewProjectName, sizeof(m_NewProjectName));
        ImGui::TextDisabled("LOCATION");
        ImGui::SetNextItemWidth(400.0f); ImGui::InputText("##ProjectLocation", m_NewProjectLocation, sizeof(m_NewProjectLocation));
        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(78.0f, 0))) {
            std::string folder; if (FileDialog::SelectFolder(folder))
                std::snprintf(m_NewProjectLocation, sizeof(m_NewProjectLocation), "%s", folder.c_str());
        }
        const std::filesystem::path preview=(std::filesystem::path(m_NewProjectLocation)/m_NewProjectName).lexically_normal();
        ImGui::TextDisabled("PROJECT FOLDER");
        ImGui::TextWrapped("%s", preview.string().c_str());
        ImGui::SetCursorPosY(ImGui::GetWindowHeight()-62.0f);
        if (ImGui::Button("Cancel", ImVec2(110.0f,38.0f))) ImGui::CloseCurrentPopup();
        ImGui::SameLine(ImGui::GetWindowWidth()-190.0f);
        if (AccentButton("##create", "CREATE", ImVec2(160.0f,38.0f))) {
            if (createProject(m_NewProjectLocation,m_NewProjectName)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
