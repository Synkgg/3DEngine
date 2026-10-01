local WeaponSystem = {}
WeaponSystem.__index = WeaponSystem

function WeaponSystem.new(api)
    return setmetatable({
        api = api,
        definitions = {},
        inventory = {},
        equipped = nil,
        viewmodel = 0,
        cooldown = 0.0,
        reloadTimer = 0.0,
        kick = 0.0,
        recoilTarget = 0.0,
        recoilApplied = 0.0,
        aiming = false
    }, WeaponSystem)
end

function WeaponSystem:Register(definition)
    self.definitions[definition.id] = definition
end

function WeaponSystem:Has(weaponID)
    return self.inventory[weaponID] ~= nil
end

function WeaponSystem:Give(weaponID, reserveOverride)
    local def = self.definitions[weaponID]
    if not def then return false end
    local item = self.inventory[weaponID]
    if not item then
        item = {
            ammo = def.magSize,
            reserve = reserveOverride or def.startingReserve
        }
        self.inventory[weaponID] = item
    elseif reserveOverride then
        item.reserve = reserveOverride
    end
    return self:Equip(weaponID)
end

function WeaponSystem:Equip(weaponID)
    local def = self.definitions[weaponID]
    if not def or not self.inventory[weaponID] then return false end
    if self.viewmodel ~= 0 then
        self.api.Scene.DestroyEntity(self.viewmodel)
        self.viewmodel = 0
    end
    self.equipped = weaponID
    self.reloadTimer = 0.0
    self.kick = 0.0
    self.recoilTarget = 0.0
    self.recoilApplied = 0.0
    self.viewmodel = self.api.Scene.InstantiatePrefab(def.viewmodelPrefab, 0)
    return true
end

function WeaponSystem:GetDefinition()
    return self.equipped and self.definitions[self.equipped] or nil
end

function WeaponSystem:GetItem()
    return self.equipped and self.inventory[self.equipped] or nil
end

function WeaponSystem:GetAmmo()
    local item = self:GetItem()
    return item and item.ammo or 0, item and item.reserve or 0
end

function WeaponSystem:IsReloading()
    return self.reloadTimer > 0.0
end

function WeaponSystem:ResetAmmo(practice)
    local def, item = self:GetDefinition(), self:GetItem()
    if not def or not item then return end
    item.ammo = def.magSize
    item.reserve = practice and def.practiceReserve or def.startingReserve
    self.reloadTimer = 0.0
end

function WeaponSystem:Reload()
    local def, item = self:GetDefinition(), self:GetItem()
    if not def or not item or self.reloadTimer > 0.0 then return false end
    if item.ammo >= def.magSize or item.reserve <= 0 then return false end
    self.reloadTimer = def.reloadTime
    return true
end

function WeaponSystem:Update(dt, cameraEntity)
    self.cooldown = math.max(0.0, self.cooldown - dt)

    local def, item = self:GetDefinition(), self:GetItem()
    if not def or not item then return end

    self.kick = math.max(0.0, self.kick - dt * (def.kickRecovery or 6.0))

    -- Smooth camera recoil: ease upward toward the accumulated shot impulse,
    -- then ease the exact applied offset back to zero.
    local target = self.recoilTarget
    local speed = target > self.recoilApplied and (def.recoilRiseSpeed or 12.0) or (def.recoilReturnSpeed or 7.0)
    local alpha = math.min(1.0, dt * speed)
    local nextApplied = self.recoilApplied + (target - self.recoilApplied) * alpha
    local delta = nextApplied - self.recoilApplied
    if cameraEntity ~= 0 and math.abs(delta) > 0.000001 then
        self.api.Camera.RotateEntity(cameraEntity, 0.0, delta)
    end
    self.recoilApplied = nextApplied
    self.recoilTarget = math.max(0.0, self.recoilTarget - dt * (def.recoilReturnSpeed or 7.0) * def.cameraKick)

    if self.reloadTimer > 0.0 then
        self.reloadTimer = math.max(0.0, self.reloadTimer - dt)
        if self.reloadTimer == 0.0 then
            local needed = def.magSize - item.ammo
            local loaded = math.min(needed, item.reserve)
            item.ammo = item.ammo + loaded
            item.reserve = item.reserve - loaded
        end
    end

    self.aiming = self.api.Input.IsMouseButtonDown(3) and self.reloadTimer <= 0.0
    if cameraEntity ~= 0 then
        self.api.Camera.SetEntityFOV(cameraEntity, self.aiming and def.adsFov or def.hipFov)
    end

    if self.viewmodel == 0 then return end
    local c, f, r = self.api.Camera.GetPosition(), self.api.Camera.GetForward(), self.api.Camera.GetRight()
    local side = self.aiming and def.adsSide or def.hipSide
    local forwardOffset = self.aiming and def.adsForward or def.hipForward
    local down = self.aiming and def.adsDown or def.hipDown

    -- Recoil moves the carried weapon visibly UP and BACK, then settles.
    local x = c.x + r.x * side + f.x * (forwardOffset - self.kick)
    local y = c.y + r.y * side + f.y * (forwardOffset - self.kick) + down + self.kick * 1.6
    local z = c.z + r.z * side + f.z * (forwardOffset - self.kick)
    self.api.Scene.SetPosition(self.viewmodel, x, y, z)

    local yaw = math.deg(math.atan(-f.x, -f.z))
    local horizontal = math.sqrt(f.x * f.x + f.z * f.z)
    local pitch = math.deg(math.atan(f.y, horizontal))
    self.api.Scene.SetRotation(self.viewmodel, -pitch, yaw, 0.0)
end

function WeaponSystem:Fire(ownerEntity, cameraEntity)
    local def, item = self:GetDefinition(), self:GetItem()
    if not def or not item or self.cooldown > 0.0 or self.reloadTimer > 0.0 or item.ammo <= 0 then
        return nil
    end

    item.ammo = item.ammo - 1
    self.cooldown = def.fireInterval
    self.kick = math.min(def.viewKick * 1.35, self.kick + def.viewKick)
    self.recoilTarget = math.min(def.cameraKick * 1.5, self.recoilTarget + def.cameraKick)
    self.api.Audio.PlaySFX(def.fireSound, 0.9)

    local c, f = self.api.Camera.GetPosition(), self.api.Camera.GetForward()
    local hit = self.api.Physics.Raycast(c.x, c.y, c.z, f.x, f.y, f.z, def.range, ownerEntity)
    return {
        hit = hit.hit,
        entityID = hit.entityID,
        x = hit.x, y = hit.y, z = hit.z,
        originX = c.x, originY = c.y, originZ = c.z,
        forwardX = f.x, forwardY = f.y, forwardZ = f.z,
        damage = def.damage,
        range = def.range
    }
end

return WeaponSystem
