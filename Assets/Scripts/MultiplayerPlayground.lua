local elapsed = 0.0
local broadcast = 0.0
local roundTime = 180.0
local redScore = 0
local blueScore = 0
local orbX, orbY, orbZ = 0.0, 1.25, 0.0
local pickupRadius = 2.1
local carrying = false
local scoredCooldown = 0.0

local function distanceSquared(a, x, y, z)
    local dx, dy, dz = a.x - x, a.y - y, a.z - z
    return dx * dx + dy * dy + dz * dz
end

local function resetOrb()
    orbX, orbY, orbZ = 0.0, 1.25, 0.0
    carrying = false
end

local function applyState(state)
    redScore = state.redScore
    blueScore = state.blueScore
    roundTime = state.roundSeconds
    orbX, orbY, orbZ = state.orbX, state.orbY, state.orbZ
end

local function updateHUD()
    local role = Network.IsHost() and "HOST" or "CLIENT"
    UI.SetText("Status", role .. "  //  PLAYERS " .. tostring(Network.GetPlayerCount()) .. "  //  ID " .. tostring(Network.GetLocalPlayerID()))
    UI.SetText("Score", "RED " .. tostring(redScore) .. "   //   " .. tostring(math.max(0, math.floor(roundTime))) .. "   //   BLUE " .. tostring(blueScore))
    UI.SetText("Objective", carrying and "YOU HAVE THE CORE // RUN TO A GOAL" or "STEAL THE CORE // SCORE AT THE ENEMY GATE")
end

function OnCreate()
    UI.Load("Assets/UI/MultiplayerPlayground.ui")
    Input.SetCursorVisible(false)
    Scene.SetPaused(false)
    if Network.IsHost() then
        Network.SetGameState(0, 0, 180, orbX, orbY, orbZ)
    end
end

function OnUpdate(deltaTime)
    elapsed = elapsed + deltaTime
    broadcast = broadcast + deltaTime
    scoredCooldown = math.max(0.0, scoredCooldown - deltaTime)

    local player = Scene.FindEntity("Player")
    local orb = Scene.FindEntity("CoreOrb")

    if Network.IsHost() then
        roundTime = math.max(0.0, roundTime - deltaTime)
        if player:IsValid() then
            local p = player:GetPosition()
            if not carrying and distanceSquared(p, orbX, orbY, orbZ) <= pickupRadius * pickupRadius then carrying = true end
            if carrying then
                orbX, orbY, orbZ = p.x, p.y + 1.2, p.z
                if scoredCooldown <= 0.0 and p.z < -22.0 then
                    redScore = redScore + 1
                    scoredCooldown = 1.0
                    resetOrb()
                elseif scoredCooldown <= 0.0 and p.z > 22.0 then
                    blueScore = blueScore + 1
                    scoredCooldown = 1.0
                    resetOrb()
                end
            end
        end
        if roundTime <= 0.0 then
            roundTime = 180.0
            redScore, blueScore = 0, 0
            resetOrb()
        end
        if broadcast >= 0.05 then
            broadcast = 0.0
            Network.SetGameState(redScore, blueScore, math.ceil(roundTime), orbX, orbY, orbZ)
        end
    else
        applyState(Network.GetGameState())
    end

    if orb:IsValid() then orb:SetPosition(orbX, orbY, orbZ) end

    if elapsed >= 0.1 then elapsed = 0.0; updateHUD() end

    if Input.IsKeyPressed("Escape") then
        Scene.SetPaused(false)
        Network.Disconnect()
        Scene.Load("Assets/Scenes/MainMenu.scene")
    end
end
