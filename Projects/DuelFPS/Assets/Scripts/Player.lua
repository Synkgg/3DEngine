local walkSpeed, sprintSpeed = 5.0, 8.0
local sensitivity, cameraHeight = 0.01, 0.55
local paused, sendTimer, stateBroadcastTimer = false, 0.0, 0.0
local possessedControllerID = 0
local remotePawns, remotePlayersByEntity = {}, {}
local remoteTargets = {}
local REMOTE_INTERPOLATION_SPEED = 20.0

local MAX_HEALTH, SHOT_DAMAGE = 100, 25
local FIRE_INTERVAL, RESPAWN_DELAY, SCORE_LIMIT = 0.25, 2.0, 5
local CHANNEL_COMBAT = 20
local fireCooldown, matchWinner = 0.0, 0
local health, score = {[1]=MAX_HEALTH,[2]=MAX_HEALTH}, {[1]=0,[2]=0}
local respawnTimers = {}

local function setPaused(value)
    paused=value
    UI.SetVisible("PauseMenu",value)
    Input.SetCursorVisible(value)
end

local function movePlayerToStart(playerID)
    local start=Controller.GetPlayerStart(playerID)
    if not start.valid then return end
    if playerID==Controller.GetLocalID() then
        transform.SetPosition(start.x,start.y,start.z)
        CharacterController.Move(0.0,0.0)
        Camera.SetPosition(start.x,start.y+cameraHeight,start.z)
    else
        local entityID=remotePawns[playerID]
        if entityID and entityID~=0 then
            Scene.SetPosition(entityID,start.x,start.y,start.z)
            remoteTargets[playerID]=nil
        end
    end
end

local function updatePossessionAndSpawn()
    local controllerID=Controller.GetLocalID()
    if controllerID==0 or controllerID==possessedControllerID then return end
    Controller.Possess(self.id,controllerID)
    possessedControllerID=controllerID
    movePlayerToStart(controllerID)
end

local function getOrCreateRemotePawn(playerID)
    local entityID=remotePawns[playerID]
    if entityID and entityID~=0 then return entityID end
    entityID=Scene.InstantiatePrefab("Assets/Prefabs/RemotePawn.prefab",0)
    if entityID==0 then return 0 end
    Controller.Possess(entityID,playerID)
    remotePawns[playerID]=entityID
    remotePlayersByEntity[entityID]=playerID
    movePlayerToStart(playerID)
    return entityID
end

local function broadcastState()
    Network.SendMessage(CHANNEL_COMBAT,"STATE:"..health[1]..":"..health[2]..":"..score[1]..":"..score[2]..":"..matchWinner)
end

local function respawnPlayer(playerID)
    health[playerID]=MAX_HEALTH
    movePlayerToStart(playerID)
    Network.SendMessage(CHANNEL_COMBAT,"RESPAWN:"..playerID)
    broadcastState()
end

local function applyHostShot(shooterID,targetID)
    if not Network.IsHost() or shooterID==targetID or not health[targetID] or health[targetID]<=0 or matchWinner~=0 then return end
    health[targetID]=math.max(0,health[targetID]-SHOT_DAMAGE)
    if health[targetID]==0 then
        score[shooterID]=(score[shooterID] or 0)+1
        if score[shooterID]>=SCORE_LIMIT then matchWinner=shooterID; respawnTimers={}
        else respawnTimers[targetID]=RESPAWN_DELAY end
    end
    broadcastState()
end

local function processCombatMessages()
    for _,message in ipairs(Network.ConsumeMessages()) do
        if message.channel==CHANNEL_COMBAT then
            local target=string.match(message.payload,"^SHOT:(%d+)$")
            if target and Network.IsHost() then applyHostShot(message.senderID,tonumber(target))
            else
                local h1,h2,s1,s2,winner=string.match(message.payload,"^STATE:(%d+):(%d+):(%d+):(%d+):(%d+)$")
                if h1 then
                    health[1],health[2]=tonumber(h1),tonumber(h2)
                    score[1],score[2]=tonumber(s1),tonumber(s2)
                    matchWinner=tonumber(winner) or 0
                else
                    local respawnID=string.match(message.payload,"^RESPAWN:(%d+)$")
                    if respawnID then movePlayerToStart(tonumber(respawnID)) end
                end
            end
        end
    end
end

local function updateNetworking(dt)
    if not Network.IsConnected() or not Network.IsReady() then return end
    if Controller.IsLocallyControlled(self.id) then
        sendTimer=sendTimer+dt
        if sendTimer>=1.0/30.0 then
            sendTimer=0
            local p=transform.GetPosition()
            local forward=Camera.GetForward()
            local bodyYaw=math.deg(math.atan2(-forward.x,-forward.z))
            Network.SendTransform(p.x,p.y,p.z,0.0,bodyYaw,0.0)
        end
    end

    local localID=Controller.GetLocalID()
    for _,remote in ipairs(Network.GetRemoteTransforms()) do
        if remote.playerID~=localID then
            local entityID=getOrCreateRemotePawn(remote.playerID)
            if entityID~=0 then
                remoteTargets[remote.playerID]={
                    x=remote.x,y=remote.y,z=remote.z,
                    rx=remote.rx,ry=remote.ry,rz=remote.rz
                }
            end
        end
    end
end

local function updateRemoteInterpolation(dt)
    local alpha=math.min(1.0,dt*REMOTE_INTERPOLATION_SPEED)
    for playerID,target in pairs(remoteTargets) do
        local entityID=remotePawns[playerID]
        if entityID and entityID~=0 then
            local p=Scene.GetPosition(entityID)
            Scene.SetPosition(
                entityID,
                p.x+(target.x-p.x)*alpha,
                p.y+(target.y-p.y)*alpha,
                p.z+(target.z-p.z)*alpha
            )
            Scene.SetRotation(entityID,target.rx,target.ry,target.rz)
        end
    end
end

local function updateHostRespawns(dt)
    if not Network.IsHost() then return end
    stateBroadcastTimer=stateBroadcastTimer+dt
    if stateBroadcastTimer>=0.5 then stateBroadcastTimer=0; broadcastState() end
    for playerID,timer in pairs(respawnTimers) do
        timer=timer-dt
        if timer<=0 then respawnTimers[playerID]=nil; respawnPlayer(playerID)
        else respawnTimers[playerID]=timer end
    end
end

local function fire()
    local localID=Controller.GetLocalID()
    if fireCooldown>0 or (health[localID] or MAX_HEALTH)<=0 or matchWinner~=0 then return end
    fireCooldown=FIRE_INTERVAL
    local c,f=Camera.GetPosition(),Camera.GetForward()
    local range=100.0
    local hit=Physics.Raycast(c.x,c.y,c.z,f.x,f.y,f.z,range,self.id)
    if hit.hit then
        Debug.DrawLine(c.x,c.y,c.z,hit.x,hit.y,hit.z,0.2,1.0,0.2,5.0)
        local targetID=remotePlayersByEntity[hit.entityID]
        if targetID then
            if Network.IsHost() then applyHostShot(localID,targetID)
            else Network.SendMessage(CHANNEL_COMBAT,"SHOT:"..targetID) end
        end
    else
        Debug.DrawLine(c.x,c.y,c.z,c.x+f.x*range,c.y+f.y*range,c.z+f.z*range,1.0,0.2,0.2,5.0)
    end
end

function OnCreate()
    State.SetNumber("mouse_sensitivity",tonumber(Preferences.LoadString("mouse_sensitivity","0.01")) or 0.01)
    State.SetBool("invert_y",Preferences.LoadString("invert_y","0")=="1")
    UI.Load("Assets/UI/Duel.ui")
    UI.SetVisible("Lobby",false)
    UI.SetVisible("PauseMenu",false)
    UI.SetVisible("RestartMatchButton",false)
    Input.SetCursorVisible(false)
    updatePossessionAndSpawn()
end

function OnUpdate(dt)
    if not Network.IsConnected() then
        Input.SetCursorVisible(true)
        Scene.Load("Assets/Scenes/MainMenu.scene")
        return
    end

    updatePossessionAndSpawn()
    updateNetworking(dt)
    updateRemoteInterpolation(dt)
    processCombatMessages()
    updateHostRespawns(dt)
    fireCooldown=math.max(0,fireCooldown-dt)

    if not Controller.IsLocallyControlled(self.id) then return end

    -- Camera and controller state must keep following the live pawn even while
    -- an online menu owns input. The world is still simulating.
    local livePosition=transform.GetPosition()
    Camera.SetPosition(livePosition.x,livePosition.y+cameraHeight,livePosition.z)

    local localID=Controller.GetLocalID()
    local opponentID=localID==1 and 2 or 1
    local localHealth=health[localID] or MAX_HEALTH
    State.SetNumber("duel_health",localHealth)
    UI.SetValue("HealthBar",math.max(0.0,math.min(1.0,localHealth/MAX_HEALTH)))
    State.SetNumber("duel_score",score[localID] or 0)
    State.SetNumber("duel_opponent_score",score[opponentID] or 0)
    UI.SetText("HealthText",tostring(localHealth).." / "..tostring(MAX_HEALTH))
    UI.SetText("ScoreText","YOU "..tostring(score[localID] or 0).."  //  "..tostring(score[opponentID] or 0).." OPPONENT")
    UI.SetVisible("RestartMatchButton",Network.IsHost() and matchWinner~=0)

    if Input.IsKeyPressed("Escape") then
        setPaused(not paused)
        if paused then CharacterController.Move(0.0,0.0) end
        return
    end
    if paused then
        CharacterController.Move(0.0,0.0)
        if UI.WasClicked("ResumeButton") then setPaused(false)
        elseif UI.WasClicked("DisconnectButton") then
            setPaused(false)
            Network.Disconnect()
            Input.SetCursorVisible(true)
            Scene.Load("Assets/Scenes/MainMenu.scene")
        end
        return
    end

    if matchWinner~=0 then
        UI.SetText("MatchStatus","FIRST TO "..SCORE_LIMIT.." // MATCH COMPLETE")
        UI.SetText("CenterMessage",matchWinner==localID and "VICTORY" or "DEFEAT")
        Input.SetCursorVisible(Network.IsHost())
        if Network.IsHost() and UI.WasClicked("RestartMatchButton") then
            health[1],health[2]=MAX_HEALTH,MAX_HEALTH
            score[1],score[2]=0,0
            matchWinner=0
            respawnTimers={}
            respawnPlayer(1); respawnPlayer(2)
            broadcastState()
            UI.SetText("CenterMessage","")
            Input.SetCursorVisible(false)
        end
        return
    end

    UI.SetText("MatchStatus","FIRST TO "..SCORE_LIMIT)
    UI.SetText("CenterMessage","")
    if (health[localID] or MAX_HEALTH)<=0 then
        UI.SetText("CenterMessage","ELIMINATED // RESPAWNING")
        return
    end

    sensitivity=State.GetNumber("mouse_sensitivity",0.01)
    local invert=State.GetBool("invert_y",false) and 1.0 or -1.0
    Camera.Rotate(Input.GetMouseDeltaX()*sensitivity,Input.GetMouseDeltaY()*sensitivity*invert)

    local ix,iz=0.0,0.0
    if Input.IsKeyDown("W") then iz=iz+1 end
    if Input.IsKeyDown("S") then iz=iz-1 end
    if Input.IsKeyDown("D") then ix=ix+1 end
    if Input.IsKeyDown("A") then ix=ix-1 end
    local forward,right=Camera.GetForward(),Camera.GetRight()
    local mx,mz=right.x*ix+forward.x*iz,right.z*ix+forward.z*iz
    local length=math.sqrt(mx*mx+mz*mz)
    local speed=Input.IsKeyDown("Left Shift") and sprintSpeed or walkSpeed
    if length>0 then mx,mz=mx/length*speed,mz/length*speed end
    CharacterController.Move(mx,mz)
    if Input.IsKeyPressed("Space") then CharacterController.Jump() end

    if Input.IsMouseButtonDown(1) then fire() end
end
