-- Short-lived 3D scene meshes, not screen-space HUD flashes. All effects
-- live in world coordinates and are destroyed after their animation.
local WeaponVFX={}
WeaponVFX.__index=WeaponVFX

local FLASH="Assets/Prefabs/VFX/MuzzleBurst.prefab"
local TRACER="Assets/Prefabs/VFX/Tracer.prefab"
local IMPACT="Assets/Prefabs/VFX/Impact.prefab"
local MAX_EFFECTS=36

local function orient(dx,dy,dz)
    local horizontal=math.sqrt(dx*dx+dz*dz)
    return -math.deg(math.atan(dy,horizontal)),math.deg(math.atan(-dx,-dz))
end

function WeaponVFX.new(scene)
    return setmetatable({scene=scene,active={}},WeaponVFX)
end

function WeaponVFX:Spawn(path,x,y,z,life,size,kind,extra)
    if #self.active>=MAX_EFFECTS then
        local old=table.remove(self.active,1)
        self.scene.DestroyEntity(old.id)
    end
    local id=self.scene.InstantiatePrefab(path,0)
    if not id or id==0 then return end
    self.scene.SetPosition(id,x,y,z)
    self.scene.SetScale(id,size,size,size)
    self.active[#self.active+1]={id=id,life=life,total=life,size=size,kind=kind,extra=extra}
end

function WeaponVFX:Emit(ox,oy,oz,dx,dy,dz,range,hit,hitX,hitY,hitZ,muzzleSide,muzzleDistance)
    local rightX,rightZ=-dz,dx
    local rl=math.sqrt(rightX*rightX+rightZ*rightZ)
    if rl>0.001 then rightX,rightZ=rightX/rl,rightZ/rl end
    local mx=ox+dx*(muzzleDistance or .75)+rightX*(muzzleSide or .12)
    local my=oy+dy*(muzzleDistance or .75)-.07
    local mz=oz+dz*(muzzleDistance or .75)+rightZ*(muzzleSide or .12)

    self:Spawn(FLASH,mx,my,mz,.072,.15,"flash")
    local flash=self.active[#self.active]
    if flash and flash.kind=="flash" then
        local pitch,yaw=orient(dx,dy,dz)
        self.scene.SetRotation(flash.id,pitch,yaw,0)
    end

    local distance=math.min(range or 60,42)
    if hit then
        local hx,hy,hz=hitX-ox,hitY-oy,hitZ-oz
        distance=math.min(distance,math.sqrt(hx*hx+hy*hy+hz*hz))
    end
    -- A moving tracer slug rather than a static laser beam. Visible for only
    -- 55 ms and positioned in 3D, with no collider or input interception.
    if distance>1.3 then
        local length=math.min(1.6,distance*.12)
        self:Spawn(TRACER,mx+dx*.7,my+dy*.7,mz+dz*.7,.055,.025,"tracer",
            {x=mx,y=my,z=mz,dx=dx,dy=dy,dz=dz,distance=distance,length=length})
        local tracer=self.active[#self.active]
        if tracer and tracer.kind=="tracer" then
            local pitch,yaw=orient(dx,dy,dz)
            self.scene.SetRotation(tracer.id,pitch,yaw,0)
            self.scene.SetScale(tracer.id,.018,.018,length)
        end
    end

    if hit then
        self:Spawn(IMPACT,hitX,hitY,hitZ,.17,.13,"impact")
    end
end

function WeaponVFX:Update(dt)
    for i=#self.active,1,-1 do
        local e=self.active[i]
        e.life=e.life-dt
        if e.life<=0 then
            self.scene.DestroyEntity(e.id)
            table.remove(self.active,i)
        else
            local t=1-e.life/e.total
            if e.kind=="tracer" then
                local p=e.extra
                local advance=.8+(p.distance-1.5)*t
                self.scene.SetPosition(e.id,
                    p.x+p.dx*advance,p.y+p.dy*advance,p.z+p.dz*advance)
                self.scene.SetScale(e.id,.018*(1-t*.7),.018*(1-t*.7),p.length)
            elseif e.kind=="flash" then
                local size=e.size*(1-t)^1.5
                self.scene.SetScale(e.id,size,size,size)
            elseif e.kind=="impact" then
                local size=e.size*(1-t)*1.6
                self.scene.SetScale(e.id,size,size,size)
            end
        end
    end
end

function WeaponVFX:Clear()
    for _,e in ipairs(self.active) do self.scene.DestroyEntity(e.id) end
    self.active={}
end

return WeaponVFX
