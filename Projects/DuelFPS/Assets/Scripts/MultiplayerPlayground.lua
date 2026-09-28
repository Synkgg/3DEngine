local roundTime=300.0
local score1,score2=0,0
local hp1,hp2=100,100
local winner,started=0,0
local fireCooldown=0.0
local respawnSerial=0
local lastRespawnSerial=-1
local paused=false
local sendTimer=0.0
local scoreLimit=10

local function localID() return Network.GetLocalPlayerID() end
local function enemyID() return localID()==1 and 2 or 1 end
local function healthFor(id) return id==1 and hp1 or hp2 end
local function setHealth(id,v) if id==1 then hp1=v else hp2=v end end
local function scoreFor(id) return id==1 and score1 or score2 end
local function setScore(id,v) if id==1 then score1=v else score2=v end end

local function spawnLocal()
    local id=localID()
    if id==0 then return end
    local player=Scene.FindEntity("Player")
    if player.id==0 then return end
    local x=id==1 and -12.0 or 12.0
    local z=id==1 and -27.0 or 27.0
    Scene.SetPosition(player.id,x,2.0,z)
end

local function resetMatch()
    roundTime=300.0;score1=0;score2=0;hp1=100;hp2=100;winner=0;started=1
    respawnSerial=respawnSerial+1
    spawnLocal()
end

local function aimedAtEnemy()
    local p=Network.GetRemotePlayerPosition(enemyID())
    if not p.valid then return false end
    local c=Camera.GetPosition()
    local f=Camera.GetForward()
    local dx,dy,dz=p.x-c.x,(p.y+0.75)-c.y,p.z-c.z
    local d=math.sqrt(dx*dx+dy*dy+dz*dz)
    if d<0.01 or d>100.0 then return false end
    local dot=(dx*f.x+dy*f.y+dz*f.z)/d
    return dot>0.985
end

local function updateHUD()
    local id=localID()
    local myHP=healthFor(id)
    UI.SetText("Title","DUEL // WAREHOUSE 01")
    UI.SetText("Status","HEALTH "..tostring(math.max(0,math.floor(myHP))).."   //   PLAYER "..tostring(id).."   //   "..tostring(Network.GetPlayerCount()).." PLAYERS")
    UI.SetText("Score","P1 "..tostring(score1).."   //   "..tostring(math.max(0,math.ceil(roundTime))).." SEC   //   P2 "..tostring(score2))
    if winner~=0 then UI.SetText("Objective","PLAYER "..tostring(winner).." WINS // HOST CAN RESTART")
    elseif started==0 then UI.SetText("Objective",Network.IsHost() and "WAIT FOR PLAYER 2 // START WHEN READY" or "WAITING FOR HOST")
    else UI.SetText("Objective","FIRST TO "..tostring(scoreLimit).." KILLS // LEFT CLICK TO FIRE") end
end

function OnCreate()
    UI.Load("Assets/UI/MultiplayerPlayground.ui")
    UI.SetVisible("PauseMenu",false)
    UI.SetVisible("InteractPlate",false)
    UI.SetVisible("InteractPrompt",false)
    UI.SetVisible("RestartMatchButton",false)
    Input.SetCursorVisible(Network.IsHost())
    spawnLocal()
    if Network.IsHost() then
        Network.SetCoreRushState(0,0,300,100,100,0,0,0,0)
    end
    updateHUD()
end

function OnUpdate(dt)
    if Network.WasKickedByHost() then
        Network.Disconnect();Input.SetCursorVisible(true);Scene.Load("Assets/Scenes/MainMenu.scene");return
    end

    fireCooldown=math.max(0,fireCooldown-dt)
    sendTimer=sendTimer+dt

    if Input.IsKeyPressed("Escape") then
        paused=not paused;Scene.SetPaused(paused);UI.SetVisible("PauseMenu",paused);UI.SetVisible("PauseMain",paused);UI.SetVisible("PauseSettings",false);Input.SetCursorVisible(paused);return
    end
    if paused then
        if UI.WasClicked("ReturnMatchButton") then paused=false;Scene.SetPaused(false);UI.SetVisible("PauseMenu",false);Input.SetCursorVisible(false) end
        if UI.WasClicked("PauseMenuButton") then Scene.SetPaused(false);Network.Disconnect();Scene.Load("Assets/Scenes/MainMenu.scene") end
        return
    end

    if Network.IsHost() and started==0 and (UI.WasClicked("StartMatchButton") or Input.IsKeyPressed("Return")) then
        resetMatch();Input.SetCursorVisible(false)
    end
    if Network.IsHost() and winner~=0 and UI.WasClicked("RestartMatchButton") then
        resetMatch();Input.SetCursorVisible(false)
    end

    if started==1 and winner==0 and Network.GetPlayerCount()>=2 and Input.IsMouseButtonDown(1) and fireCooldown<=0 then
        fireCooldown=0.18
        if aimedAtEnemy() then Network.SendGameAction(10) end
    end

    if Network.IsHost() then
        if started==1 and winner==0 then
            roundTime=math.max(0,roundTime-dt)
            for _,a in ipairs(Network.ConsumeGameActions()) do
                if a.action==10 and (a.playerID==1 or a.playerID==2) then
                    local target=a.playerID==1 and 2 or 1
                    local newHP=healthFor(target)-25
                    if newHP<=0 then
                        setScore(a.playerID,scoreFor(a.playerID)+1)
                        setHealth(target,100)
                        respawnSerial=respawnSerial+1
                        if scoreFor(a.playerID)>=scoreLimit then winner=a.playerID;started=0 end
                    else setHealth(target,newHP) end
                end
            end
            if roundTime<=0 then
                if score1>score2 then winner=1 elseif score2>score1 then winner=2 else roundTime=60 end
                if winner~=0 then started=0 end
            end
        end
        if sendTimer>=0.05 then
            sendTimer=0
            Network.SetCoreRushState(score1,score2,math.ceil(roundTime),hp1,hp2,respawnSerial,enemyID(),winner,started)
        end
    else
        local s=Network.GetGameState()
        score1,score2=s.redScore,s.blueScore;roundTime=s.roundSeconds;hp1,hp2=s.orbX,s.orbY
        respawnSerial=math.floor(s.orbZ+0.5);winner=s.winner;started=s.matchStarted or 0
    end

    if respawnSerial~=lastRespawnSerial then
        lastRespawnSerial=respawnSerial
        if started==1 then spawnLocal() end
    end

    UI.SetVisible("StartMatchButton",Network.IsHost() and started==0 and winner==0)
    UI.SetVisible("RestartMatchButton",Network.IsHost() and winner~=0)
    if not paused then Input.SetCursorVisible(Network.IsHost() and started==0) end
    updateHUD()
end
