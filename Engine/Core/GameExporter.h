#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

// The exporter packages the saved project as a standalone Windows/Vulkan game.
// The output folder must not exist and must be outside the source project.
struct GameExportSummary
{
    std::string projectName;
    std::filesystem::path startupScene;
    std::uintmax_t assetFileCount = 0;
    std::uintmax_t assetBytes = 0;
};

// Preflight without creating an export folder. Returns actionable errors for
// the editor's export dialog and the command-line exporter.
bool InspectGameExport(const std::filesystem::path& descriptor,
                       GameExportSummary& summary,
                       std::string& error);

bool ExportGame(const std::filesystem::path& descriptor,
                const std::filesystem::path& destination,
                const std::filesystem::path& executable,
                std::string& error);
