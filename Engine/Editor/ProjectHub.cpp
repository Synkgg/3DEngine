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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(kBg));
    ImGui::Begin("Velcryn Hub", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();
    const float rail = 264.0f;
    dl->AddRectFilled(wp, ImVec2(wp.x+rail, wp.y+ws.y), kRail);
    dl->AddLine(ImVec2(wp.x+rail,wp.y), ImVec2(wp.x+rail,wp.y+ws.y), kLine);
    dl->AddLine(ImVec2(wp.x+rail+30,wp.y+86), ImVec2(wp.x+ws.x-30,wp.y+86), kLine);

    DrawVelcrynMark(dl, ImVec2(wp.x+28,wp.y+22), 64);
    dl->AddText(ImVec2(wp.x+110,wp.y+31), kText, "V E L C R Y N");
    dl->AddText(ImVec2(wp.x+110,wp.y+55), kBlue, "E N G I N E");

    ImGui::SetCursorPos(ImVec2(18,126));
    NavItem("##projects","PROJECTS",true,ImVec2(228,44));
    ImGui::SetCursorPosX(18);
    if (NavItem("##open","OPEN PROJECT",false,ImVec2(228,44))) {
        std::string path; if (FileDialog::OpenProject(path)) openProject(path);
    }
    ImGui::SetCursorPosX(18);
    if (NavItem("##refresh","REFRESH",false,ImVec2(228,44))) LoadRecentProjects();

    ImGui::SetCursorPos(ImVec2(18,ws.y-74));
    if (NavItem("##legacy","LEGACY WORKSPACE",false,ImVec2(228,44))) continueLegacyWorkspace();

    dl->AddText(ImVec2(wp.x+rail+38,wp.y+27), kText, "PROJECTS");
    dl->AddText(ImVec2(wp.x+rail+38,wp.y+53), kMuted, "YOUR WORLDS. YOUR ENGINE.");
    if (ws.x > 1180.0f) dl->AddText(ImVec2(wp.x+ws.x-252,wp.y+40), kMuted, "CREATE   |   RENDER   |   BUILD");

    const float x0=rail+38, top=122, right=ws.x-38;
    const float createW=350, gap=28, recentW=std::max(390.0f,right-x0-createW-gap);
    dl->AddText(ImVec2(wp.x+x0,wp.y+top), kMuted, "RECENT PROJECTS");

    float y=top+30;
    std::size_t removeIndex=static_cast<std::size_t>(-1);
    if (m_RecentProjects.empty()) {
        dl->AddRect(ImVec2(wp.x+x0,wp.y+y),ImVec2(wp.x+x0+recentW,wp.y+y+120),kLine,6.0f,ImDrawFlags_None,1.0f);
        dl->AddText(ImVec2(wp.x+x0+22,wp.y+y+27),kText,"NO RECENT PROJECTS");
        dl->AddText(ImVec2(wp.x+x0+22,wp.y+y+54),kMuted,"Open an existing project or create a new workspace.");
    } else {
        for(std::size_t i=0;i<m_RecentProjects.size();++i) {
            const auto& recent=m_RecentProjects[i]; const bool exists=std::filesystem::is_regular_file(recent.descriptorPath);
            ImGui::SetCursorPos(ImVec2(x0,y)); ImGui::PushID((int)i);
            ImVec2 p=ImGui::GetCursorScreenPos(); ImGui::InvisibleButton("##card",ImVec2(recentW,92));
            const bool hover=ImGui::IsItemHovered();
            dl->AddRectFilled(p,ImVec2(p.x+recentW,p.y+92),hover?kPanelHover:kPanel,7);
            dl->AddRect(p,ImVec2(p.x+recentW,p.y+92),hover?kBlue:kLine,7.0f,ImDrawFlags_None,1.0f);
            dl->AddRectFilled(ImVec2(p.x,p.y+14),ImVec2(p.x+3,p.y+78),exists?kCyan:kMuted);
            dl->AddText(ImVec2(p.x+22,p.y+18),exists?kText:kMuted,recent.name.c_str());
            const std::string sub=exists?recent.descriptorPath:"Project file is missing";
            dl->AddText(ImVec2(p.x+22,p.y+50),kMuted,sub.c_str());
            if(hover&&exists&&ImGui::IsItemClicked()) openProject(recent.descriptorPath);
            ImGui::SetCursorPos(ImVec2(x0+recentW-82,y+31));
            if(NavItem("##remove","REMOVE",false,ImVec2(72,30))) removeIndex=i;
            ImGui::PopID(); y+=104;
        }
    }
    if(removeIndex!=static_cast<std::size_t>(-1)) RemoveRecentProject(removeIndex);

    const float cx=x0+recentW+gap;
    dl->AddText(ImVec2(wp.x+cx,wp.y+top),kMuted,"NEW PROJECT");
    const float createBottom=std::min(wp.y+top+390.0f,wp.y+ws.y-58.0f);\n    dl->AddRectFilled(ImVec2(wp.x+cx,wp.y+top+30),ImVec2(wp.x+cx+createW,createBottom),kPanel,7);
    dl->AddRect(ImVec2(wp.x+cx,wp.y+top+30),ImVec2(wp.x+cx+createW,createBottom),kLine,7.0f,ImDrawFlags_None,1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.035f,.055f,.078f,1));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(.05f,.08f,.11f,1));
    ImGui::PushStyleColor(ImGuiCol_Border,ImGui::ColorConvertU32ToFloat4(kLine));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,4);
    ImGui::SetCursorPos(ImVec2(cx+20,top+65)); ImGui::TextDisabled("PROJECT NAME");
    ImGui::SetCursorPosX(cx+20); ImGui::SetNextItemWidth(createW-40); ImGui::InputText("##ProjectName",m_NewProjectName,sizeof(m_NewProjectName));
    ImGui::SetCursorPosX(cx+20); ImGui::TextDisabled("LOCATION");
    ImGui::SetCursorPosX(cx+20); ImGui::SetNextItemWidth(createW-116); ImGui::InputText("##ProjectLocation",m_NewProjectLocation,sizeof(m_NewProjectLocation));
    ImGui::SameLine();
    if(NavItem("##browse","BROWSE",false,ImVec2(70,ImGui::GetFrameHeight()))) {
        std::string folder; if(FileDialog::SelectFolder(folder)) std::snprintf(m_NewProjectLocation,sizeof(m_NewProjectLocation),"%s",folder.c_str());
    }
    const std::filesystem::path preview=(std::filesystem::path(m_NewProjectLocation)/m_NewProjectName).lexically_normal();
    ImGui::SetCursorPosX(cx+20); ImGui::TextDisabled("PROJECT FOLDER");
    ImGui::SetCursorPosX(cx+20); ImGui::PushTextWrapPos(cx+createW-20); ImGui::TextWrapped("%s",preview.string().c_str()); ImGui::PopTextWrapPos();
    ImGui::SetCursorPos(ImVec2(cx+20,top+330));
    if(AccentButton("##create","CREATE PROJECT",ImVec2(createW-40,42))) createProject(m_NewProjectLocation,m_NewProjectName);
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(3);

    if(!m_Error.empty()) dl->AddText(ImVec2(wp.x+x0,wp.y+ws.y-34),IM_COL32(245,112,108,255),m_Error.c_str());
    else dl->AddText(ImVec2(wp.x+x0,wp.y+ws.y-34),kMuted,"VELCRYN ENGINE   //   WINDOWS + LINUX");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
