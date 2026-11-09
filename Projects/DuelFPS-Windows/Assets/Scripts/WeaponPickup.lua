Properties = {
    WeaponID = "pistol"
}

local collected = false

local function displayName()
    if Properties.WeaponID == "rifle" then return "rifle" end
    return "pistol"
end

function OnCreate()
    self:SetInteractablePrompt("Pick up "..displayName())
end

function OnUpdate(dt)
    if collected then return end
    local r = self:GetRotation()
    self:SetRotation(r.x, r.y + 28.0 * dt, r.z)
end

function OnInteract()
    if collected then return end
    collected = true
    if Properties.WeaponID == "pistol" then
        State.SetNumber("duelfps_weapon_pickup", 1)
    elseif Properties.WeaponID == "rifle" then
        State.SetNumber("duelfps_weapon_pickup", 2)
    end
    self:Destroy()
end
