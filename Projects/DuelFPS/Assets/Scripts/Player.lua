local walkSpeed, sprintSpeed = 5.0, 8.0
local sensitivity, cameraHeight = 0.01, 0.55
local paused, sendTimer = false, 0.0
local possessedControllerID = 0
local remotePawns, remotePlayersByEntity = {}, {}

-- Duel gameplay state. The host is authoritative for health, score and respawns.
local MAX_HEALTH, SHOT_DAMAGE = 100, 25
local FIRE_INTERVAL, RESPAWN_DELAY = 0.25, 2.0
local fireCooldown = 0.0
local health = {[1]=MAX_HEALTH,[2]=MAX_HEALTH}
local score = {[1]=0,[2]=0}
local respawnTimers = {}
local CHANNEL_COMBAT = 20

local function setPaused(value)
    paused=value; Scene.SetPaused(value); UI.SetVisible("PauseMenu",value); Input.SetCursorVisible(value)
end

local function movePlayerToStart(playerID)
    local start=Controller.GetPlayerStart(playerID)
    if not start.valid then return end
    if playerID==Controller.GetLocalID() then transform.SetPosition(start.x,start.y,start.z)
    else
        local entityID=remotePawns[playerID]
        if entityID and entityID~=0 then Scene.SetPosition(entityID,start.x,start.y,start.z) end
    end
end

local function updatePossessionAndSpawn()
    local controllerID=Controller.GetLocalID()
    if controllerID==0 or controllerID==possessedControllerID then return end
    Controller.Possess(self.id,controllerID); possessedControllerID=controllerID; movePlayerToStart(controllerID)
end

local function getOrCreateRemotePawn(playerID)
    local entityID=remotePawns[playerID]
    if entityID and entityID~=0 then return entityID end
    entityID=Scene.InstantiatePrefab("Assets/Prefabs/RemotePawn.prefab",0)
    if entityID==0 then return 0 end
    Controller.Possess(entityID,playerID)
    remotePawns[playerID]=entityID; remotePlayersByEntity[entityID]=playerID
    movePlayerToStart(playerID)
    return entityID
end

local function broadcastState()
    Network.SendMessage(CHANNEL_COMBAT,"STATE:"..health[1]..":"..health[2]..":"..score[1]..":"..score[2])
end

local function respawnPlayer(playerID)
    health[playerID]=MAX_HEALTH
    movePlayerToStart(playerID)
    Network.SendMessage(CHANNEL_COMBAT,"RESPAWN:"..playerID)
    broadcastState()
end

local function applyHostShot(shooterID,targetID)
    if not Network.IsHost() or shooterID==targetID or not health[targetID] or health[targetID]<=0 then return end
    health[targetID]=math.max(0,health[targetID]-SHOT_DAMAGE)
    if health[targetID]==0 then
        score[shooterID]=(score[shooterID] or 0)+1
        respawnTimers[targetID]=RESPAWN_DELAY
    end
    broadcastState()
end

local function processCombatMessages()
    for _,message in ipairs(Network.ConsumeMessages()) do
        if message.channel==CHANNEL_COMBAT then
            local target=string.match(message.payload,"^SHOT:(%d+)$")
            if target and Network.IsHost() then
                applyHostShot(message.senderID,tonumber(target))
            else
                local h1,h2,s1,s2=string.match(message.payload,"^STATE:(%d+):(%d+):(%d+):(%d+)$")
                if h1 then
                    health[1],health[2]=tonumber(h1),tonumber(h2)
                    score[1],score[2]=tonumber(s1),tonumber(s2)
                else
                    local respawnID=string.match(message.payload,"^RESPAWN:(%d+)$")
                    if respawnID then movePlayerToStart(tonumber(respawnID)) end
                end
            end
        end
    end
end

local function updateNetworking(dt)
    if not Network.IsConnected() then return end
    updatePossessionAndSpawn()
    if not Network.IsReady() or not Controller.IsLocallyControlled(self.id) then return end

    sendTimer=sendTimer+dt
    if sendTimer>=1.0/30.0 then
        sendTimer=0.0
        local p=transform.GetPosition(); local r=self:GetRotation()
        Network.SendTransform(p.x,p.y,p.z,r.x,r.y,r.z)
    end

    local localID=Controller.GetLocalID()
    for _,remote in ipairs(Network.GetRemoteTransforms()) do
        if remote.playerID~=localID then
            local entityID=getOrCreateRemotePawn(remote.playerID)
            if entityID~=0 then
                Scene.SetPosition(entityID,remote.x,remote.y,remote.z)
                Scene.SetRotation(entityID,remote.rx,remote.ry,remote.rz)
            end
        end
    end
end

local function updateHostRespawns(dt)
    if not Network.IsHost() then return end
    for playerID,timer in pairs(respawnTimers) do
        timer=timer-dt
        if timer<=0 then respawnTimers[playerID]=nil; respawnPlayer(playerID)
        else respawnTimers[playerID]=timer end
    end
end

local function fire()
    if fireCooldown>0 or health[Controller.GetLocalID()]==0 then return end
    fireCooldown=FIRE_INTERVAL
    local c,f=Camera.GetPosition(),Camera.GetForward(); local range=100.0
    local hit=Physics.Raycast(c.x,c.y,c.z,f.x,f.y,f.z,range,self.id)
    if hit.hit then
        Debug.DrawLine(c.x,c.y,c.z,hit.x,hit.y,hit.z,0.2,1.0,0.2,5.0)
        local targetID=remotePlayersByEntity[hit.entityID]
        if targetID then
            if Network.IsHost() then applyHostShot(Controller.GetLocalID(),targetID)
            else Network.SendMessage(CHANNEL_COMBAT,"SHOT:"..targetID) end
        end
    else
        Debug.DrawLine(c.x,c.y,c.z,c.x+f.x*range,c.y+f.y*range,c.z+f.z*range,1.0,0.2,0.2,5.0)
    end
end

function OnCreate()
    State.SetNumber("mouse_sensitivity",tonumber(Preferences.LoadString("mouse_sensitivity","0.01")) or 0.01)
    State.SetBool("invert_y",Preferences.LoadString("invert_y","0")=="1")
    Controller.Possess(self.id); possessedControllerID=Controller.GetLocalID(); movePlayerToStart(possessedControllerID)
    UI.Load("Assets/UI/Pause.ui"); UI.SetVisible("PauseMenu",false); Scene.SetPaused(false); Input.SetCursorVisible(false)
end

function OnUpdate(dt)
    if not Network.IsConnected() then
        if Input.IsKeyPressed("H") then Network.Host(7777); updatePossessionAndSpawn(); broadcastState()
        elseif Input.IsKeyPressed("J") then Network.Join("127.0.0.1",7777) end
    end

    fireCooldown=math.max(0,fireCooldown-dt)
    updateNetworking(dt); updatePossessionAndSpawn(); processCombatMessages(); updateHostRespawns(dt)
    if not Controller.IsLocallyControlled(self.id) then return end

    if Input.IsKeyPressed("Escape") then setPaused(not paused); return end
    if paused then if UI.WasClicked("ResumeButton") then setPaused(false) end; return end

    -- Keep useful combat state visible in the runtime Inspector.
    local localID=Controller.GetLocalID()
    State.SetNumber("duel_health",health[localID] or MAX_HEALTH)
    State.SetNumber("duel_score",score[localID] or 0)
    State.SetNumber("duel_opponent_score",score[localID==1 and 2 or 1] or 0)

    if (health[localID] or MAX_HEALTH)<=0 then return end

    sensitivity=State.GetNumber("mouse_sensitivity",0.01)
    local invert=State.GetBool("invert_y",false) and 1.0 or -1.0
    Camera.Rotate(Input.GetMouseDeltaX()*sensitivity,Input.GetMouseDeltaY()*sensitivity*invert)

    local ix,iz=0.0,0.0
    if Input.IsKeyDown("W") then iz=iz+1 end; if Input.IsKeyDown("S") then iz=iz-1 end
    if Input.IsKeyDown("D") then ix=ix+1 end; if Input.IsKeyDown("A") then ix=ix-1 end
    local forward,right=Camera.GetForward(),Camera.GetRight()
    local mx,mz=right.x*ix+forward.x*iz,right.z*ix+forward.z*iz
    local length=math.sqrt(mx*mx+mz*mz); local speed=Input.IsKeyDown("Left Shift") and sprintSpeed or walkSpeed
    if length>0 then mx,mz=mx/length*speed,mz/length*speed end
    CharacterController.Move(mx,mz); if Input.IsKeyPressed("Space") then CharacterController.Jump() end

    local p=transform.GetPosition(); Camera.SetPosition(p.x,p.y+cameraHeight,p.z)
    if Input.IsMouseButtonDown(1) then fire() end
end
