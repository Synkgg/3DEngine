#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct HierarchyFolder
{
    std::string name;
    bool expanded = true;

    std::vector<std::uint32_t> entities;
    std::vector<HierarchyFolder> children;
};