-- Persistent, per-player custom loadouts shared by the lobby and in-game armory.
-- Store only whitelisted weapon IDs: no code or asset paths are deserialized.
local Profiles = {}
Profiles.count = 5
Profiles.weapons = {rifle=true,smg=true,shotgun=true,pistol=true}
Profiles.order = {"rifle","smg","shotgun","pistol"}
Profiles.categories = {
    rifle="ASSAULT RIFLES",smg="SUBMACHINE GUNS",
    shotgun="SHOTGUNS",pistol="HANDGUNS"
}
local defaults = {
    {"rifle","pistol"},
    {"smg","pistol"},
    {"shotgun","pistol"},
    {"rifle","smg"},
    {"shotgun","smg"}
}
local function slotNumber(slot)
    return math.max(1,math.min(Profiles.count,math.floor(tonumber(slot) or 1)))
end
function Profiles.Active(preferences)
    return slotNumber(preferences.LoadString("breakbulk_active_loadout","1"))
end
function Profiles.Get(slot,preferences)
    slot=slotNumber(slot)
    local default=defaults[slot]
    local primary=preferences.LoadString("breakbulk_loadout_"..slot.."_primary",
        slot==1 and preferences.LoadString("breakbulk_primary",default[1]) or default[1])
    local secondary=preferences.LoadString("breakbulk_loadout_"..slot.."_secondary",
        slot==1 and preferences.LoadString("breakbulk_secondary",default[2]) or default[2])
    if not Profiles.weapons[primary] then primary=default[1] end
    if not Profiles.weapons[secondary] or primary==secondary then
        secondary=primary=="pistol" and "rifle" or "pistol"
    end
    return primary,secondary
end
function Profiles.Save(slot,primary,secondary,preferences)
    slot=slotNumber(slot)
    if not Profiles.weapons[primary] or not Profiles.weapons[secondary] or primary==secondary then
        return false
    end
    preferences.SaveString("breakbulk_loadout_"..slot.."_primary",primary)
    preferences.SaveString("breakbulk_loadout_"..slot.."_secondary",secondary)
    if slot==Profiles.Active(preferences) then
        preferences.SaveString("breakbulk_primary",primary)
        preferences.SaveString("breakbulk_secondary",secondary)
    end
    return true
end
function Profiles.Select(slot,preferences)
    slot=slotNumber(slot)
    local primary,secondary=Profiles.Get(slot,preferences)
    preferences.SaveString("breakbulk_active_loadout",tostring(slot))
    -- Retain compatibility with existing user profiles.
    preferences.SaveString("breakbulk_primary",primary)
    preferences.SaveString("breakbulk_secondary",secondary)
    return slot,primary,secondary
end
function Profiles.GetActive(preferences)
    local slot=Profiles.Active(preferences)
    local primary,secondary=Profiles.Get(slot,preferences)
    return slot,primary,secondary
end
return Profiles
