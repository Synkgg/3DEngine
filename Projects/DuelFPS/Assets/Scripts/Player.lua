local WeaponSystem = require("Scripts.Weapons.WeaponSystem")
local Pistol = require("Scripts.Weapons.Pistol")
local Rifle = require("Scripts.Weapons.Rifle")
local Shotgun = require("Scripts.Weapons.Shotgun")
local SMG = require("Scripts.Weapons.SMG")
local ShotPattern = require("Scripts.Weapons.ShotPattern")
local gunDefs={pistol=Pistol,rifle=Rifle,shotgun=Shotgun,smg=SMG}
local loadoutPrimary,loadoutSecondary="rifle","pistol"
local loadoutSent=false
local playerLoadouts={}

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

local hitmarkerTimer = 0.0
local weapons = nil
local health, score = {[1]=MAX_HEALTH,[2]=MAX_HEALTH}, {[1]=0,[2]=0}
local matchState, stateTimer, roundNumber = WAITING, 0.0, 0
local roundWinner, matchWinner = 0, 0
local activeIntroTimer = 0.0
local playerCamera = 0
local practiceMode = false
local practiceHits = 0
local damageTimer,previousHealth,stepTimer=0,100,0
local ROUND_DURATION=90
local elapsed,shotSequence,stateSequence,lastStateSequence=0,0,0,-1
local lastShotSequence,lastShotTime={},{}
local pendingShots={}
local rematchVotes={[1]=false,[2]=false}
local rematchRetry=0


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
        Scene.SetRotation(self.id,0,0,0)
        if playerCamera~=0 then Scene.SetRotation(playerCamera,0,playerID==1 and -90 or 90,0) end
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
    stateSequence=stateSequence+1
    Network.SendMessage(CHANNEL_COMBAT,
        "STATE:"..stateSequence..":"..matchState..":"..timerTenths..":"..roundNumber..":"..
        health[1]..":"..health[2]..":"..score[1]..":"..score[2]..":"..
        roundWinner..":"..matchWinner)
end

local function beginWarmup()
    if not Network.IsHost() then return end
    roundNumber=roundNumber+1
    pendingShots={};lastShotTime={};rematchVotes={[1]=false,[2]=false}
    health[1],health[2]=MAX_HEALTH,MAX_HEALTH
    roundWinner,matchWinner=0,0
    matchState=WARMUP
    stateTimer=WARMUP_DURATION
    activeIntroTimer=0.0
    if weapons then weapons:ResetAmmo(false) end
    hitmarkerTimer=0.0
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
    if winnerID>0 then score[winnerID]=(score[winnerID] or 0)+1 end
    if winnerID>0 and score[winnerID]>=ROUNDS_TO_WIN then
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
    if previousState~=matchState and matchState==WARMUP then
        spawnRoundPlayers();pendingShots={};rematchVotes={[1]=false,[2]=false}
        if weapons then weapons:ResetAmmo(false) end
        Input.SetCursorVisible(false)
    end
end

local function validateShot(shooterID,sequence,round,weaponID,ox,oy,oz,dx,dy,dz)
    if not Network.IsHost() or matchState~=ROUND_ACTIVE or round~=roundNumber then return end
    if shooterID~=1 and shooterID~=2 then return end
    if sequence<=(lastShotSequence[shooterID] or 0) then return end
    lastShotSequence[shooterID]=sequence
    local def=gunDefs[weaponID]
    local equipped=playerLoadouts[shooterID]
    if not def or not equipped or (weaponID~=equipped[1] and weaponID~=equipped[2]) then return false end
    if elapsed-(lastShotTime[shooterID] or -100)<def.fireInterval*.8 then return false end
    if (health[shooterID] or 0)<=0 then return end
    local shooter=shooterID==Controller.GetLocalID() and self.id or remotePawns[shooterID]
    if not shooter then return end
    local p=Scene.GetPosition(shooter)
    if (ox-p.x)^2+(oy-p.y)^2+(oz-p.z)^2>9 then return end
    local length=dx*dx+dy*dy+dz*dz
    if length<.9 or length>1.1 then return end
    lastShotTime[shooterID]=elapsed
    local damageByTarget={}
    for _,dir in ipairs(ShotPattern.Directions(dx,dy,dz,def.pellets or 1,def.spread or 0)) do
        local hit=Physics.Raycast(ox,oy,oz,dir.x,dir.y,dir.z,def.range,shooter)
        local target=hit.entityID==self.id and Controller.GetLocalID() or remotePlayersByEntity[hit.entityID]
        if hit.hit and target and target~=shooterID then
            damageByTarget[target]=(damageByTarget[target] or 0)+def.damage
        end
    end
    for target,damage in pairs(damageByTarget) do applyHostShot(shooterID,target,damage) end
    return true
end

local function processCombatMessages()
    for _,message in ipairs(Network.ConsumeMessages()) do
        if message.channel==CHANNEL_COMBAT then
            local parts={}
            for token in message.payload:gmatch("[^:]+") do parts[#parts+1]=token end
            if parts[1]=="SHOT" and Network.IsHost() and #parts==10 then
                local numbers={};local valid=true
                for _,i in ipairs({2,3,5,6,7,8,9,10}) do
                    numbers[i]=tonumber(parts[i]);if not numbers[i] or numbers[i]~=numbers[i] or math.abs(numbers[i])>1000000 then valid=false end
                end
                if valid then
                    local accepted=validateShot(message.senderID,numbers[2],numbers[3],parts[4],numbers[5],numbers[6],numbers[7],numbers[8],numbers[9],numbers[10])
                    if accepted then
                        Network.SendMessage(CHANNEL_COMBAT,"FX:"..message.senderID..":"..parts[4]..":"..
                            parts[5]..":"..parts[6]..":"..parts[7]..":"..parts[8]..":"..parts[9]..":"..parts[10])
                        Network.SendMessage(CHANNEL_COMBAT,"ACK:"..message.senderID..":"..numbers[2])
                    end
                end
            elseif parts[1]=="LOADOUT" and Network.IsHost() and message.senderID==2 and #parts==3 then
                if gunDefs[parts[2]] and gunDefs[parts[3]] and parts[2]~=parts[3] then
                    playerLoadouts[2]={parts[2],parts[3]}
                end
            elseif parts[1]=="FX" and not Network.IsHost() and message.senderID==1 and #parts==9 then
                local shooter=tonumber(parts[2])
                if shooter and shooter~=Controller.GetLocalID() and gunDefs[parts[3]] and weapons then
                    local x,y,z=tonumber(parts[4]),tonumber(parts[5]),tonumber(parts[6])
                    local dx,dy,dz=tonumber(parts[7]),tonumber(parts[8]),tonumber(parts[9])
                    if x and y and z and dx and dy and dz then
                        weapons:SpawnRemoteShot(remotePawns[shooter] or 0,parts[3],x,y,z,dx,dy,dz)
                    end
                end
            elseif parts[1]=="ACK" and not Network.IsHost() and message.senderID==1 then
                if tonumber(parts[2])==Controller.GetLocalID() then pendingShots[tonumber(parts[3])]=nil end
            elseif parts[1]=="STATE" and not Network.IsHost() and message.senderID==1 and #parts==11 then
                local v={};local valid=true
                for i=2,11 do v[i]=tonumber(parts[i]);if not v[i] then valid=false end end
                if valid and v[2]>lastStateSequence then
                    lastStateSequence=v[2]
                    applyReplicatedState(v[3],v[4]/10,v[5],v[6],v[7],v[8],v[9],v[10],v[11])
                end
            elseif parts[1]=="REMATCH" and Network.IsHost() and matchState==MATCH_END and message.senderID==2 then
                rematchVotes[2]=true
                if rematchVotes[1] then beginMatch() end
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
    if Network.IsHost() and Network.GetPlayerCount()<2 and remotePawns[2] then
        Scene.DestroyEntity(remotePawns[2]);remotePlayersByEntity[remotePawns[2]]=nil
        remotePawns[2]=nil;remoteTargets[2]=nil;lastShotSequence[2]=nil
    end
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
                stateTimer=ROUND_DURATION
                activeIntroTimer=0.65
                broadcastState()
            else
                beginWarmup()
            end
        end
    end

    if matchState==ROUND_ACTIVE then
        stateTimer=math.max(0,stateTimer-dt)
        if stateTimer==0 then finishRound(health[1]==health[2] and 0 or (health[1]>health[2] and 1 or 2)) end
    end

    stateBroadcastTimer=stateBroadcastTimer+dt
    if stateBroadcastTimer>=0.25 then
        stateBroadcastTimer=0
        broadcastState()
    end
end

local function handleWeaponShot(result)
    if not result then return end
    if not practiceMode then
        shotSequence=shotSequence+1
        local def=weapons:GetDefinition()
        local payload=string.format("SHOT:%d:%d:%s:%.3f:%.3f:%.3f:%.4f:%.4f:%.4f",shotSequence,roundNumber,def.id,result.originX,result.originY,result.originZ,result.forwardX,result.forwardY,result.forwardZ)
        if Network.IsHost() then
            if validateShot(Controller.GetLocalID(),shotSequence,roundNumber,def.id,result.originX,result.originY,result.originZ,result.forwardX,result.forwardY,result.forwardZ) then
                Network.SendMessage(CHANNEL_COMBAT,"FX:"..Controller.GetLocalID()..":"..def.id..":"..
                    string.format("%.3f:%.3f:%.3f:%.4f:%.4f:%.4f",result.originX,result.originY,result.originZ,result.forwardX,result.forwardY,result.forwardZ))
            end
        else Network.SendMessage(CHANNEL_COMBAT,payload);pendingShots[shotSequence]={payload=payload,age=0,retry=0} end
    end
    if result.hit then
        local practiceTarget=false
        local enemyHit=false
        for _,pellet in ipairs(result.hits or {}) do
            if practiceMode then
                for _,name in ipairs({"Target_10m","Target_15m","Target_20m","Target_25m_Left","Target_25m_Right","Target_35m"}) do
                    local e=Scene.FindEntity(name)
                    if e:IsValid() and e.id==pellet.entityID then practiceTarget=true break end
                end
            elseif remotePlayersByEntity[pellet.entityID] then enemyHit=true end
        end
        if practiceTarget or enemyHit then
            if practiceTarget then practiceHits=practiceHits+1 end
            hitmarkerTimer=.12
            Audio.PlaySFX("Assets/Audio/Breakbulk/hit.wav",.7)
        end
    end
end

local function consumeWeaponPickup()
    local request=math.floor(State.GetNumber("duelfps_weapon_pickup",0))
    if request==0 or not weapons then return end
    State.SetNumber("duelfps_weapon_pickup",0)
    if request==1 then
        weapons:Give("pistol",practiceMode and Pistol.practiceReserve or Pistol.startingReserve)
    elseif request==2 then
        weapons:Give("rifle",practiceMode and Rifle.practiceReserve or Rifle.startingReserve)
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
    if localHealth<previousHealth then damageTimer=.25 end
    previousHealth=localHealth
    UI.SetVisible("DamageFlash",damageTimer>0)
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
    local ammo,reserve=0,0
    if weapons then ammo,reserve=weapons:GetAmmo() end
    UI.SetText("RoundClock",practiceMode and "DRILL" or string.format("%02d:%02d",math.floor(stateTimer/60),math.ceil(stateTimer)%60))
    local weaponDef=weapons and weapons:GetDefinition() or nil
    local equipped=weaponDef and weaponDef.id or ""
    local primary=gunDefs[loadoutPrimary]
    local secondary=gunDefs[loadoutSecondary]
    UI.SetText("AmmoLabel",weaponDef and weaponDef.displayName or "UNARMED")
    UI.SetText("AmmoText",weaponDef and (tostring(ammo).."  /  "..tostring(reserve)) or "--  /  --")
    UI.SetText("Slot1Text",(equipped==loadoutPrimary and "> " or "").."1  "..(primary.shortName or loadoutPrimary:upper()))
    UI.SetText("Slot2Text",(equipped==loadoutSecondary and "> " or "").."2  "..(secondary.shortName or loadoutSecondary:upper()))
    UI.SetColor("Slot1Plate",equipped==loadoutPrimary and 0.30 or 0.08,equipped==loadoutPrimary and 0.11 or 0.08,equipped==loadoutPrimary and 0.025 or 0.09,0.96)
    UI.SetColor("Slot2Plate",equipped==loadoutSecondary and 0.30 or 0.08,equipped==loadoutSecondary and 0.11 or 0.08,equipped==loadoutSecondary and 0.025 or 0.09,0.96)
    UI.SetColor("Slot1Text",1,0.86,0.68,1)
    UI.SetColor("Slot2Text",1,0.86,0.68,1)
    UI.SetVisible("ReloadText",weapons and weapons:IsReloading() or false)
    if weapons and weapons:IsReloading() then UI.SetText("ReloadText","RELOADING") end
    UI.SetVisible("Hitmarker",hitmarkerTimer>0)
    -- Muzzle flash is a 3D world effect spawned by WeaponVFX, not a HUD widget.

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
        UI.SetText("RoundResultTitle",roundWinner==0 and "STALEMATE" or (roundWinner==localID and "ROUND SECURED" or "ROUND LOST"))
        UI.SetText("RoundResultScore",scoreLine)
        UI.SetText("RoundResultNext","NEXT ROUND IN "..math.max(1,math.ceil(stateTimer)))
    elseif showMatchResult then
        UI.SetText("MatchStatus","MATCH COMPLETE")
        UI.SetText("CenterMessage","")
        UI.SetText("MatchResultTitle",matchWinner==localID and "VICTORY" or "DEFEAT")
        UI.SetText("MatchResultScore",scoreLine)
        UI.SetText("MatchResultHint",matchWinner==localID and "MATCH WON" or "MATCH LOST")
        UI.SetText("RematchLabel",rematchVotes[localID] and "WAITING FOR RIVAL" or "REQUEST REMATCH")
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
    weapons:Register(Rifle)
    weapons:Register(Shotgun)
    weapons:Register(SMG)
    local primary=Preferences.LoadString("breakbulk_primary","rifle")
    local secondary=Preferences.LoadString("breakbulk_secondary","pistol")
    loadoutPrimary=gunDefs[primary] and primary or "rifle"
    loadoutSecondary=gunDefs[secondary] and secondary or "pistol"
    if loadoutPrimary==loadoutSecondary then loadoutSecondary=loadoutPrimary=="pistol" and "rifle" or "pistol" end
    weapons:SetLoadout(loadoutPrimary,loadoutSecondary,practiceMode)
    playerLoadouts[Controller.GetLocalID()]={loadoutPrimary,loadoutSecondary}
    loadoutSent=false
    State.SetNumber("duelfps_weapon_pickup",0)
    Input.SetCursorVisible(false)
    if practiceMode then
        possessedControllerID=1
        matchState=ROUND_ACTIVE
        health[1]=MAX_HEALTH
        UI.SetText("MatchStatus","PRACTICE RANGE // TARGET DRILL")
        UI.SetText("CenterMessage","")
    else
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
    if matchState~=MATCH_END then return end
    rematchVotes[Controller.GetLocalID()]=true
    if Network.IsHost() then
        if rematchVotes[2] then Input.SetCursorVisible(false);beginMatch() end
    else Network.SendMessage(CHANNEL_COMBAT,"REMATCH") end
end

function OnReturnToMenuClicked()
    if matchState==MATCH_END then returnToMenu() end
end

function OnUpdate(dt)
    elapsed=elapsed+dt
    damageTimer=math.max(0,damageTimer-dt)
    for sequence,pending in pairs(pendingShots) do
        pending.age=pending.age+dt;pending.retry=pending.retry+dt
        if pending.age>1 then pendingShots[sequence]=nil
        elseif pending.retry>.08 then pending.retry=0;Network.SendMessage(CHANNEL_COMBAT,pending.payload) end
    end
    if not Network.IsHost() and rematchVotes[2] and matchState==MATCH_END then
        rematchRetry=rematchRetry+dt
        if rematchRetry>.3 then rematchRetry=0;Network.SendMessage(CHANNEL_COMBAT,"REMATCH") end
    end
    if not practiceMode and not Network.IsConnected() then
        State.SetBool("breakbulk_disconnected",true)
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
    if not practiceMode and Network.IsReady() and not loadoutSent then
        loadoutSent=true
        playerLoadouts[Controller.GetLocalID()]={loadoutPrimary,loadoutSecondary}
        if not Network.IsHost() then
            Network.SendMessage(CHANNEL_COMBAT,"LOADOUT:"..loadoutPrimary..":"..loadoutSecondary)
        end
    end

    if not practiceMode and not Controller.IsLocallyControlled(self.id) then return end

    consumeWeaponPickup()
    if weapons then weapons:Update(dt,playerCamera) end
    updateHUD()
    if practiceMode then
        UI.SetText("MatchStatus","PRACTICE RANGE // HITS "..practiceHits)
        UI.SetText("ScoreText","TARGET HITS  "..practiceHits)
        local prompt=Scene.GetInteractionPrompt()
        UI.SetText("CenterMessage",prompt~="" and ("[E]  "..string.upper(prompt)) or "")
        UI.SetVisible("CenterMessage",prompt~="")
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
    local sprint=Input.IsKeyDown("Left Shift")
    local speed=weapons and weapons.aiming and 2.7 or (sprint and sprintSpeed or walkSpeed)
    stepTimer=stepTimer-dt
    if length>0 and stepTimer<=0 and CharacterController.IsGrounded() then
        Audio.PlayFootstep(practiceMode and "concrete" or "metal",sprint)
        stepTimer=sprint and .29 or .43
    end
    if length>0 then mx,mz=mx/length*speed,mz/length*speed end
    CharacterController.Move(mx,mz)
    if Input.IsKeyPressed("Space") then CharacterController.Jump() end
    if Input.IsKeyPressed("1") and weapons then weapons:Equip(loadoutPrimary) end
    if Input.IsKeyPressed("2") and weapons then weapons:Equip(loadoutSecondary) end
    if Input.IsKeyPressed("R") and weapons and weapons:Reload() then Audio.PlaySFX("Assets/Audio/Breakbulk/reload.wav",.5) end
    if Input.IsMouseButtonDown(1) and weapons then
        handleWeaponShot(weapons:Fire(self.id,playerCamera))
    end
end
