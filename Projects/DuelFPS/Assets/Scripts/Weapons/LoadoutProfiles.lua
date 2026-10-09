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
function Profiles.Active()
    return slotNumber(Preferences.LoadString("breakbulk_active_loadout","1"))
end
function Profiles.Get(slot)
    slot=slotNumber(slot)
    local default=defaults[slot]
    local primary=Preferences.LoadString("breakbulk_loadout_"..slot.."_primary",
        slot==1 and Preferences.LoadString("breakbulk_primary",default[1]) or default[1])
    local secondary=Preferences.LoadString("breakbulk_loadout_"..slot.."_secondary",
        slot==1 and Preferences.LoadString("breakbulk_secondary",default[2]) or default[2])
    if not Profiles.weapons[primary] then primary=default[1] end
    if not Profiles.weapons[secondary] or primary==secondary then
        secondary=primary=="pistol" and "rifle" or "pistol"
    end
    return primary,secondary
end
function Profiles.Save(slot,primary,secondary)
    slot=slotNumber(slot)
    if not Profiles.weapons[primary] or not Profiles.weapons[secondary] or primary==secondary then
        return false
    end
    Preferences.SaveString("breakbulk_loadout_"..slot.."_primary",primary)
    Preferences.SaveString("breakbulk_loadout_"..slot.."_secondary",secondary)
    if slot==Profiles.Active() then
        Preferences.SaveString("breakbulk_primary",primary)
        Preferences.SaveString("breakbulk_secondary",secondary)
    end
    return true
end
function Profiles.Select(slot)
    slot=slotNumber(slot)
    local primary,secondary=Profiles.Get(slot)
    Preferences.SaveString("breakbulk_active_loadout",tostring(slot))
    -- Retain compatibility with existing user profiles.
    Preferences.SaveString("breakbulk_primary",primary)
    Preferences.SaveString("breakbulk_secondary",secondary)
    return slot,primary,secondary
end
function Profiles.GetActive()
    local slot=Profiles.Active()
    local primary,secondary=Profiles.Get(slot)
    return slot,primary,secondary
end
return Profiles
