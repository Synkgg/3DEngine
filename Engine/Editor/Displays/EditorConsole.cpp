#include "../Editor.h"

#include "../../Core/Logger.h"

#include <string>

namespace
{
    const char* GetLogPrefix(
        LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Info:
            return "[INFO]";

        case LogLevel::Warning:
            return "[WARNING]";

        case LogLevel::Error:
            return "[ERROR]";

        case LogLevel::Debug:
            return "[DEBUG]";
        }

        return "[INFO]";
    }

    ImVec4 GetLogColor(
        LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Info:
            return ImVec4(
                0.35f,
                1.0f,
                0.45f,
                1.0f
            );

        case LogLevel::Warning:
            return ImVec4(
                1.0f,
                0.85f,
                0.25f,
                1.0f
            );

        case LogLevel::Error:
            return ImVec4(
                1.0f,
                0.30f,
                0.30f,
                1.0f
            );

        case LogLevel::Debug:
            return ImVec4(
                0.35f,
                0.65f,
                1.0f,
                1.0f
            );
        }

        return ImVec4(
            1.0f,
            1.0f,
            1.0f,
            1.0f
        );
    }

    std::string MakeLogText(
        const LogMessage& message)
    {
        return std::string(
            GetLogPrefix(
                message.level
            )
        ) +
            " " +
            message.message;
    }
}

void Editor::RenderConsole()
{
    ImGui::Begin("Console");

    /*
     * Controls
     */
    if (ImGui::Button("Clear"))
    {
        Logger::Clear();
    }

    ImGui::SameLine();

    if (ImGui::Button("Copy All"))
    {
        std::string allLogs;

        for (const LogMessage& message :
            Logger::GetMessages())
        {
            allLogs +=
                MakeLogText(message);

            allLogs +=
                '\n';
        }

        ImGui::SetClipboardText(
            allLogs.c_str()
        );
    }

    ImGui::SameLine();

    if (ImGui::Button("Copy Errors"))
    {
        std::string errors;

        for (const LogMessage& message :
            Logger::GetMessages())
        {
            if (message.level !=
                LogLevel::Error)
            {
                continue;
            }

            errors +=
                MakeLogText(message);

            errors +=
                '\n';
        }

        ImGui::SetClipboardText(
            errors.c_str()
        );
    }

    ImGui::SameLine();

    ImGui::Checkbox(
        "Info",
        &m_ShowInfoLogs
    );

    ImGui::SameLine();

    ImGui::Checkbox(
        "Warning",
        &m_ShowWarningLogs
    );

    ImGui::SameLine();

    ImGui::Checkbox(
        "Error",
        &m_ShowErrorLogs
    );

    ImGui::SameLine();

    ImGui::Checkbox(
        "Debug",
        &m_ShowDebugLogs
    );

    ImGui::SameLine();

    ImGui::Checkbox(
        "Auto-scroll",
        &m_ConsoleAutoScroll
    );

    ImGui::Separator();

    /*
     * Console output
     */
    ImGui::BeginChild(
        "ConsoleOutput",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar
    );

    int logIndex = 0;

    for (const LogMessage& message :
        Logger::GetMessages())
    {
        bool visible = false;

        switch (message.level)
        {
        case LogLevel::Info:
            visible =
                m_ShowInfoLogs;
            break;

        case LogLevel::Warning:
            visible =
                m_ShowWarningLogs;
            break;

        case LogLevel::Error:
            visible =
                m_ShowErrorLogs;
            break;

        case LogLevel::Debug:
            visible =
                m_ShowDebugLogs;
            break;
        }

        if (!visible)
        {
            ++logIndex;
            continue;
        }

        const std::string logText =
            MakeLogText(message);

        const ImVec4 logColor =
            GetLogColor(message.level);

        ImGui::PushID(
            logIndex
        );

        ImGui::PushStyleColor(
            ImGuiCol_Text,
            logColor
        );

        /*
         * Entire log line is colored.
         */
        ImGui::Selectable(
            logText.c_str(),
            false
        );

        ImGui::PopStyleColor();

        /*
         * Right-click a log to copy it.
         */
        if (ImGui::BeginPopupContextItem(
            "LogContextMenu"))
        {
            if (ImGui::MenuItem("Copy"))
            {
                ImGui::SetClipboardText(
                    logText.c_str()
                );
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();

        ++logIndex;
    }

    if (m_ConsoleAutoScroll &&
        ImGui::GetScrollY() >=
        ImGui::GetScrollMaxY() - 10.0f)
    {
        ImGui::SetScrollHereY(
            1.0f
        );
    }

    ImGui::EndChild();
    ImGui::End();
}