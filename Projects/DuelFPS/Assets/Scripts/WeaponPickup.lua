Properties = {
    WeaponID = "pistol"
}

local collected = false

function OnCreate()
    self:SetInteractablePrompt("Pick up pistol")
end

function OnUpdate(dt)
    if collected then return end
    local r = self:GetRotation()
    self:SetRotation(r.x, r.y + 28.0 * dt, r.z)
end

function OnInteract()
    if collected then return end
    collected = true
    -- Numeric request keeps the pickup decoupled from the player script while
    -- using the engine's existing shared runtime State API.
    if Properties.WeaponID == "pistol" then
        State.SetNumber("duelfps_weapon_pickup", 1)
    end
    self:Destroy()
end
