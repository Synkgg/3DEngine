#pragma once
#include <filesystem>
#include <string>

// Creates a new, portable Windows folder. Never overwrites an existing export.
bool ExportGame(const std::filesystem::path& descriptor,
                const std::filesystem::path& destination,
                const std::filesystem::path& executable,
                std::string& error);
