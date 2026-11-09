local Settings=require("Scripts.Settings")
local settings
local Profiles = require("Scripts.Weapons.LoadoutProfiles")
local WeaponSystem = require("Scripts.Weapons.WeaponSystem")
local Pistol = require("Scripts.Weapons.Pistol")
local Rifle = require("Scripts.Weapons.Rifle")
local Shotgun = require("Scripts.Weapons.Shotgun")
local SMG = require("Scripts.Weapons.SMG")
local Sniper=require("Scripts.Weapons.Sniper")
local Marksman=require("Scripts.Weapons.Marksman")
local ShotPattern = require("Scripts.Weapons.ShotPattern")
local gunDefs={pistol=Pistol,rifle=Rifle,shotgun=Shotgun,smg=SMG,sniper=Sniper,marksman=Marksman}
local loadoutPrimary,loadoutSecondary="rifle","pistol"
local loadoutSent=false
local cloudCode,loadoutRetry=0,0
local teamSize=1
local spawnOffsets={0,-2,2}
local function teamOf(slot) return slot%2==1 and 1 or 2 end
local loadoutMenuOpen=false
local pendingArmoryAction=nil
local editingSlot="primary"
local editingProfile=1
local fieldDraft={}
local editingPrimary,editingSecondary="rifle","pistol"
local editingCategory="rifle"
local armoryStats={
    rifle={damage=7,range=8,control=7,mobility=6},
    smg={damage=5,range=5,control=6,mobility=9},
    shotgun={damage=10,range=3,control=4,mobility=5},
    pistol={damage=7,range=4,control=8,mobility=10}
}
armoryStats.sniper={damage=10,range=10,control=3,mobility=3}
armoryStats.marksman={damage=8,range=9,control=6,mobility=5}
for _,v in ipairs(require("Scripts.Weapons.Variants").entries) do
    gunDefs[v.id]=require("Scripts.Weapons."..v.module)
    armoryStats[v.id]=armoryStats[v.base]
end
local playerLoadouts={}

local crouched=false
local depot=false
local history={}
local hostClock=nil
local lastAck={}
local walkSpeed, sprintSpeed = 5.0, 8.0
local sensitivity = 0.01
local paused, sendTimer, stateBroadcastTimer = false, 0.0, 0.0
local possessedControllerID = 0
local remotePawns, remotePlayersByEntity, remoteTargets = {}, {}, {}
local REMOTE_INTERPOLATION_SPEED = 20.0
local stridePhase=0
local remoteKick,aimPose=0,0

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
    local side=teamOf(playerID)
    local start=Controller.GetPlayerStart(side)
    if not start.valid then return end
    local offset=spawnOffsets[math.ceil(playerID/2)] or 0
    start.x=start.x+offset
    if playerID==Controller.GetLocalID() then
        transform.SetPosition(start.x,start.y,start.z)
        CharacterController.Move(0.0,0.0)
        Scene.SetRotation(self.id,0,0,0)
        if playerCamera~=0 then Scene.SetRotation(playerCamera,0,side==1 and -90 or 90,0) end
    else
        local entityID=remotePawns[playerID]
        if entityID and entityID~=0 then
            Scene.SetPosition(entityID,start.x,start.y,start.z)
            remoteTargets[playerID]=nil
        end
    end
end

local function spawnRoundPlayers()
    for slot=1,2*teamSize do movePlayerToStart(slot) end
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
    if teamSize>1 then
        local armor=Scene.FindChild(entityID,'Operator armor')
        if armor and armor~=0 then
            local ally=teamOf(playerID)==teamOf(Controller.GetLocalID())
            for _,name in ipairs({'Upper body','Left leg','Right leg'}) do
                local part=Scene.FindChild(armor,name)
                if part and part~=0 then Scene.SetTint(part,ally and .18 or 1,ally and .78 or .28,ally and 1 or .14) end
            end
        end
    end
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
        roundWinner..":"..matchWinner..":"..math.floor(elapsed*1000))
    Network.SendMessage(21,string.format('%d:%d:%d',depot and 2 or 1,ROUNDS_TO_WIN,ROUND_DURATION))
end

local function beginWarmup()
    if not Network.IsHost() then return end
    roundNumber=roundNumber+1
    history={};lastAck={};pendingShots={};lastShotTime={};rematchVotes={[1]=false,[2]=false}
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
    local previousState,previousRound=matchState,roundNumber
    matchState,stateTimer,roundNumber=newState,newTimer,newRound
    health[1],health[2]=h1,h2
    score[1],score[2]=s1,s2
    roundWinner,matchWinner=newRoundWinner,newMatchWinner
    if (previousState~=matchState and matchState==WARMUP) or previousRound~=roundNumber then
        spawnRoundPlayers();pendingShots={};rematchVotes={[1]=false,[2]=false}
        if weapons then weapons:ResetAmmo(false) end
        Input.SetCursorVisible(false)
    end
end

local function validateShot(shooterID,sequence,round,weaponID,ox,oy,oz,dx,dy,dz,stamp)
    if not Network.IsHost() or matchState~=ROUND_ACTIVE or round~=roundNumber then return end
    if shooterID~=1 and shooterID~=2 then return end
    if sequence<=(lastShotSequence[shooterID] or 0) then return end
    lastShotSequence[shooterID]=sequence
    local def=gunDefs[weaponID]
    local equipped=playerLoadouts[shooterID]
    if not def or not equipped or (weaponID~=equipped[1] and weaponID~=equipped[2]) then return false end
    local shotTime=stamp and math.max(elapsed-.18,math.min(elapsed,stamp)) or elapsed
    if shotTime-(lastShotTime[shooterID] or -100)<def.fireInterval*.8 then return false end
    if (health[shooterID] or 0)<=0 then return end
    local shooter=shooterID==Controller.GetLocalID() and self.id or remotePawns[shooterID]
    if not shooter then return end
    local p=Scene.GetPosition(shooter)
    if (ox-p.x)^2+(oy-p.y)^2+(oz-p.z)^2>9 then return end
    -- A plausible displacement during transit must not let the shot originate
    -- on the other side of solid cover.
    local low=(shooterID==Controller.GetLocalID() and crouched) or (remoteTargets[shooterID] and remoteTargets[shooterID].rz>.5)
    local eyeY=p.y+(low and .40 or .65)
    local ex,ey,ez=ox-p.x,oy-eyeY,oz-p.z;local displacement=math.sqrt(ex*ex+ey*ey+ez*ez)
    if displacement>.15 then
        local obstruction=Physics.Raycast(p.x,eyeY,p.z,ex/displacement,ey/displacement,ez/displacement,displacement-.02,shooter)
        if obstruction.hit then return false end
    end
    local length=dx*dx+dy*dy+dz*dz
    if length<.9 or length>1.1 then return end
    lastShotTime[shooterID]=shotTime
    -- Rewind only the host player for a remote shot. Cover stays authoritative.
    local restored=nil
    if shooterID~=Controller.GetLocalID() and stamp and #history>0 then
        local at=math.max(elapsed-.18,math.min(elapsed,stamp))
        local frame=history[1];for _,v in ipairs(history) do if v.time<=at then frame=v end end
        restored=transform.GetPosition();transform.SetPosition(frame.x,frame.y,frame.z)
        if Scene.SetReplicaHeight then Scene.SetReplicaHeight(self.id,frame.height) end
    end
    local damageByTarget={}
    for _,dir in ipairs(ShotPattern.Directions(dx,dy,dz,def.pellets or 1,def.spread or 0)) do
        local hit=Physics.Raycast(ox,oy,oz,dir.x,dir.y,dir.z,def.range,shooter)
        local target=hit.entityID==self.id and Controller.GetLocalID() or remotePlayersByEntity[hit.entityID]
        if hit.hit and target and target~=shooterID then
            damageByTarget[target]=(damageByTarget[target] or 0)+def.damage
        end
    end
    if restored then transform.SetPosition(restored.x,restored.y,restored.z);if Scene.SetReplicaHeight then Scene.SetReplicaHeight(self.id,crouched and 1.16 or 1.8) end end
    for target,damage in pairs(damageByTarget) do applyHostShot(shooterID,target,damage) end
    return true,next(damageByTarget)~=nil
end

local function confirmedHit() hitmarkerTimer=.16;Audio.PlaySFX("Assets/Audio/Breakbulk/hit.wav",.7) end

local function processCombatMessages()
    for _,message in ipairs(Network.ConsumeMessages()) do
        if message.channel==CHANNEL_COMBAT then
            local parts={}
            for token in message.payload:gmatch("[^:]+") do parts[#parts+1]=token end
            if parts[1]=="SHOT" and Network.IsHost() and (#parts==10 or #parts==11) then
                local numbers={};local valid=true
                for _,i in ipairs({2,3,5,6,7,8,9,10}) do
                    numbers[i]=tonumber(parts[i]);if not numbers[i] or numbers[i]~=numbers[i] or math.abs(numbers[i])>1000000 then valid=false end
                end
                if valid then
                    local ack=lastAck[numbers[2]]
                    if ack then Network.SendMessage(CHANNEL_COMBAT,ack) else
                    local accepted,hit=validateShot(message.senderID,numbers[2],numbers[3],parts[4],numbers[5],numbers[6],numbers[7],numbers[8],numbers[9],numbers[10],tonumber(parts[11]))
                    if accepted then
                        remoteKick=4
                        weapons:SpawnRemoteShot(remotePawns[message.senderID] or 0,parts[4],numbers[5],numbers[6],numbers[7],numbers[8],numbers[9],numbers[10])
                        Network.SendMessage(CHANNEL_COMBAT,"FX:"..message.senderID..":"..parts[4]..":"..
                            parts[5]..":"..parts[6]..":"..parts[7]..":"..parts[8]..":"..parts[9]..":"..parts[10])
                        local reply="ACK:"..message.senderID..":"..numbers[2]..":"..(hit and "1" or "0")
                        lastAck[numbers[2]]=reply;Network.SendMessage(CHANNEL_COMBAT,reply)
                    end
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
                        remoteKick=4
                    end
                end
            elseif parts[1]=="ACK" and not Network.IsHost() and message.senderID==1 then
                if tonumber(parts[2])==Controller.GetLocalID() and pendingShots[tonumber(parts[3])] then pendingShots[tonumber(parts[3])]=nil;if parts[4]=="1" then confirmedHit() end end
            elseif parts[1]=="STATE" and not Network.IsHost() and message.senderID==1 and
                    (#parts==10+2*teamSize or (teamSize==1 and #parts==11)) then
                local v={};local valid=true
                for i=2,11 do v[i]=tonumber(parts[i]);if not v[i] then valid=false end end
                if valid and v[2]>lastStateSequence then
                    for slot=3,2*teamSize do health[slot]=tonumber(parts[10+slot]) end
                    lastStateSequence=v[2];hostClock=(tonumber(parts[12]) or 0)/1000
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
            Network.SendTransform(p.x,p.y,p.z,math.deg(math.asin(math.max(-1,math.min(1,forward.y)))),bodyYaw,crouched and 1 or 0)
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
            if teamSize>1 and health[playerID]==0 then
                Scene.SetPosition(entityID,p.x,-100,p.z)
            else
                Scene.SetPosition(entityID,
                    p.x+(target.x-p.x)*alpha,
                    p.y+(target.y-p.y)*alpha,
                    p.z+(target.z-p.z)*alpha)
            end
            Scene.SetRotation(entityID,0,target.ry,0)
            local low=target.rz>.5
            if Scene.SetReplicaHeight then Scene.SetReplicaHeight(entityID,low and 1.16 or 1.8) end
            local mesh=Scene.FindChild(entityID,"Operator armor")
            if mesh and mesh~=0 then Scene.SetScale(mesh,1,low and .645 or 1,1) end
            local travel=math.sqrt((target.x-p.x)^2+(target.z-p.z)^2)*alpha
            stridePhase=stridePhase+math.min(travel,.25)*6
            local swing=math.sin(stridePhase)*math.min(24,travel/math.max(dt,.001)*4)
            if mesh and mesh~=0 then
                local torso=Scene.FindChild(mesh,'Upper body')
                aimPose=aimPose+(math.max(-50,math.min(50,target.rx))-aimPose)*(1-math.exp(-dt*15));remoteKick=remoteKick*math.exp(-dt*16)
                if torso and torso~=0 then
                    Scene.SetRotation(torso,aimPose*.7+remoteKick+math.sin(elapsed*2)*.55,0,swing*.09)
                end
                for i,name in ipairs({'Left leg','Right leg'}) do local leg=Scene.FindChild(mesh,name)
                    if leg and leg~=0 then Scene.SetRotation(leg,(i==1 and swing or -swing),0,0) end
                end
            end
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
            local accepted,hit=validateShot(Controller.GetLocalID(),shotSequence,roundNumber,def.id,result.originX,result.originY,result.originZ,result.forwardX,result.forwardY,result.forwardZ)
            if hit then confirmedHit() end
            if accepted then
                Network.SendMessage(CHANNEL_COMBAT,"FX:"..Controller.GetLocalID()..":"..def.id..":"..
                    string.format("%.3f:%.3f:%.3f:%.4f:%.4f:%.4f",result.originX,result.originY,result.originZ,result.forwardX,result.forwardY,result.forwardZ))
            end
        else if hostClock then payload=payload..":"..string.format("%.3f",hostClock-.055) end;Network.SendMessage(CHANNEL_COMBAT,payload);pendingShots[shotSequence]={payload=payload,age=0,retry=0} end
    end
    if practiceMode and result.hit then
        local practiceTarget=false
        for _,pellet in ipairs(result.hits or {}) do
            for _,name in ipairs({"Target_10m","Target_15m","Target_20m","Target_25m_Left","Target_25m_Right","Target_35m"}) do
                local e=Scene.FindEntity(name)
                if e:IsValid() and e.id==pellet.entityID then practiceTarget=true break end
            end
        end
        if practiceTarget then
            practiceHits=practiceHits+1
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
    local side=teamOf(localID)
    local localHealth=health[localID] or MAX_HEALTH
    if localHealth<previousHealth then damageTimer=.25 end
    previousHealth=localHealth
    UI.SetVisible("DamageFlash",damageTimer>0)
    local dead=localHealth<=0
    local localScore=score[side] or 0
    local enemyScore=score[3-side] or 0
    local scoreLine=(teamSize>1 and "TEAM " or "YOU ")..localScore.."  —  "..enemyScore.." ENEMY"

    State.SetNumber("duel_health",localHealth)
    State.SetNumber("duel_score",localScore)
    State.SetNumber("duel_opponent_score",enemyScore)
    UI.SetValue("HealthBar",math.max(0.0,math.min(1.0,localHealth/MAX_HEALTH)))
    local f=Camera.GetForward();local bearing=math.floor((math.deg(math.atan(f.x,-f.z))+360)%360+.5)%360
    local cardinal=({"N","NE","E","SE","S","SW","W","NW"})[math.floor((bearing+22.5)/45)%8+1]
    UI.SetText("Compass",string.format("%s   %03d",cardinal,bearing))
    UI.SetVisible("RadarTerminal",not depot);UI.SetVisible("RadarDepot",depot)
    if UI.SetPosition then local p=transform.GetPosition();UI.SetPosition("RadarPlayer",151+p.x*4.375,149+p.z*4.375) end
    UI.SetText("Stance",crouched and "CROUCHED" or "STANDING")
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
    UI.SetText("Slot1Text",(equipped==loadoutPrimary and "> " or "")..settings:binding("primary").."  "..(primary.shortName or loadoutPrimary:upper()))
    UI.SetText("Slot2Text",(equipped==loadoutSecondary and "> " or "")..settings:binding("secondary").."  "..(secondary.shortName or loadoutSecondary:upper()))
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
    local scoped=weapons and weapons:IsScoped() and matchState==ROUND_ACTIVE and not dead and not paused
    UI.SetVisible("ScopeOverlay",scoped)
    UI.SetText("ScopeZoom",weapons and weapons:GetDefinition() and (weapons:GetDefinition().scopeZoom or "") or "")
    UI.SetVisible("CrosshairH",matchState==ROUND_ACTIVE and not dead and not scoped)
    UI.SetVisible("CrosshairV",matchState==ROUND_ACTIVE and not dead and not scoped)
    UI.SetVisible("MatchActions",showMatchResult)

    if matchState==WAITING then
        UI.SetText("MatchStatus",teamSize>1 and ("WAITING FOR %dv%d"):format(teamSize,teamSize) or "WAITING FOR OPPONENT")
        UI.SetText("CenterMessage",cloudCode>0 and not State.GetBool('duel_public_match',false)
            and ("LOBBY CODE  %08d"):format(cloudCode) or "WAITING FOR OPPONENT")
    elseif showWarmup then
        local countdown=math.max(1,math.ceil(stateTimer))
        UI.SetText("MatchStatus","ROUND "..roundNumber.." // FIRST TO "..ROUNDS_TO_WIN)
        UI.SetText("CenterMessage",teamSize>1 and "Allies in blue / enemies in red" or "")
        UI.SetText("RoundIntroEyebrow","ROUND "..roundNumber)
        UI.SetText("RoundIntroTitle",tostring(countdown))
        UI.SetText("RoundIntroHint","GET READY")
    elseif matchState==ROUND_ACTIVE then
        if teamSize>1 then
            local alive={0,0}
            for slot=1,teamSize*2 do
                local team=teamOf(slot)
                if (health[slot] or 0)>0 then alive[team]=alive[team]+1 end
            end
            UI.SetText("MatchStatus",("ROUND %d  /  ALIVE  %d vs %d"):format(roundNumber,alive[side],alive[3-side]))
        else UI.SetText("MatchStatus","ROUND "..roundNumber.." // FIRST TO "..ROUNDS_TO_WIN) end
        UI.SetText("CenterMessage",dead and "ELIMINATED — WAIT FOR NEXT ROUND" or "")
        if activeIntroTimer>0 then
            UI.SetText("RoundIntroEyebrow","ROUND "..roundNumber)
            UI.SetText("RoundIntroTitle","FIGHT")
            UI.SetText("RoundIntroHint","")
        end
    elseif showRoundResult then
        UI.SetText("MatchStatus","ROUND "..roundNumber.." COMPLETE")
        UI.SetText("CenterMessage","")
        UI.SetText("RoundResultTitle",roundWinner==0 and "STALEMATE" or (roundWinner==side and "ROUND SECURED" or "ROUND LOST"))
        UI.SetText("RoundResultScore",scoreLine)
        UI.SetText("RoundResultNext","NEXT ROUND IN "..math.max(1,math.ceil(stateTimer)))
    elseif showMatchResult then
        UI.SetText("MatchStatus","MATCH COMPLETE")
        UI.SetText("CenterMessage","")
        UI.SetText("MatchResultTitle",matchWinner==side and "VICTORY" or "DEFEAT")
        UI.SetText("MatchResultScore",scoreLine)
        UI.SetText("MatchResultHint",matchWinner==side and "MATCH WON" or "MATCH LOST")
        UI.SetText("RematchLabel",rematchVotes[localID] and "WAITING FOR RIVAL" or "REQUEST REMATCH")
    end
end

function OnCreate()
    cloudCode=State.GetNumber('duel_cloud_code',0)
    teamSize=State.GetNumber('duel_team_size',1)
    ROUNDS_TO_WIN=State.GetNumber('duel_limit',5);ROUND_DURATION=State.GetNumber('duel_seconds',90)
    depot=Scene.FindEntity("Depot retaining wall"):IsValid()
    settings=Settings.new(_ENV,function() UI.Load("Assets/UI/Duel.ui");setPaused(true) end)
    settings.path="Assets/UI/SettingsInGame.ui"
    practiceMode=Scene.FindEntity("PracticeMode"):IsValid()
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
        Scene=Scene, Camera=Camera, Physics=Physics, Audio=Audio,
        GetBaseFOV=function()return settings.values.fov end,
        Input={IsMouseButtonDown=function(b)return not paused and not loadoutMenuOpen and not settings.open and settings:down(b==1 and 'fire' or 'aim')end,
               IsKeyDown=function(k)local actions={W='forward',S='back',A='left',D='right'};return actions[k] and settings:down(actions[k]) or false end}
    })
    weapons:Register(Pistol)
    weapons:Register(Rifle)
    weapons:Register(Shotgun)
    weapons:Register(SMG)
    weapons:Register(Sniper)
    weapons:Register(Marksman)
    for _,v in ipairs(require("Scripts.Weapons.Variants").entries) do weapons:Register(gunDefs[v.id]) end
    local _,primary,secondary=Profiles.GetActive(Preferences)
    loadoutPrimary,loadoutSecondary=primary,secondary
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

-- The field armory reuses the editor-authored menu UI, but callbacks run
-- in this Player script so it can return to the HUD without changing scenes.
local ArmoryView=require('Scripts.Weapons.ArmoryView')
local picking=false
local function refreshFieldArmory()
    ArmoryView.refresh(UI,editingProfile,editingPrimary,editingSecondary,editingSlot,editingCategory,picking)
end
function OnPickerBack() picking=false;refreshFieldArmory() end

local function openFieldArmory()
    picking=false
    if loadoutMenuOpen then return end
    loadoutMenuOpen=true
    fieldDraft={}
    paused=true
    editingProfile,editingPrimary,editingSecondary=Profiles.GetActive(Preferences)
    editingSlot="primary"
    editingCategory=editingPrimary
    CharacterController.Move(0,0)
    Input.SetCursorVisible(true)
    UI.Load("Assets/UI/LoadoutInGame.ui")
    refreshFieldArmory()
end

local function closeFieldArmory(apply)
    if not loadoutMenuOpen then return end
    if apply and Profiles.Apply(fieldDraft,editingProfile,editingPrimary,editingSecondary,Preferences) then
        local changed=loadoutPrimary~=editingPrimary or loadoutSecondary~=editingSecondary
        loadoutPrimary,loadoutSecondary=editingPrimary,editingSecondary
        -- Re-selecting an unchanged preset must not refill ammo mid-round.
        if changed then weapons:SetLoadout(loadoutPrimary,loadoutSecondary,practiceMode) end
        playerLoadouts[Controller.GetLocalID()]={loadoutPrimary,loadoutSecondary}
        loadoutSent=false -- resend validated IDs to the host
    end
    loadoutMenuOpen=false
    UI.Load("Assets/UI/Duel.ui")
    setPaused(false)
    Input.SetCursorVisible(false)
end

local function selectFieldProfile(index)
    editingPrimary,editingSecondary=Profiles.Browse(fieldDraft,editingProfile,editingPrimary,editingSecondary,index,Preferences)
    editingProfile=index;editingCategory=editingSlot=='primary' and editingPrimary or editingSecondary
    refreshFieldArmory()
end
local function selectFieldGun(id)
    picking=false
    if editingSlot=="primary" then
        if editingSecondary==id then editingSecondary=editingPrimary end
        editingPrimary=id
    else
        if editingPrimary==id then editingPrimary=editingSecondary end
        editingSecondary=id
    end
    editingCategory=id
    refreshFieldArmory()
end
function OnLoadoutClicked() pendingArmoryAction="open" end
function OnPrimarySlotClicked() picking=true;editingSlot="primary";editingCategory=editingPrimary;refreshFieldArmory() end
function OnSecondarySlotClicked() picking=true;editingSlot="secondary";editingCategory=editingSecondary;refreshFieldArmory() end
function OnProfile1Clicked() selectFieldProfile(1) end
function OnProfile2Clicked() selectFieldProfile(2) end
function OnProfile3Clicked() selectFieldProfile(3) end
function OnProfile4Clicked() selectFieldProfile(4) end
function OnProfile5Clicked() selectFieldProfile(5) end
function OnCategoryRifle() editingCategory="rifle";refreshFieldArmory() end
function OnCategorySMG() editingCategory="smg";refreshFieldArmory() end
function OnCategoryShotgun() editingCategory="shotgun";refreshFieldArmory() end
function OnCategoryPistol() editingCategory="pistol";refreshFieldArmory() end
function OnRifleSelected() selectFieldGun("rifle") end
function OnSMGSelected() selectFieldGun("smg") end
function OnShotgunSelected() selectFieldGun("shotgun") end
function OnPistolSelected() selectFieldGun("pistol") end
function OnLoadoutSaved() pendingArmoryAction="apply" end
function OnLoadoutBack() pendingArmoryAction="close" end

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
    -- Never replace the canvas inside a UI button callback: the input
    -- dispatcher may still be traversing that widget hierarchy.
    if pendingArmoryAction then
        local action=pendingArmoryAction
        pendingArmoryAction=nil
        if action=="open" then openFieldArmory()
        elseif action=="apply" then closeFieldArmory(true)
        elseif action=="close" then closeFieldArmory(false) end
    end
    elapsed=elapsed+dt
    damageTimer=math.max(0,damageTimer-dt)
    for sequence,pending in pairs(pendingShots) do
        pending.age=pending.age+dt;pending.retry=pending.retry+dt
        if pending.age>1 then pendingShots[sequence]=nil
        elseif pending.retry>.08 then pending.retry=0;Network.SendMessage(CHANNEL_COMBAT,pending.payload) end
    end
    if not Network.IsHost() and rematchVotes[Controller.GetLocalID()] and matchState==MATCH_END then
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
        if hostClock then hostClock=hostClock+dt end
        if Network.IsHost() then
            local p=transform.GetPosition();history[#history+1]={time=elapsed,x=p.x,y=p.y,z=p.z,height=crouched and 1.16 or 1.8}
            while #history>1 and history[2].time<elapsed-.22 do table.remove(history,1) end
        end
        processCombatMessages()
        updateHostMatch(dt)
    end
    activeIntroTimer=math.max(0,activeIntroTimer-dt)
    hitmarkerTimer=math.max(0,hitmarkerTimer-dt)
    if cloudCode>0 then loadoutRetry=loadoutRetry-dt end
    if not practiceMode and Network.IsReady() and (not loadoutSent or (cloudCode>0 and loadoutRetry<=0)) then
        loadoutRetry=1
        loadoutSent=true
        playerLoadouts[Controller.GetLocalID()]={loadoutPrimary,loadoutSecondary}
        if not Network.IsHost() then
            Network.SendMessage(CHANNEL_COMBAT,"LOADOUT:"..loadoutPrimary..":"..loadoutSecondary)
        end
    end

    if not practiceMode and not Controller.IsLocallyControlled(self.id) then return end

    if settings:tick(dt) then CharacterController.Move(0,0);return end
    consumeWeaponPickup()
    if weapons then weapons:Update(dt,playerCamera) end
    if loadoutMenuOpen then
        CharacterController.Move(0,0)
        if Input.IsKeyPressed("Escape") then closeFieldArmory(false) end
        return
    end
    if settings:pressed("loadout") then
        openFieldArmory()
        return
    end
    updateHUD()
    if practiceMode then
        UI.SetText("MatchStatus","PRACTICE RANGE // HITS "..practiceHits)
        UI.SetText("ScoreText","TARGET HITS  "..practiceHits)
        local prompt=Scene.GetInteractionPrompt()
        UI.SetText("CenterMessage",prompt~="" and ("["..settings:binding("interact").."]  "..string.upper(prompt)) or "")
        UI.SetVisible("CenterMessage",prompt~="")
        UI.SetVisible("RoundIntro",false)
        UI.SetVisible("RoundResult",false)
        UI.SetVisible("MatchResult",false)
        UI.SetVisible("DeathOverlay",false)
        UI.SetVisible("CrosshairH",not weapons:IsScoped())
        UI.SetVisible("CrosshairV",not weapons:IsScoped())
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
    sensitivity=settings.values.sensitivity*(weapons and weapons.aiming and settings.values.ads or 1)
    local invert=State.GetBool("invert_y",false) and 1.0 or -1.0
    if playerCamera~=0 then
        Camera.RotateEntity(playerCamera,Input.GetMouseDeltaX()*sensitivity,Input.GetMouseDeltaY()*sensitivity*invert)
    else
        Camera.Rotate(Input.GetMouseDeltaX()*sensitivity,Input.GetMouseDeltaY()*sensitivity*invert)
    end

    local ix,iz=0.0,0.0
    if settings:down("forward") then iz=iz+1 end
    if settings:down("back") then iz=iz-1 end
    if settings:down("right") then ix=ix+1 end
    if settings:down("left") then ix=ix-1 end
    local forward,right=Camera.GetForward(),Camera.GetRight()
    local mx,mz=right.x*ix+forward.x*iz,right.z*ix+forward.z*iz
    local length=math.sqrt(mx*mx+mz*mz)
    local want=settings:down("crouch")
    if want~=crouched and Scene.TryActorHeight and Scene.TryActorHeight(want and 1.16 or 1.8) then
        crouched=want
        Scene.SetPosition(playerCamera,0,crouched and .40 or .65,0)
    end
    local sprint=not crouched and settings:down("sprint")
    local speed=crouched and 1.85 or weapons and weapons.aiming and 2.7 or (sprint and sprintSpeed or walkSpeed)
    stepTimer=stepTimer-dt
    if length>0 and stepTimer<=0 and CharacterController.IsGrounded() then
        -- Read the floor beneath the character, not the game mode. Metal
        -- cargo decks ring differently from concrete and wooden cover.
        local surface="concrete"
        local p=transform.GetPosition()
        local ground=Physics.Raycast(p.x,p.y+.25,p.z,0,-1,0,2.2,self.id)
        if ground and ground.hit and ground.entityID and ground.entityID~=0 then
            local name=Scene.GetName(ground.entityID):lower()
            if name:find("metal") or name:find("container") or name:find("catwalk")
                or name:find("grate") or name:find("ramp") then
                surface="metal"
            elseif name:find("wood") or name:find("crate") or name:find("pallet") then
                surface="wood"
            end
        end
        Audio.PlayFootstep(surface,sprint)
        stepTimer=sprint and .275 or (weapons and weapons.aiming and .54 or .41)
    end
    if length>0 then mx,mz=mx/length*speed,mz/length*speed end
    CharacterController.Move(mx,mz)
    if not crouched and settings:pressed("jump") then CharacterController.Jump() end
    if settings:pressed("primary") and weapons then weapons:Equip(loadoutPrimary) end
    if settings:pressed("secondary") and weapons then weapons:Equip(loadoutSecondary) end
    if settings:pressed("reload") and weapons and weapons:Reload() then Audio.PlaySFX("Assets/Audio/Breakbulk/reload.wav",.5) end
    if settings:down("fire") and weapons then
        handleWeaponShot(weapons:Fire(self.id,playerCamera))
    end
end

function OnCategorySniper() editingCategory="sniper";refreshFieldArmory() end
function OnSniperSelected() selectFieldGun("sniper") end

function OnCategoryMarksman() editingCategory="marksman";refreshFieldArmory() end
function OnMarksmanSelected() selectFieldGun("marksman") end

_ENV["OnBrowse_rifle"]=function() editingCategory="rifle";refreshFieldArmory() end

_ENV["OnBrowse_smg"]=function() editingCategory="smg";refreshFieldArmory() end

_ENV["OnBrowse_shotgun"]=function() editingCategory="shotgun";refreshFieldArmory() end

_ENV["OnBrowse_pistol"]=function() editingCategory="pistol";refreshFieldArmory() end

_ENV["OnBrowse_sniper"]=function() editingCategory="sniper";refreshFieldArmory() end

_ENV["OnBrowse_marksman"]=function() editingCategory="marksman";refreshFieldArmory() end

_ENV["OnBrowse_rifle2"]=function() editingCategory="rifle2";refreshFieldArmory() end

_ENV["OnBrowse_smg2"]=function() editingCategory="smg2";refreshFieldArmory() end

_ENV["OnBrowse_shotgun2"]=function() editingCategory="shotgun2";refreshFieldArmory() end

_ENV["OnBrowse_pistol2"]=function() editingCategory="pistol2";refreshFieldArmory() end

_ENV["OnBrowse_sniper2"]=function() editingCategory="sniper2";refreshFieldArmory() end

_ENV["OnBrowse_marksman2"]=function() editingCategory="marksman2";refreshFieldArmory() end

function OnFalconSelected() selectFieldGun("rifle2") end

function OnWaspSelected() selectFieldGun("smg2") end

function OnHammerSelected() selectFieldGun("shotgun2") end

function OnViperSelected() selectFieldGun("pistol2") end

function OnLongbowSelected() selectFieldGun("sniper2") end

function OnScoutSelected() selectFieldGun("marksman2") end
