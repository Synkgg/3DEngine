local WeaponSystem = require("Scripts.Weapons.WeaponSystem")
local Pistol = require("Scripts.Weapons.Pistol")

local walkSpeed, sprintSpeed = 5.0, 8.0
local sensitivity = 0.01
local paused, sendTimer, stateBroadcastTimer = false, 0.0, 0.0
local possessedControllerID = 0
local remotePawns, remotePlayersByEntity, remoteTargets = {}, {}, {}
local REMOTE_INTERPOLATION_SPEED = 20.0

local MAX_HEALTH = 100
local ROUNDS_TO_WIN = 5
local WARMUP_DURATION, ROUND_END_DURATION = 3.0, 3.0
local CHANNEL_COMBAT = 20
local WAITING, WARMUP, ROUND_ACTIVE, ROUND_END, MATCH_END = 0, 1, 2, 3, 4

local hitmarkerTimer, muzzleTimer = 0.0, 0.0
local weapons = nil
local health, score = {[1]=MAX_HEALTH,[2]=MAX_HEALTH}, {[1]=0,[2]=0}
local matchState, stateTimer, roundNumber = WAITING, 0.0, 0
local roundWinner, matchWinner = 0, 0
local activeIntroTimer = 0.0
local playerCamera = 0
local practiceMode = false
local practiceHits = 0

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
    else
        local entityID=remotePawns[playerID]
        if entityID and entityID~=0 then
            Scene.SetPosition(entityID,start.x,start.y,start.z)
            remoteTargets[playerID]=nil
        end
    end
end

local function spawnRoundPlayers()
    movePlayerToStart(1)
    movePlayerToStart(2)
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
    if not Network.IsHost() then return end
    local timerTenths=math.max(0,math.floor(stateTimer*10+0.5))
    Network.SendMessage(CHANNEL_COMBAT,
        "STATE:"..matchState..":"..timerTenths..":"..roundNumber..":"..
        health[1]..":"..health[2]..":"..score[1]..":"..score[2]..":"..
        roundWinner..":"..matchWinner)
end

local function beginWarmup()
    if not Network.IsHost() then return end
    roundNumber=roundNumber+1
    health[1],health[2]=MAX_HEALTH,MAX_HEALTH
    roundWinner,matchWinner=0,0
    matchState=WARMUP
    stateTimer=WARMUP_DURATION
    activeIntroTimer=0.0
    if weapons then weapons:ResetAmmo(false) end
    hitmarkerTimer,muzzleTimer=0.0,0.0
    spawnRoundPlayers()
    broadcastState()
end

local function beginMatch()
    if not Network.IsHost() then return end
    score[1],score[2]=0,0
    roundNumber=0
    roundWinner,matchWinner=0,0
    if Network.GetPlayerCount()>=2 then beginWarmup()
    else
        matchState=WAITING
        stateTimer=0.0
        health[1],health[2]=MAX_HEALTH,MAX_HEALTH
        broadcastState()
    end
end

local function finishRound(winnerID)
    if not Network.IsHost() or matchState~=ROUND_ACTIVE then return end
    roundWinner=winnerID
    score[winnerID]=(score[winnerID] or 0)+1
    if score[winnerID]>=ROUNDS_TO_WIN then
        matchWinner=winnerID
        matchState=MATCH_END
        stateTimer=0.0
    else
        matchState=ROUND_END
        stateTimer=ROUND_END_DURATION
    end
    broadcastState()
end

local function applyHostShot(shooterID,targetID,damage)
    if not Network.IsHost() or matchState~=ROUND_ACTIVE or shooterID==targetID or
       not health[targetID] or health[targetID]<=0 then return end
    health[targetID]=math.max(0,health[targetID]-(damage or 25))
    if health[targetID]==0 then finishRound(shooterID)
    else broadcastState() end
end

local function applyReplicatedState(newState,newTimer,newRound,h1,h2,s1,s2,newRoundWinner,newMatchWinner)
    local previousState=matchState
    matchState,stateTimer,roundNumber=newState,newTimer,newRound
    health[1],health[2]=h1,h2
    score[1],score[2]=s1,s2
    roundWinner,matchWinner=newRoundWinner,newMatchWinner
    if previousState~=matchState and matchState==WARMUP then spawnRoundPlayers() end
end

local function processCombatMessages()
    for _,message in ipairs(Network.ConsumeMessages()) do
        if message.channel==CHANNEL_COMBAT then
            local target,damage=string.match(message.payload,"^SHOT:(%d+):(%d+)$")
            if target and Network.IsHost() then
                applyHostShot(message.senderID,tonumber(target),tonumber(damage))
            else
                local st,timer,rnd,h1,h2,s1,s2,rw,mw=string.match(
                    message.payload,
                    "^STATE:(%d+):(%d+):(%d+):(%d+):(%d+):(%d+):(%d+):(%d+):(%d+)$")
                if st and not Network.IsHost() then
                    applyReplicatedState(
                        tonumber(st),tonumber(timer)/10.0,tonumber(rnd),
                        tonumber(h1),tonumber(h2),tonumber(s1),tonumber(s2),
                        tonumber(rw),tonumber(mw))
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
            local bodyYaw=math.deg(math.atan(-forward.x,-forward.z))
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
            Scene.SetPosition(entityID,
                p.x+(target.x-p.x)*alpha,
                p.y+(target.y-p.y)*alpha,
                p.z+(target.z-p.z)*alpha)
            Scene.SetRotation(entityID,target.rx,target.ry,target.rz)
        end
    end
end

local function updateHostMatch(dt)
    if not Network.IsHost() then return end

    if Network.GetPlayerCount()<2 then
        if matchState~=WAITING then
            matchState=WAITING
            stateTimer=0.0
            roundWinner,matchWinner=0,0
            health[1],health[2]=MAX_HEALTH,MAX_HEALTH
            broadcastState()
        end
        return
    end

    if matchState==WAITING then
        beginWarmup()
        return
    end

    if matchState==WARMUP or matchState==ROUND_END then
        stateTimer=math.max(0,stateTimer-dt)
        if stateTimer<=0 then
            if matchState==WARMUP then
                matchState=ROUND_ACTIVE
                stateTimer=0.0
                activeIntroTimer=0.65
                broadcastState()
            else
                beginWarmup()
            end
        end
    end

    stateBroadcastTimer=stateBroadcastTimer+dt
    if stateBroadcastTimer>=0.25 then
        stateBroadcastTimer=0
        broadcastState()
    end
end

local function handleWeaponShot(result)
    if not result then return end
    muzzleTimer=0.055
    if result.hit then
        Debug.DrawLine(result.originX,result.originY,result.originZ,result.x,result.y,result.z,0.2,1.0,0.2,0.08)
        local targetID=remotePlayersByEntity[result.entityID]
        if practiceMode then
            local target=false
            for _,name in ipairs({"Target_10m","Target_15m","Target_20m","Target_25m_Left","Target_25m_Right","Target_35m"}) do
                local e=Scene.FindEntity(name)
                if e:IsValid() and e.id==result.entityID then target=true break end
            end
            if target then
                practiceHits=practiceHits+1
                hitmarkerTimer=0.12
                Audio.PlaySFX("Assets/Audio/Weapons/hit.wav",0.7)
            end
        elseif targetID then
            hitmarkerTimer=0.12
            Audio.PlaySFX("Assets/Audio/Weapons/hit.wav",0.7)
            local localID=Controller.GetLocalID()
            if Network.IsHost() then applyHostShot(localID,targetID,result.damage)
            else Network.SendMessage(CHANNEL_COMBAT,"SHOT:"..targetID..":"..math.floor(result.damage)) end
        end
    else
        Debug.DrawLine(result.originX,result.originY,result.originZ,
            result.originX+result.forwardX*result.range,
            result.originY+result.forwardY*result.range,
            result.originZ+result.forwardZ*result.range,1.0,0.2,0.2,0.08)
    end
end

local function consumeWeaponPickup()
    local request=math.floor(State.GetNumber("duelfps_weapon_pickup",0))
    if request==0 or not weapons then return end
    State.SetNumber("duelfps_weapon_pickup",0)
    if request==1 then
        weapons:Give("pistol",practiceMode and Pistol.practiceReserve or Pistol.startingReserve)
    end
end

local function returnToMenu()
    setPaused(false)
    Network.Disconnect()
    Input.SetCursorVisible(true)
    Scene.Load("Assets/Scenes/MainMenu.scene")
end

local function updateHUD()
    local localID=practiceMode and 1 or Controller.GetLocalID()
    local opponentID=localID==1 and 2 or 1
    local localHealth=health[localID] or MAX_HEALTH
    local dead=localHealth<=0
    local localScore=score[localID] or 0
    local enemyScore=score[opponentID] or 0
    local scoreLine="YOU "..localScore.."  —  "..enemyScore.." ENEMY"

    State.SetNumber("duel_health",localHealth)
    State.SetNumber("duel_score",localScore)
    State.SetNumber("duel_opponent_score",enemyScore)
    UI.SetValue("HealthBar",math.max(0.0,math.min(1.0,localHealth/MAX_HEALTH)))
    UI.SetText("HealthText",tostring(localHealth).." / "..tostring(MAX_HEALTH))
    UI.SetText("ScoreText",scoreLine)
    local ammo,reserve=weapons and weapons:GetAmmo() or 0,0
    local weaponDef=weapons and weapons:GetDefinition() or nil
    UI.SetText("AmmoText",weaponDef and (tostring(ammo).." / "..tostring(reserve)) or "UNARMED")
    UI.SetVisible("ReloadText",weapons and weapons:IsReloading() or false)
    if weapons and weapons:IsReloading() then UI.SetText("ReloadText","RELOADING") end
    UI.SetVisible("Hitmarker",hitmarkerTimer>0)
    UI.SetVisible("MuzzleFlash",muzzleTimer>0)

    local showWarmup=matchState==WARMUP
    local showRoundResult=matchState==ROUND_END
    local showMatchResult=matchState==MATCH_END
    UI.SetVisible("RoundIntro",showWarmup or (matchState==ROUND_ACTIVE and activeIntroTimer>0))
    UI.SetVisible("RoundResult",showRoundResult)
    UI.SetVisible("MatchResult",showMatchResult)
    UI.SetVisible("DeathOverlay",dead and (matchState==ROUND_END or matchState==MATCH_END))
    UI.SetVisible("CrosshairH",matchState==ROUND_ACTIVE and not dead)
    UI.SetVisible("CrosshairV",matchState==ROUND_ACTIVE and not dead)
    UI.SetVisible("MatchActions",showMatchResult)

    if matchState==WAITING then
        UI.SetText("MatchStatus","WAITING FOR OPPONENT")
        UI.SetText("CenterMessage","WAITING FOR PLAYER 2")
    elseif showWarmup then
        local countdown=math.max(1,math.ceil(stateTimer))
        UI.SetText("MatchStatus","ROUND "..roundNumber.." // FIRST TO "..ROUNDS_TO_WIN)
        UI.SetText("CenterMessage","")
        UI.SetText("RoundIntroEyebrow","ROUND "..roundNumber)
        UI.SetText("RoundIntroTitle",tostring(countdown))
        UI.SetText("RoundIntroHint","GET READY")
    elseif matchState==ROUND_ACTIVE then
        UI.SetText("MatchStatus","ROUND "..roundNumber.." // FIRST TO "..ROUNDS_TO_WIN)
        UI.SetText("CenterMessage","")
        if activeIntroTimer>0 then
            UI.SetText("RoundIntroEyebrow","ROUND "..roundNumber)
            UI.SetText("RoundIntroTitle","FIGHT")
            UI.SetText("RoundIntroHint","")
        end
    elseif showRoundResult then
        UI.SetText("MatchStatus","ROUND "..roundNumber.." COMPLETE")
        UI.SetText("CenterMessage","")
        UI.SetText("RoundResultTitle",roundWinner==localID and "ROUND WON" or "ROUND LOST")
        UI.SetText("RoundResultScore",scoreLine)
        UI.SetText("RoundResultNext","NEXT ROUND IN "..math.max(1,math.ceil(stateTimer)))
    elseif showMatchResult then
        UI.SetText("MatchStatus","MATCH COMPLETE")
        UI.SetText("CenterMessage","")
        UI.SetText("MatchResultTitle",matchWinner==localID and "VICTORY" or "DEFEAT")
        UI.SetText("MatchResultScore",scoreLine)
        UI.SetText("MatchResultHint",matchWinner==localID and "MATCH WON" or "MATCH LOST")
        UI.SetText("RematchLabel",Network.IsHost() and "REMATCH" or "HOST CAN START REMATCH")
    end
end

function OnCreate()
    practiceMode=Scene.FindEntity("PracticeMode"):IsValid()
    State.SetNumber("mouse_sensitivity",tonumber(Preferences.LoadString("mouse_sensitivity","0.01")) or 0.01)
    State.SetBool("invert_y",Preferences.LoadString("invert_y","0")=="1")
    UI.Load("Assets/UI/Duel.ui")
    UI.SetVisible("Lobby",false)
    UI.SetVisible("PauseMenu",false)
    UI.SetVisible("DeathOverlay",false)
    UI.SetVisible("RoundIntro",false)
    UI.SetVisible("RoundResult",false)
    UI.SetVisible("MatchResult",false)
    UI.SetVisible("MatchActions",false)
    UI.SetVisible("ReloadText",false)
    UI.SetVisible("Hitmarker",false)
    UI.SetVisible("MuzzleFlash",false)
    playerCamera=Scene.FindEntity("FirstPersonCamera").id
    if playerCamera~=0 then Camera.SetActive(playerCamera) end
    weapons=WeaponSystem.new({
        Scene=Scene, Camera=Camera, Physics=Physics, Audio=Audio, Input=Input
    })
    weapons:Register(Pistol)
    State.SetNumber("duelfps_weapon_pickup",0)
    Input.SetCursorVisible(false)
    if practiceMode then
        possessedControllerID=1
        matchState=ROUND_ACTIVE
        health[1]=MAX_HEALTH
        UI.SetText("MatchStatus","PRACTICE RANGE // TARGET DRILL")
        UI.SetText("CenterMessage","")
    else
        weapons:Give("pistol",Pistol.startingReserve)
        updatePossessionAndSpawn()
        if Network.IsHost() then beginMatch() end
    end
end

function OnResumeClicked()
    if paused then setPaused(false) end
end

function OnDisconnectClicked()
    if paused then returnToMenu() end
end

function OnRematchClicked()
    if matchState==MATCH_END and Network.IsHost() then
        Input.SetCursorVisible(false)
        beginMatch()
    end
end

function OnReturnToMenuClicked()
    if matchState==MATCH_END then returnToMenu() end
end

function OnUpdate(dt)
    if not practiceMode and not Network.IsConnected() then
        Input.SetCursorVisible(true)
        Scene.Load("Assets/Scenes/MainMenu.scene")
        return
    end

    if not practiceMode then
        updatePossessionAndSpawn()
        updateNetworking(dt)
        updateRemoteInterpolation(dt)
        processCombatMessages()
        updateHostMatch(dt)
    end
    activeIntroTimer=math.max(0,activeIntroTimer-dt)
    hitmarkerTimer=math.max(0,hitmarkerTimer-dt)
    muzzleTimer=math.max(0,muzzleTimer-dt)

    if not practiceMode and not Controller.IsLocallyControlled(self.id) then return end

    consumeWeaponPickup()
    if weapons then weapons:Update(dt,playerCamera) end
    updateHUD()
    if practiceMode then
        UI.SetText("MatchStatus","PRACTICE RANGE // HITS "..practiceHits)
        UI.SetText("ScoreText","TARGET HITS  "..practiceHits)
        local prompt=Scene.GetInteractionPrompt()
        UI.SetText("CenterMessage",prompt~="" and ("[E]  "..string.upper(prompt)) or "")
        UI.SetVisible("RoundIntro",false)
        UI.SetVisible("RoundResult",false)
        UI.SetVisible("MatchResult",false)
        UI.SetVisible("DeathOverlay",false)
        UI.SetVisible("CrosshairH",true)
        UI.SetVisible("CrosshairV",true)
    end

    if Input.IsKeyPressed("Escape") and matchState~=MATCH_END then
        setPaused(not paused)
        if paused then CharacterController.Move(0.0,0.0) end
        return
    end

    if paused then
        CharacterController.Move(0.0,0.0)
        return
    end

    if matchState==MATCH_END then
        CharacterController.Move(0.0,0.0)
        Input.SetCursorVisible(true)
        return
    end

    if not practiceMode and (matchState~=ROUND_ACTIVE or (health[Controller.GetLocalID()] or 0)<=0) then
        CharacterController.Move(0.0,0.0)
        return
    end

    Input.SetCursorVisible(false)
    sensitivity=State.GetNumber("mouse_sensitivity",0.01)
    local invert=State.GetBool("invert_y",false) and 1.0 or -1.0
    if playerCamera~=0 then
        Camera.RotateEntity(playerCamera,Input.GetMouseDeltaX()*sensitivity,Input.GetMouseDeltaY()*sensitivity*invert)
    else
        Camera.Rotate(Input.GetMouseDeltaX()*sensitivity,Input.GetMouseDeltaY()*sensitivity*invert)
    end

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
    if Input.IsKeyPressed("R") and weapons then weapons:Reload() end
    if Input.IsMouseButtonDown(1) and weapons then
        handleWeaponShot(weapons:Fire(self.id,playerCamera))
    end
end
