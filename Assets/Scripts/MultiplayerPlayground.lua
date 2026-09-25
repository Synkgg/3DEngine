local hudTimer = 0.0
local sendTimer = 0.0
local roundTime = 180.0
local redScore, blueScore = 0, 0
local orbX, orbY, orbZ = 0.0, 1.25, 0.0
local carrierID = 0
local winner = 0
local pickupRadius = 2.4
local scoreLimit = 5
local restartTimer = 0.0
local actionCooldown = 0.0
local paused = false
local aaSamples = {1,2,4,8}
local viewDistances = {220.0,500.0,1000.0}
local aaIndex, viewIndex = 1, 1
local pauseFog, pauseBloom = false, false

local function nearestIndex(values,current)
    local best,distance=1,math.abs(values[1]-current)
    for i=2,#values do local d=math.abs(values[i]-current);if d<distance then best,distance=i,d end end
    return best
end

local function refreshPauseSettings()
    aaIndex=nearestIndex(aaSamples,Graphics.GetAntiAliasingSamples())
    viewIndex=nearestIndex(viewDistances,Graphics.GetViewDistance())
    pauseFog=Graphics.GetFog();pauseBloom=Graphics.GetBloom()
    UI.SetText("PauseAAValue",aaSamples[aaIndex]==1 and "OFF" or aaSamples[aaIndex].."X")
    UI.SetText("PauseFogValue",pauseFog and "ENABLED" or "DISABLED")
    UI.SetText("PauseBloomValue",pauseBloom and "ENABLED" or "DISABLED")
    UI.SetText("PauseViewValue",tostring(math.floor(viewDistances[viewIndex])))
end

local function setPaused(value)
    paused=value
    Scene.SetPaused(value)
    UI.SetVisible("PauseMenu",value)
    Input.SetCursorVisible(value)
    if value then refreshPauseSettings() end
end

local function savePauseSettings()
    Graphics.SetAntiAliasingSamples(aaSamples[aaIndex])
    Graphics.SetFog(pauseFog)
    Graphics.SetBloom(pauseBloom)
    Graphics.SetViewDistance(viewDistances[viewIndex])
    Graphics.Save()
end

local function dist2(p, x, y, z)
    local dx,dy,dz=p.x-x,p.y-y,p.z-z
    return dx*dx+dy*dy+dz*dz
end

local function playerPosition(id)
    if id == Network.GetLocalPlayerID() then
        local player=Scene.FindEntity("Player")
        if player.id ~= 0 then local p=Scene.GetPosition(player.id); return {valid=true,x=p.x,y=p.y,z=p.z} end
    end
    return Network.GetRemotePlayerPosition(id)
end

local function resetCore()
    orbX,orbY,orbZ=0.0,1.25,0.0
    carrierID=0
end

local function resetRound()
    redScore,blueScore=0,0
    roundTime=180.0
    winner=0
    restartTimer=0.0
    resetCore()
end

local function pullState()
    local s=Network.GetGameState()
    redScore,blueScore=s.redScore,s.blueScore
    roundTime=s.roundSeconds
    orbX,orbY,orbZ=s.orbX,s.orbY,s.orbZ
    carrierID=s.carrierID
    winner=s.winner
end

local function processAction(playerID, action)
    if action == 1 and carrierID == 0 and winner == 0 then
        local p=playerPosition(playerID)
        if p.valid and dist2(p,orbX,orbY,orbZ)<=pickupRadius*pickupRadius then carrierID=playerID end
    elseif action == 2 and carrierID == playerID then
        carrierID=0
    end
end

local function updateHUD()
    local id=Network.GetLocalPlayerID()
    local role=Network.IsHost() and "HOST" or "CLIENT"
    UI.SetText("Status",role.."  //  PLAYERS "..tostring(Network.GetPlayerCount()).."  //  ID "..tostring(id))
    UI.SetText("Score","RED "..tostring(redScore).."   //   "..tostring(math.max(0,math.floor(roundTime))).."   //   BLUE "..tostring(blueScore))
    if winner ~= 0 then
        UI.SetText("Objective",(winner==1 and "RED" or "BLUE").." WINS // NEW ROUND STARTING")
    elseif carrierID == id then
        UI.SetText("Objective","STEP 3: YOU HAVE THE CORE // RUN TO A GLOWING END ZONE // E DROPS")
    elseif carrierID ~= 0 then
        UI.SetText("Objective","CORE TAKEN BY PLAYER "..tostring(carrierID).." // CHASE THEM OR WAIT FOR A DROP")
    else
        UI.SetText("Objective","STEP 1: FIND GOLD CORE AT CENTER // STEP 2: GET CLOSE + PRESS E")
    end
end

function OnCreate()
    UI.Load("Assets/UI/MultiplayerPlayground.ui")
    Input.SetCursorVisible(false)
    Scene.SetPaused(false)
    UI.SetVisible("PauseMenu",false)
    if Network.IsHost() then
        Network.SetCoreRushState(0,0,180,orbX,orbY,orbZ,0,0)
    end
end

function OnUpdate(dt)
    hudTimer=hudTimer+dt
    sendTimer=sendTimer+dt
    actionCooldown=math.max(0,actionCooldown-dt)

    if Input.IsKeyPressed("Escape") then
        setPaused(not paused)
        return
    end

    if paused then
        if UI.WasClicked("ResumeButton") then setPaused(false); return end
        if UI.WasClicked("PauseAAButton") then aaIndex=aaIndex%#aaSamples+1;savePauseSettings();refreshPauseSettings() end
        if UI.WasClicked("PauseFogButton") then pauseFog=not pauseFog;savePauseSettings();refreshPauseSettings() end
        if UI.WasClicked("PauseBloomButton") then pauseBloom=not pauseBloom;savePauseSettings();refreshPauseSettings() end
        if UI.WasClicked("PauseViewButton") then viewIndex=viewIndex%#viewDistances+1;savePauseSettings();refreshPauseSettings() end
        if UI.WasClicked("PauseMenuButton") then
            Scene.SetPaused(false);Network.Disconnect();Scene.Load("Assets/Scenes/MainMenu.scene");return
        end
        return
    end

    if Input.IsKeyPressed("E") and actionCooldown<=0 then
        actionCooldown=0.25
        if carrierID==Network.GetLocalPlayerID() then Network.SendGameAction(2) else Network.SendGameAction(1) end
    end

    if Network.IsHost() then
        if winner==0 then
            roundTime=math.max(0,roundTime-dt)
            for _,a in ipairs(Network.ConsumeGameActions()) do processAction(a.playerID,a.action) end

            if carrierID~=0 then
                local p=playerPosition(carrierID)
                if p.valid then
                    orbX,orbY,orbZ=p.x,p.y+1.35,p.z
                    if p.z < -23.0 then
                        redScore=redScore+1; resetCore()
                    elseif p.z > 23.0 then
                        blueScore=blueScore+1; resetCore()
                    end
                else
                    resetCore()
                end
            end

            if redScore>=scoreLimit then winner=1;restartTimer=6.0;resetCore()
            elseif blueScore>=scoreLimit then winner=2;restartTimer=6.0;resetCore()
            elseif roundTime<=0 then
                if redScore>blueScore then winner=1 elseif blueScore>redScore then winner=2 else roundTime=60.0 end
                if winner~=0 then restartTimer=6.0;resetCore() end
            end
        else
            restartTimer=restartTimer-dt
            if restartTimer<=0 then resetRound() end
        end

        if sendTimer>=0.05 then
            sendTimer=0
            Network.SetCoreRushState(redScore,blueScore,math.ceil(roundTime),orbX,orbY,orbZ,carrierID,winner)
        end
    else
        pullState()
    end

    local orb=Scene.FindEntity("CoreOrb")
    if orb.id ~= 0 then Scene.SetPosition(orb.id,orbX,orbY,orbZ) end

    if hudTimer>=0.1 then hudTimer=0;updateHUD() end

end
