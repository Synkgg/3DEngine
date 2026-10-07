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
    const float topH = 72.0f, sideW = 224.0f, bottomH = 74.0f;

    // UE-style project browser shell: title bar, category rail, content browser, action footer.
    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + topH), IM_COL32(11,17,25,255));
    dl->AddLine(ImVec2(wp.x,wp.y+topH),ImVec2(wp.x+ws.x,wp.y+topH),kLine);
    DrawVelcrynMark(dl, ImVec2(wp.x+24,wp.y+14), 42.0f);
    dl->AddText(ImVec2(wp.x+82,wp.y+19),kText,"VELCRYN PROJECT BROWSER");
    dl->AddText(ImVec2(wp.x+82,wp.y+41),kMuted,"Select an existing project or create a new one");
    dl->AddText(ImVec2(wp.x+ws.x-178,wp.y+29),kMuted,"VELCRYN ENGINE  0.1");

    dl->AddRectFilled(ImVec2(wp.x,wp.y+topH),ImVec2(wp.x+sideW,wp.y+ws.y-bottomH),kRail);
    dl->AddLine(ImVec2(wp.x+sideW,wp.y+topH),ImVec2(wp.x+sideW,wp.y+ws.y-bottomH),kLine);
    dl->AddText(ImVec2(wp.x+20,wp.y+topH+22),kMuted,"PROJECTS");
    ImGui::SetCursorPos(ImVec2(12,topH+48));
    NavItem("##recent","RECENT PROJECTS",true,ImVec2(sideW-24,40));
    ImGui::SetCursorPosX(12);
    if(NavItem("##browse","BROWSE",false,ImVec2(sideW-24,40))) {
        std::string path; if(FileDialog::OpenProject(path)) openProject(path);
    }
    ImGui::SetCursorPosX(12);
    if(NavItem("##refresh","REFRESH",false,ImVec2(sideW-24,40))) LoadRecentProjects();
    dl->AddText(ImVec2(wp.x+20,wp.y+topH+190),kMuted,"TOOLS");
    ImGui::SetCursorPos(ImVec2(12,topH+216));
    if(NavItem("##legacy","LEGACY WORKSPACE",false,ImVec2(sideW-24,40))) continueLegacyWorkspace();

    const float contentX=sideW+30.0f, contentY=topH+24.0f, contentRight=ws.x-30.0f;
    dl->AddText(ImVec2(wp.x+contentX,wp.y+contentY),kText,"RECENT PROJECTS");
    char count[48]{};
    std::snprintf(count,sizeof(count),"%zu project%s",m_RecentProjects.size(),m_RecentProjects.size()==1?"":"s");
    dl->AddText(ImVec2(wp.x+contentX+128,wp.y+contentY),kMuted,count);
    dl->AddLine(ImVec2(wp.x+contentX,wp.y+contentY+28),ImVec2(wp.x+contentRight,wp.y+contentY+28),kLine);

    const float gridY=contentY+50.0f, gap=18.0f;
    const float available=contentRight-contentX;
    const int columns=available>900.0f?3:2;
    const float cardW=(available-gap*(columns-1))/columns;
    const float cardH=150.0f;
    std::size_t removeIndex=static_cast<std::size_t>(-1);

    if(m_RecentProjects.empty()) {
        ImVec2 a(wp.x+contentX,wp.y+gridY),b(wp.x+contentRight,wp.y+gridY+150);
        dl->AddRectFilled(a,b,kPanel,5.0f); dl->AddRect(a,b,kLine,5.0f,ImDrawFlags_None,1.0f);
        dl->AddText(ImVec2(a.x+24,a.y+30),kText,"NO RECENT PROJECTS");
        dl->AddText(ImVec2(a.x+24,a.y+58),kMuted,"Browse for an existing project or create a new project.");
    } else {
        for(std::size_t i=0;i<m_RecentProjects.size() && i<6;++i) {
            const int col=(int)i%columns,row=(int)i/columns;
            const float x=contentX+col*(cardW+gap), y=gridY+row*(cardH+gap);
            const auto& recent=m_RecentProjects[i];
            const bool exists=std::filesystem::is_regular_file(recent.descriptorPath);
            ImGui::PushID((int)i); ImGui::SetCursorPos(ImVec2(x,y));
            ImVec2 p=ImGui::GetCursorScreenPos(); ImGui::InvisibleButton("##project",ImVec2(cardW,cardH));
            const bool hover=ImGui::IsItemHovered();
            dl->AddRectFilled(p,ImVec2(p.x+cardW,p.y+cardH),hover?kPanelHover:kPanel,5.0f);
            dl->AddRect(p,ImVec2(p.x+cardW,p.y+cardH),hover?kBlue:kLine,5.0f,ImDrawFlags_None,1.0f);
            dl->AddRectFilled(p,ImVec2(p.x+cardW,p.y+70),IM_COL32(18,29,42,255),5.0f);
            // Simple scene/project thumbnail motif.
            dl->AddRectFilled(ImVec2(p.x+18,p.y+17),ImVec2(p.x+62,p.y+55),IM_COL32(20,55,78,255),4.0f);
            DrawVelcrynMark(dl,ImVec2(p.x+28,p.y+23),24.0f);
            dl->AddText(ImVec2(p.x+18,p.y+86),exists?kText:kMuted,recent.name.c_str());
            std::string path=recent.descriptorPath;
            if(path.size()>46) path="..."+path.substr(path.size()-43);
            dl->AddText(ImVec2(p.x+18,p.y+112),kMuted,path.c_str());
            dl->AddText(ImVec2(p.x+cardW-62,p.y+22),hover?kCyan:kMuted,exists?"OPEN":"!");
            if(hover&&exists&&ImGui::IsItemClicked()) openProject(recent.descriptorPath);
            ImGui::SetCursorPos(ImVec2(x+cardW-45,y+105));
            if(NavItem("##remove","X",false,ImVec2(28,26))) removeIndex=i;
            ImGui::PopID();
        }
    }
    if(removeIndex!=static_cast<std::size_t>(-1)) RemoveRecentProject(removeIndex);

    // Bottom action strip mirrors an editor/project-browser workflow.
    const float footerY=ws.y-bottomH;
    dl->AddRectFilled(ImVec2(wp.x,wp.y+footerY),ImVec2(wp.x+ws.x,wp.y+ws.y),IM_COL32(11,17,25,255));
    dl->AddLine(ImVec2(wp.x,wp.y+footerY),ImVec2(wp.x+ws.x,wp.y+footerY),kLine);
    dl->AddText(ImVec2(wp.x+24,wp.y+footerY+29),kMuted,"Select a project to open it");
    if(!m_Error.empty()) dl->AddText(ImVec2(wp.x+250,wp.y+footerY+29),IM_COL32(245,112,108,255),m_Error.c_str());

    ImGui::SetCursorPos(ImVec2(ws.x-382,footerY+17));
    if(NavItem("##footerBrowse","BROWSE...",false,ImVec2(120,40))) {
        std::string path; if(FileDialog::OpenProject(path)) openProject(path);
    }
    ImGui::SameLine(0,10);
    if(AccentButton("##footerNew","CREATE PROJECT",ImVec2(220,40))) m_CreateProjectOpen=true;

    if(m_CreateProjectOpen) ImGui::OpenPopup("New Project");
    ImGui::SetNextWindowSize(ImVec2(600,390),ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(wp.x+ws.x*.5f,wp.y+ws.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,ImGui::ColorConvertU32ToFloat4(kPanel));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.035f,.055f,.078f,1));
    ImGui::PushStyleColor(ImGuiCol_Border,ImGui::ColorConvertU32ToFloat4(kLine));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,3.0f);
    if(ImGui::BeginPopupModal("New Project",nullptr,ImGuiWindowFlags_NoResize)) {
        m_CreateProjectOpen=false;
        ImGui::TextUnformatted("CREATE NEW PROJECT");
        ImGui::TextDisabled("Configure a new Velcryn project workspace.");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::TextDisabled("PROJECT NAME");
        ImGui::SetNextItemWidth(-1); ImGui::InputText("##ProjectName",m_NewProjectName,sizeof(m_NewProjectName));
        ImGui::Spacing(); ImGui::TextDisabled("LOCATION");
        ImGui::SetNextItemWidth(465); ImGui::InputText("##ProjectLocation",m_NewProjectLocation,sizeof(m_NewProjectLocation));
        ImGui::SameLine();
        if(ImGui::Button("Browse...",ImVec2(90,0))) {
            std::string folder; if(FileDialog::SelectFolder(folder))
                std::snprintf(m_NewProjectLocation,sizeof(m_NewProjectLocation),"%s",folder.c_str());
        }
        const std::filesystem::path preview=(std::filesystem::path(m_NewProjectLocation)/m_NewProjectName).lexically_normal();
        ImGui::Spacing(); ImGui::TextDisabled("PROJECT WILL BE CREATED AT");
        ImGui::TextWrapped("%s",preview.string().c_str());
        ImGui::SetCursorPosY(ImGui::GetWindowHeight()-64);
        if(ImGui::Button("Cancel",ImVec2(110,38))) ImGui::CloseCurrentPopup();
        ImGui::SameLine(ImGui::GetWindowWidth()-205);
        if(AccentButton("##create","CREATE PROJECT",ImVec2(175,38))) {
            if(createProject(m_NewProjectLocation,m_NewProjectName)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2); ImGui::PopStyleColor(3);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
