#pragma once

#include <string>
#include <vector>
#include <unordered_map>

enum class ScriptPropertyType
{
    Number,
    Boolean,
    String,
    Entity
};

struct ScriptPropertyValue
{
    ScriptPropertyType type = ScriptPropertyType::String;
    std::string value;
};

struct ScriptComponent
{
    std::vector<std::string> scriptNames;
    // Per-script, per-property overrides authored in the Inspector.
    std::unordered_map<std::string, std::unordered_map<std::string, ScriptPropertyValue>> properties;
};
