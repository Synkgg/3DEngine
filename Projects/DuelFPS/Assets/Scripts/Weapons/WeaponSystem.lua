local ShotPattern = require("Scripts.Weapons.ShotPattern")
local WeaponVFX = require("Scripts.Weapons.WeaponVFX")
local WeaponSystem = {}
WeaponSystem.__index = WeaponSystem

function WeaponSystem.new(api)
    return setmetatable({
        api = api,
        definitions = {},
        inventory = {},
        equipped = nil,
        viewmodel = 0,
        muzzleSocket = 0,
        viewmodelBaseOffset = {x=0,y=0,z=0},
        cooldown = 0.0,
        reloadTimer = 0.0,
        kick = 0.0,
        recoilTarget = 0.0,
        recoilApplied = 0.0,
        aiming = false,
        animationTime=0,
        triggerHeld=false,
        fx=WeaponVFX.new(api.Scene)
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

-- Equip exactly two chosen guns. Each round restores ammunition, not
-- the original default loadout, so players keep their armory selection.
function WeaponSystem:SetLoadout(primary,secondary,practice)
    if not self.definitions[primary] or not self.definitions[secondary] or primary==secondary then
        return false
    end
    if self.viewmodel~=0 then self.api.Scene.DestroyEntity(self.viewmodel) end
    self.viewmodel=0
    self.muzzleSocket=0
    self.equipped=nil
    self.inventory={}
    for _,id in ipairs({primary,secondary}) do
        local def=self.definitions[id]
        self.inventory[id]={ammo=def.magSize,reserve=practice and def.practiceReserve or def.startingReserve}
    end
    self.primary=primary
    self.secondary=secondary
    self.triggerHeld=false
    return self:Equip(primary)
end

function WeaponSystem:Equip(weaponID)
    local def = self.definitions[weaponID]
    if not def or not self.inventory[weaponID] then return false end

    -- Pressing the slot for the weapon already in our hands is a no-op.
    -- Do not respawn its viewmodel or reset reload/recoil state.
    if self.equipped == weaponID and self.viewmodel ~= 0 then
        return false
    end

    if self.viewmodel ~= 0 then
        self.api.Scene.DestroyEntity(self.viewmodel)
        self.viewmodel = 0
    end
    self.muzzleSocket = 0
    self.equipped = weaponID
    self.reloadTimer = 0.0
    self.kick = 0.0
    self.recoilTarget = 0.0
    self.recoilApplied = 0.0
    self.viewmodel = self.api.Scene.InstantiatePrefab(def.viewmodelPrefab, 0)
    if self.viewmodel and self.viewmodel ~= 0 then
        -- The prefab root is a camera-relative pose offset, not a world
        -- position. Preserve authored offsets instead of overwriting them.
        self.viewmodelBaseOffset = self.api.Scene.GetPosition(self.viewmodel)
        self.muzzleSocket = self.api.Scene.FindChild(self.viewmodel, "MuzzleSocket")
    end
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
    for id,item in pairs(self.inventory) do
        local def=self.definitions[id]
        item.ammo=def.magSize;item.reserve=practice and def.practiceReserve or def.startingReserve
    end
    self.reloadTimer=0;self.cooldown=0;self.kick=0;self.recoilTarget=0;self.recoilApplied=0
end

function WeaponSystem:Reload()
    local def, item = self:GetDefinition(), self:GetItem()
    if not def or not item or self.reloadTimer > 0.0 then return false end
    if item.ammo >= def.magSize or item.reserve <= 0 then return false end
    self.reloadTimer = def.reloadTime
    return true
end

function WeaponSystem:Update(dt, cameraEntity)
    self.fx:Update(dt)
    if not self.api.Input.IsMouseButtonDown(1) then self.triggerHeld=false end
    self.animationTime=self.animationTime+dt
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

    -- Camera-relative position keeps the gun in the same part of the screen
    -- while looking up/down. Right x Forward gives camera Up; the previous
    -- world-Y offset drifted as the camera pitched.
    local upX = r.y*f.z-r.z*f.y
    local upY = r.z*f.x-r.x*f.z
    local upZ = r.x*f.y-r.y*f.x
    local upLength = math.max(0.0001, math.sqrt(upX*upX+upY*upY+upZ*upZ))
    upX,upY,upZ = upX/upLength,upY/upLength,upZ/upLength

    -- The prefab root contains only the carried pose. The mesh child already
    -- has its GunPack +90 degree Y correction. Pitching the parent with the
    -- additive Euler hierarchy caused the barrel to bank onto its side.
    -- Keep a level, yaw-only root and use the camera basis for screen anchoring.
    local base = self.viewmodelBaseOffset
    local rightAmount = side + base.x
    local forwardAmount = forwardOffset - self.kick + base.z
    local upAmount = down + base.y + self.kick * 1.6
    local moving = self.api.Input.IsKeyDown("W") or self.api.Input.IsKeyDown("S")
        or self.api.Input.IsKeyDown("A") or self.api.Input.IsKeyDown("D")
    if moving and not self.aiming then upAmount=upAmount+math.sin(self.animationTime*13)*.008 end
    local reloadPose = self.reloadTimer>0 and math.sin(math.pi*self.reloadTimer/def.reloadTime) or 0
    upAmount=upAmount-reloadPose*.12

    self.api.Scene.SetPosition(self.viewmodel,
        c.x+r.x*rightAmount+f.x*forwardAmount+upX*upAmount,
        c.y+r.y*rightAmount+f.y*forwardAmount+upY*upAmount,
        c.z+r.z*rightAmount+f.z*forwardAmount+upZ*upAmount)

    -- Scene.SetRotation accepts DEGREES. The child mesh's Y=90deg is already
    -- authored in the prefab; do not add camera pitch or any roll here.
    local yaw = math.deg(math.atan(-f.x, -f.z))
    self.api.Scene.SetRotation(self.viewmodel, 0, yaw, 0)
end

function WeaponSystem:SpawnRemoteShot(ownerEntity,weaponID,ox,oy,oz,dx,dy,dz)
    local def=self.definitions[weaponID]
    if not def then return end
    local hit=self.api.Physics.Raycast(ox,oy,oz,dx,dy,dz,def.range,ownerEntity or 0)
    self.fx:Emit(ox,oy,oz,dx,dy,dz,def.range,hit.hit,hit.x,hit.y,hit.z,.05,.48)
    self.api.Audio.PlaySFX(def.fireSound,.43)
end

function WeaponSystem:Fire(ownerEntity, cameraEntity)
    local def,item=self:GetDefinition(),self:GetItem()
    if not def or not item or self.cooldown>0 or self.reloadTimer>0 or item.ammo<=0 then return nil end
    if not def.automatic and self.triggerHeld then return nil end
    self.triggerHeld=true
    item.ammo=item.ammo-1
    self.cooldown=def.fireInterval
    self.kick=math.min(def.viewKick*1.35,self.kick+def.viewKick)
    self.recoilTarget=math.min(def.cameraKick*1.5,self.recoilTarget+def.cameraKick)
    self.api.Audio.PlaySFX(def.fireSound,def.id=="shotgun" and 1.0 or .9)
    if def.id=="shotgun" then self.api.Audio.PlaySFX("Assets/Audio/Breakbulk/sidearm.wav",.38) end

    local c,f=self.api.Camera.GetPosition(),self.api.Camera.GetForward()
    local dirs=ShotPattern.Directions(f.x,f.y,f.z,def.pellets or 1,def.spread or 0)
    local hits={}
    local first=nil
    for _,dir in ipairs(dirs) do
        local hit=self.api.Physics.Raycast(c.x,c.y,c.z,dir.x,dir.y,dir.z,def.range,ownerEntity)
        if hit.hit then
            hits[#hits+1]={entityID=hit.entityID,x=hit.x,y=hit.y,z=hit.z}
            if not first then first=hit end
        end
    end
    local muzzle=nil
    if self.muzzleSocket and self.muzzleSocket~=0 then
        muzzle=self.api.Scene.GetWorldPosition(self.muzzleSocket)
    end
    self.fx:Emit(c.x,c.y,c.z,f.x,f.y,f.z,def.range,
        first~=nil,first and first.x or 0,first and first.y or 0,first and first.z or 0,
        self.aiming and def.adsSide or def.hipSide,
        (self.aiming and def.adsForward or def.hipForward)+(def.muzzleLocalDistance or .6),
        (self.aiming and def.adsDown or def.hipDown)+(def.muzzleLocalY or 0),
        muzzle and muzzle.x,muzzle and muzzle.y,muzzle and muzzle.z)

    return {
        hit=first~=nil,
        entityID=first and first.entityID or 0,
        x=first and first.x or 0,y=first and first.y or 0,z=first and first.z or 0,
        hits=hits,
        originX=c.x,originY=c.y,originZ=c.z,
        forwardX=f.x,forwardY=f.y,forwardZ=f.z,
        damage=def.damage,range=def.range
    }
end

return WeaponSystem
