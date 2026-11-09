#include "ProjectHub.h"
#include "Brand/VelcrynBrand.h"
#include "../Core/Project.h"

#include <SDL3/SDL.h>
#include <imgui.h>

#include "../Platform/Windows/FileDialog.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace
{
    constexpr ImU32 kBg = IM_COL32(25, 26, 28, 255);
    constexpr ImU32 kRail = IM_COL32(31, 32, 35, 255);
    constexpr ImU32 kPanel = IM_COL32(38, 39, 43, 255);
    constexpr ImU32 kPanelHover = IM_COL32(45, 47, 52, 255);
    constexpr ImU32 kLine = IM_COL32(58, 60, 65, 255);
    constexpr ImU32 kText = IM_COL32(226, 228, 232, 255);
    constexpr ImU32 kMuted = IM_COL32(145, 148, 155, 255);
    constexpr ImU32 kBlue = IM_COL32(55, 156, 220, 255);
    constexpr ImU32 kCyan = IM_COL32(65, 190, 235, 255);


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
        dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), hovered ? IM_COL32(62, 132, 174, 255) : IM_COL32(48, 104, 140, 255), 5.0f);
        dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), hovered ? kCyan : kBlue, 5.0f, ImDrawFlags_None, 1.0f);
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(p.x + (size.x-ts.x)*.5f, p.y + (size.y-ts.y)*.5f), kText, label);
        return ImGui::IsItemClicked();
    }
}

ProjectHub::ProjectHub() = default;

void ProjectHub::Initialize()
{
    Velcryn::Editor::Brand::Initialize();
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
    const float margin = ws.x < 900 ? 24.0f : 48.0f;
    const float contentWidth = std::max(240.0f, ws.x - margin * 2);
    Velcryn::Editor::Brand::DrawLogoMark(dl, ImVec2(wp.x+margin,wp.y+28), 40.0f);
    dl->AddText(ImVec2(wp.x+margin+56,wp.y+32),kText,"VELCRYN");
    dl->AddText(ImVec2(wp.x+margin+56,wp.y+53),kMuted,"PROJECT LIBRARY");
    dl->AddLine(ImVec2(wp.x+margin,wp.y+90),ImVec2(wp.x+ws.x-margin,wp.y+90),kLine);

    ImGui::SetCursorPos(ImVec2(margin,120));
    ImGui::TextUnformatted("Your next world starts here.");
    ImGui::TextDisabled("Open a recent project or start something new.");
    ImGui::SetCursorPos(ImVec2(ws.x-margin-330,112));
    if(NavItem("##browse","OPEN PROJECT",false,ImVec2(150,42))) {
        std::string path; if(FileDialog::OpenProject(path)) openProject(path);
    }
    ImGui::SameLine(0,12);
    if(AccentButton("##new","NEW PROJECT",ImVec2(168,42))) m_CreateProjectOpen=true;

    ImGui::SetCursorPos(ImVec2(margin,196));
    ImGui::SetNextItemWidth(std::max(180.0f,contentWidth-134));
    ImGui::InputTextWithHint("##search","Search projects by name or location...",m_Search,sizeof(m_Search));
    ImGui::SameLine(0,14);
    if(ImGui::Button("Refresh",ImVec2(120,0))) LoadRecentProjects();
    ImGui::SetCursorPos(ImVec2(margin,240));
    ImGui::TextDisabled("RECENT PROJECTS  /  %zu",m_RecentProjects.size());

    std::size_t removeIndex=static_cast<std::size_t>(-1);
    std::string projectToOpen;
    ImGui::SetCursorPos(ImVec2(margin,276));
    ImGui::BeginChild("ProjectList",ImVec2(contentWidth,std::max(100.0f,ws.y-370)),ImGuiChildFlags_None);
    auto lower=[](std::string value) { for(char& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value; };
    const std::string query=lower(m_Search);
    unsigned matches=0;
    for(std::size_t i=0;i<m_RecentProjects.size();++i) {
        const auto& recent=m_RecentProjects[i];
        if(!query.empty() && lower(recent.name+" "+recent.descriptorPath).find(query)==std::string::npos)continue;
        ++matches;
        std::error_code ec;
        const bool exists=std::filesystem::is_regular_file(recent.descriptorPath,ec);
        ImGui::PushID(static_cast<int>(i));
        const ImVec2 pos=ImGui::GetCursorScreenPos();
        const float width=ImGui::GetContentRegionAvail().x;
        const bool clicked=ImGui::InvisibleButton("##project",ImVec2(width-52,82));
        const bool hover=ImGui::IsItemHovered();
        auto* rows=ImGui::GetWindowDrawList();
        rows->AddRectFilled(pos,ImVec2(pos.x+width-52,pos.y+82),hover?kPanelHover:kPanel,5);
        rows->AddRectFilled(ImVec2(pos.x+16,pos.y+18),ImVec2(pos.x+60,pos.y+62),kRail,4);
        Velcryn::Editor::Brand::DrawLogoMark(rows,ImVec2(pos.x+25,pos.y+27),26);
        rows->PushClipRect(ImVec2(pos.x+78,pos.y),ImVec2(pos.x+width-152,pos.y+82),true);
        rows->AddText(ImVec2(pos.x+78,pos.y+17),exists?kText:kMuted,recent.name.c_str());
        rows->AddText(ImVec2(pos.x+78,pos.y+46),kMuted,recent.descriptorPath.c_str());
        rows->PopClipRect();
        rows->AddText(ImVec2(pos.x+width-133,pos.y+32),exists?kCyan:kMuted,exists?"OPEN  >":"MISSING");
        if(hover)ImGui::SetTooltip("%s",recent.descriptorPath.c_str());
        if(clicked) {
            if(exists)projectToOpen=recent.descriptorPath;
            else m_Error="Project not found. Browse to its new location or remove it from this list.";
        }
        ImGui::SameLine(0,8);
        if(ImGui::Button("X",ImVec2(36,82)))removeIndex=i;
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Remove from recents (keeps project files)");
        ImGui::Dummy(ImVec2(1,6));
        ImGui::PopID();
    }
    if(!matches) {
        ImGui::Spacing();
        ImGui::TextUnformatted(query.empty()?"No projects yet.":"No matching projects.");
        ImGui::TextDisabled(query.empty()?"Use Open Project to locate a .project file, or create a new project.":"Try a different name or location.");
    }
    ImGui::EndChild();
    if(removeIndex!=static_cast<std::size_t>(-1))RemoveRecentProject(removeIndex);
    if(!projectToOpen.empty())openProject(projectToOpen);
    const float footerY=ws.y-68;
    dl->AddLine(ImVec2(wp.x+margin,wp.y+footerY),ImVec2(wp.x+ws.x-margin,wp.y+footerY),kLine);
    ImGui::SetCursorPos(ImVec2(margin,footerY+18));
    if(!m_Error.empty())ImGui::TextWrapped("%s",m_Error.c_str());
    else ImGui::TextDisabled("VELCRYN 0.1  /  WINDOWS  /  VULKAN");
    ImGui::SetCursorPos(ImVec2(ws.x-margin-190,footerY+10));
    if(NavItem("##legacy","LEGACY WORKSPACE",false,ImVec2(190,40)))continueLegacyWorkspace();

    if(m_CreateProjectOpen) ImGui::OpenPopup("New Project");
    ImGui::SetNextWindowSize(ImVec2(600,390),ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(wp.x+ws.x*.5f,wp.y+ws.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,ImGui::ColorConvertU32ToFloat4(kPanel));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.115f,.118f,.125f,1));
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
