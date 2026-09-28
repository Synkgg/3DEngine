local walkSpeed, sprintSpeed = 5.0, 8.0
local sensitivity, cameraHeight = 0.01, 0.55
local paused, sendTimer = false, 0.0
local stateBroadcastTimer = 0.0
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
local SCORE_LIMIT = 5
local matchWinner = 0
local respawnUntil = 0.0
local enteredOnlineMatch = false

local function setPaused(value)
    paused=value
    -- Online menus never pause simulation/networking. Offline editor play may still truly pause.
    Scene.SetPaused(value and not Network.IsConnected())
    UI.SetVisible("PauseMenu",value)
    Input.SetCursorVisible(value)
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
    Network.SendMessage(CHANNEL_COMBAT,"STATE:"..health[1]..":"..health[2]..":"..score[1]..":"..score[2]..":"..matchWinner)
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
        if score[shooterID]>=SCORE_LIMIT then matchWinner=shooterID; respawnTimers={} else respawnTimers[targetID]=RESPAWN_DELAY end
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
                local h1,h2,s1,s2,winner=string.match(message.payload,"^STATE:(%d+):(%d+):(%d+):(%d+):(%d+)$")
                if h1 then
                    health[1],health[2]=tonumber(h1),tonumber(h2)
                    score[1],score[2]=tonumber(s1),tonumber(s2); matchWinner=tonumber(winner) or 0
                else
                    local respawnID=string.match(message.payload,"^RESPAWN:(%d+)$")
                    if respawnID then movePlayerToStart(tonumber(respawnID)) end
                end
            end
        end
    end
end

local function updateNetworking(dt)
    if not Network.IsConnected() then
        Input.SetCursorVisible(true)
        Scene.Load("Assets/Scenes/MainMenu.scene")
        return
    elseif Network.IsReady() and not enteredOnlineMatch then
        enteredOnlineMatch=true
        paused=false
        Scene.SetPaused(false)
        UI.SetVisible("PauseMenu",false)
        Input.SetCursorVisible(false)
        updatePossessionAndSpawn()
    end

    fireCooldown=math.max(0,fireCooldown-dt)
    updateNetworking(dt); updatePossessionAndSpawn(); processCombatMessages(); updateHostRespawns(dt)
    if not Controller.IsLocallyControlled(self.id) then return end

    if Input.IsKeyPressed("Escape") then setPaused(not paused); return end
    if paused then
        if UI.WasClicked("ResumeButton") then setPaused(false)
        elseif UI.WasClicked("DisconnectButton") then
            setPaused(false)
            Network.Disconnect()
            enteredOnlineMatch=false
            matchWinner=0; score[1],score[2]=0,0; health[1],health[2]=MAX_HEALTH,MAX_HEALTH
            Input.SetCursorVisible(true)
            Scene.Load("Assets/Scenes/MainMenu.scene")
            return
        end
        -- Do not return from networking above; only suppress local gameplay input while menu is open.
        return
    end

    -- Keep useful combat state visible in the runtime Inspector.
    local localID=Controller.GetLocalID()
    State.SetNumber("duel_health",health[localID] or MAX_HEALTH)
    State.SetNumber("duel_score",score[localID] or 0)
    local opponentID=localID==1 and 2 or 1
    State.SetNumber("duel_opponent_score",score[opponentID] or 0)
    UI.SetText("HealthText","HP "..tostring(health[localID] or MAX_HEALTH))
    UI.SetText("ScoreText","YOU "..tostring(score[localID] or 0).."  //  "..tostring(score[opponentID] or 0).." OPPONENT")
    UI.SetVisible("RestartMatchButton",Network.IsHost() and matchWinner~=0)
    if matchWinner~=0 then
        UI.SetText("MatchStatus","FIRST TO "..SCORE_LIMIT.." // MATCH COMPLETE")
        UI.SetText("CenterMessage",matchWinner==localID and "VICTORY" or "DEFEAT")
        Input.SetCursorVisible(Network.IsHost())
        if Network.IsHost() and UI.WasClicked("RestartMatchButton") then
            health[1],health[2]=MAX_HEALTH,MAX_HEALTH; score[1],score[2]=0,0; matchWinner=0; respawnTimers={}
            respawnPlayer(1); respawnPlayer(2); broadcastState(); UI.SetText("CenterMessage",""); Input.SetCursorVisible(false)
        end
        return
    else
        UI.SetText("MatchStatus","FIRST TO "..SCORE_LIMIT)
        UI.SetText("CenterMessage","")
    end

    if (health[localID] or MAX_HEALTH)<=0 then
        UI.SetText("CenterMessage","ELIMINATED // RESPAWNING")
        return
    end

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
