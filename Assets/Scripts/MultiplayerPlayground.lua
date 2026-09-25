local hudTimer = 0.0
local sendTimer = 0.0
local roundTime = 180.0
local redScore, blueScore = 0, 0
local orbX, orbY, orbZ = 0.0, 1.25, 0.0
local carrierID = 0
local winner = 0
local matchStarted = 0
local pickupRadius = 2.4
local scoreLimit = 5
local restartTimer = 0.0
local actionCooldown = 0.0
local paused = false
local aaSamples = {1,2,4,8}
local viewDistances = {220.0,500.0,1000.0}
local aaIndex, viewIndex = 1, 1
local pauseFog, pauseBloom = false, false
local settingsOpen = false
local fovValues={60,75,90,105}
local sensitivityValues={0.005,0.01,0.015,0.02}
local volumeValues={0.0,0.25,0.5,0.75,1.0}
local shadowValues={0,1,2}
local fovIndex,sensitivityIndex,masterIndex,sfxIndex,uiVolumeIndex,shadowIndex=2,2,5,5,5,3
local invertY,sprintToggle,cameraBob,showFPS=false,false,true,false
local fpsTimer,fpsFrames=0.0,0
local spawnedAtBase = false

local function teamFor(id)
    if id == 0 then return "WAITING" end
    return (id % 2 == 1) and "RED" or "BLUE"
end

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

local function loadExtraSettings()
    fovIndex=nearestIndex(fovValues,tonumber(Preferences.LoadString("fov","75")) or 75)
    sensitivityIndex=nearestIndex(sensitivityValues,tonumber(Preferences.LoadString("mouse_sensitivity","0.01")) or 0.01)
    masterIndex=nearestIndex(volumeValues,tonumber(Preferences.LoadString("master_volume","1")) or 1)
    sfxIndex=nearestIndex(volumeValues,tonumber(Preferences.LoadString("sfx_volume","1")) or 1)
    uiVolumeIndex=nearestIndex(volumeValues,tonumber(Preferences.LoadString("ui_volume","1")) or 1)
    shadowIndex=nearestIndex(shadowValues,tonumber(Preferences.LoadString("shadow_quality","2")) or 2)
    invertY=Preferences.LoadString("invert_y","0")=="1"; sprintToggle=Preferences.LoadString("sprint_toggle","0")=="1"
    cameraBob=Preferences.LoadString("camera_bob","1")=="1"; showFPS=Preferences.LoadString("show_fps","0")=="1"
end

local function applyExtraSettings()
    Graphics.SetFOV(fovValues[fovIndex]); Graphics.SetShadowQuality(shadowValues[shadowIndex])
    GameSettings.SetMouseSensitivity(sensitivityValues[sensitivityIndex]); GameSettings.SetInvertY(invertY)
    GameSettings.SetSprintToggle(sprintToggle); GameSettings.SetCameraBob(cameraBob); GameSettings.SetShowFPS(showFPS)
    Audio.SetMasterVolume(volumeValues[masterIndex]); Audio.SetSFXVolume(volumeValues[sfxIndex]); Audio.SetUIVolume(volumeValues[uiVolumeIndex])
end

local function saveExtraSettings()
    Preferences.SaveString("fov",tostring(fovValues[fovIndex])); Preferences.SaveString("mouse_sensitivity",tostring(sensitivityValues[sensitivityIndex]))
    Preferences.SaveString("master_volume",tostring(volumeValues[masterIndex])); Preferences.SaveString("sfx_volume",tostring(volumeValues[sfxIndex])); Preferences.SaveString("ui_volume",tostring(volumeValues[uiVolumeIndex]))
    Preferences.SaveString("shadow_quality",tostring(shadowValues[shadowIndex])); Preferences.SaveString("invert_y",invertY and "1" or "0"); Preferences.SaveString("sprint_toggle",sprintToggle and "1" or "0")
    Preferences.SaveString("camera_bob",cameraBob and "1" or "0"); Preferences.SaveString("show_fps",showFPS and "1" or "0"); Graphics.Save()
end

local function refreshExtraSettings()
    local shadowNames={"OFF","LOW","HIGH"}
    UI.SetText("ShadowValue","SHADOWS: "..shadowNames[shadowIndex]); UI.SetText("FOVValue","FOV: "..tostring(fovValues[fovIndex]))
    UI.SetText("SensitivityValue","SENSITIVITY: "..string.format("%.1fX",sensitivityValues[sensitivityIndex]/0.01))
    UI.SetText("InvertValue","INVERT Y: "..(invertY and "ON" or "OFF")); UI.SetText("SprintModeValue","SPRINT: "..(sprintToggle and "TOGGLE" or "HOLD"))
    UI.SetText("CameraBobValue","CAMERA BOB: "..(cameraBob and "ON" or "OFF")); UI.SetText("MasterVolumeValue","MASTER: "..math.floor(volumeValues[masterIndex]*100).."%")
    UI.SetText("SFXVolumeValue","SFX: "..math.floor(volumeValues[sfxIndex]*100).."%"); UI.SetText("UIVolumeValue","UI VOLUME: "..math.floor(volumeValues[uiVolumeIndex]*100).."%")
    UI.SetValue("MasterVolumeSlider",volumeValues[masterIndex]); UI.SetValue("SFXVolumeSlider",volumeValues[sfxIndex]); UI.SetValue("UIVolumeSlider",volumeValues[uiVolumeIndex])
    UI.SetText("ShowFPSValue","SHOW FPS: "..(showFPS and "ON" or "OFF")); UI.SetVisible("FPSCounter",showFPS)
end

local function setPaused(value)
    paused=value
    settingsOpen=false
    Scene.SetPaused(value)
    UI.SetVisible("PauseMenu",value)
    UI.SetVisible("PauseMain",value)
    UI.SetVisible("PauseSettings",false)
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

local function randomCorePosition()
    -- Keep the core inside a 75-unit radius around midfield.
    local angle=math.random()*math.pi*2.0
    local radius=math.sqrt(math.random())*75.0
    return math.cos(angle)*radius,0.65,math.sin(angle)*radius
end

local function resetCore()
    orbX,orbY,orbZ=randomCorePosition()
    carrierID=0
end

local function moveLocalPlayerToBase()
    local id=Network.GetLocalPlayerID()
    if id==0 then return end
    local player=Scene.FindEntity("Player")
    if player.id==0 then return end
    local z=(teamFor(id)=="RED") and -30.0 or 30.0
    Scene.SetPosition(player.id,0.0,2.0,z)
end

local function resetRound()
    redScore,blueScore=0,0
    roundTime=180.0
    winner=0
    matchStarted=1
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
    matchStarted=s.matchStarted or 0
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
    local team=teamFor(id)
    UI.SetText("Status","YOU ARE "..team.." TEAM  //  YOUR BASE: "..team.."  //  PLAYER "..tostring(id).."  //  "..tostring(Network.GetPlayerCount()).." PLAYERS")
    UI.SetText("Score","RED "..tostring(redScore).."   //   "..tostring(math.max(0,math.floor(roundTime))).." SEC   //   BLUE "..tostring(blueScore))

    local p=playerPosition(id)
    local nearCore=p.valid and carrierID==0 and dist2(p,orbX,orbY,orbZ)<=pickupRadius*pickupRadius
    local showInteraction=nearCore or carrierID==id
    UI.SetVisible("InteractPrompt",showInteraction)
    UI.SetVisible("InteractPlate",showInteraction)

    if carrierID==id then
        UI.SetText("InteractPrompt","[ E ]  DROP CORE")
    elseif nearCore then
        UI.SetText("InteractPrompt","[ E ]  PICK UP CORE")
    end

    if winner ~= 0 then
        UI.SetText("Objective",(winner==1 and "RED" or "BLUE").." TEAM WINS!")
    elseif carrierID == id then
        local target=(team=="RED") and "RED BASE" or "BLUE BASE"
        UI.SetText("Objective","YOU HAVE THE CORE -> RETURN TO YOUR "..target.." TO SCORE")
    elseif matchStarted==0 then
        UI.SetText("Objective",Network.IsHost() and "WAIT FOR YOUR FRIEND // PRESS START MATCH WHEN READY" or "WAITING FOR HOST TO START THE MATCH")
    elseif carrierID ~= 0 then
        UI.SetText("Objective","PLAYER "..tostring(carrierID).." HAS THE CORE // STOP THEM")
    else
        UI.SetText("Objective","FIND THE GOLD CORE // IT SPAWNS RANDOMLY AROUND MIDFIELD // PRESS E")
    end
end

function OnCreate()
    UI.Load("Assets/UI/MultiplayerPlayground.ui")
    Input.SetCursorVisible(false)
    Scene.SetPaused(false)
    UI.SetVisible("PauseMenu",false)
    UI.SetVisible("InteractPrompt",false)
    UI.SetVisible("InteractPlate",false)
    loadExtraSettings(); applyExtraSettings(); refreshExtraSettings()
    moveLocalPlayerToBase()
    spawnedAtBase = Network.GetLocalPlayerID() ~= 0
    UI.SetVisible("StartMatchButton",Network.IsHost())
    UI.SetVisible("RestartMatchButton",false)
    Input.SetCursorVisible(Network.IsHost())
    if Network.IsHost() then
        -- The engine sandbox does not expose Lua's os library. math.random is sufficient here.
        resetCore()
        Network.SetCoreRushState(0,0,180,orbX,orbY,orbZ,0,0,0)
    end
end

function OnUpdate(dt)
    hudTimer=hudTimer+dt
    sendTimer=sendTimer+dt
    actionCooldown=math.max(0,actionCooldown-dt)
    fpsTimer=fpsTimer+dt; fpsFrames=fpsFrames+1
    if fpsTimer>=0.5 then UI.SetText("FPSCounter","FPS "..tostring(math.floor(fpsFrames/fpsTimer+0.5)));fpsTimer=0;fpsFrames=0 end

    if Input.IsKeyPressed("Escape") then
        setPaused(not paused)
        return
    end

    if paused then
        if UI.WasClicked("ReturnMatchButton") then setPaused(false); return end
        if UI.WasClicked("OpenSettingsButton") then settingsOpen=true;UI.SetVisible("PauseMain",false);UI.SetVisible("PauseSettings",true);refreshPauseSettings() end
        if UI.WasClicked("SettingsBackButton") then settingsOpen=false;UI.SetVisible("PauseSettings",false);UI.SetVisible("PauseMain",true) end
        if UI.WasClicked("PauseAAButton") then aaIndex=aaIndex%#aaSamples+1;savePauseSettings();refreshPauseSettings() end
        if UI.WasClicked("PauseFogButton") then pauseFog=not pauseFog;savePauseSettings();refreshPauseSettings() end
        if UI.WasClicked("PauseBloomButton") then pauseBloom=not pauseBloom;savePauseSettings();refreshPauseSettings() end
        if UI.WasClicked("PauseViewButton") then viewIndex=viewIndex%#viewDistances+1;savePauseSettings();refreshPauseSettings() end
        if UI.WasClicked("ShadowButton") then shadowIndex=shadowIndex%#shadowValues+1;applyExtraSettings();saveExtraSettings();refreshExtraSettings() end
        if UI.WasClicked("FOVButton") then fovIndex=fovIndex%#fovValues+1;applyExtraSettings();saveExtraSettings();refreshExtraSettings() end
        if UI.WasClicked("SensitivityButton") then sensitivityIndex=sensitivityIndex%#sensitivityValues+1;applyExtraSettings();saveExtraSettings();refreshExtraSettings() end
        if UI.WasClicked("InvertButton") then invertY=not invertY;applyExtraSettings();saveExtraSettings();refreshExtraSettings() end
        if UI.WasClicked("SprintModeButton") then sprintToggle=not sprintToggle;applyExtraSettings();saveExtraSettings();refreshExtraSettings() end
        if UI.WasClicked("CameraBobButton") then cameraBob=not cameraBob;applyExtraSettings();saveExtraSettings();refreshExtraSettings() end
        local master=UI.GetValue("MasterVolumeSlider"); local sfx=UI.GetValue("SFXVolumeSlider"); local uiVol=UI.GetValue("UIVolumeSlider")
        masterIndex=nearestIndex(volumeValues,master); sfxIndex=nearestIndex(volumeValues,sfx); uiVolumeIndex=nearestIndex(volumeValues,uiVol)
        Audio.SetMasterVolume(master); Audio.SetSFXVolume(sfx); Audio.SetUIVolume(uiVol)
        UI.SetText("MasterVolumeValue","MASTER: "..math.floor(master*100+0.5).."%")
        UI.SetText("SFXVolumeValue","SFX: "..math.floor(sfx*100+0.5).."%"); UI.SetText("UIVolumeValue","UI VOLUME: "..math.floor(uiVol*100+0.5).."%")
        if UI.WasClicked("ShowFPSButton") then showFPS=not showFPS;applyExtraSettings();saveExtraSettings();refreshExtraSettings() end
        if UI.WasClicked("PauseMenuButton") then Scene.SetPaused(false);Network.Disconnect();Scene.Load("Assets/Scenes/MainMenu.scene");return end
        return
    end

    -- Clients may not have their player ID yet during OnCreate. Spawn them
    -- at their own base as soon as the handshake assigns an ID.
    if not spawnedAtBase and Network.GetLocalPlayerID() ~= 0 then
        moveLocalPlayerToBase()
        spawnedAtBase=true
    end

    -- The host can click START MATCH, or press Enter as a keyboard fallback.
    if Network.IsHost() and matchStarted==0 and
       (UI.WasClicked("StartMatchButton") or Input.IsKeyPressed("Enter") or Input.IsKeyPressed("Return")) then
        resetRound()
        moveLocalPlayerToBase()
        Input.SetCursorVisible(false)
    end

    if Network.IsHost() and UI.WasClicked("RestartMatchButton") then
        resetRound()
        moveLocalPlayerToBase()
    end

    if matchStarted==1 and Input.IsKeyPressed("E") and actionCooldown<=0 then
        actionCooldown=0.25
        if carrierID==Network.GetLocalPlayerID() then Network.SendGameAction(2) else Network.SendGameAction(1) end
    end

    if Network.IsHost() then
        if matchStarted==0 then
            -- Lobby: wait for the host to explicitly start.
        elseif winner==0 then
            roundTime=math.max(0,roundTime-dt)
            for _,a in ipairs(Network.ConsumeGameActions()) do processAction(a.playerID,a.action) end

            if carrierID~=0 then
                local p=playerPosition(carrierID)
                if p.valid then
                    orbX,orbY,orbZ=p.x,p.y+1.35,p.z
                    local team=teamFor(carrierID)
                    if team=="RED" and p.z < -25.0 then
                        redScore=redScore+1; resetCore()
                    elseif team=="BLUE" and p.z > 25.0 then
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
            if restartTimer<=0 then matchStarted=0 end
        end

        if sendTimer>=0.05 then
            sendTimer=0
            Network.SetCoreRushState(redScore,blueScore,math.ceil(roundTime),orbX,orbY,orbZ,carrierID,winner,matchStarted)
        end
    else
        pullState()
    end

    local orb=Scene.FindEntity("CoreOrb")
    if orb.id ~= 0 then Scene.SetPosition(orb.id,orbX,orbY,orbZ) end

    local hostLobby=Network.IsHost() and matchStarted==0 and winner==0
    UI.SetVisible("StartMatchButton",hostLobby)
    UI.SetVisible("RestartMatchButton",Network.IsHost() and winner~=0)
    if not paused then Input.SetCursorVisible(hostLobby) end
    if hudTimer>=0.1 then hudTimer=0;updateHUD() end

end
