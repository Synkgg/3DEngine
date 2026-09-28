local walkSpeed = 5.0
local sprintSpeed = 8.0
local sensitivity = 0.01
local cameraHeight = 0.55
local paused = false
local sendTimer = 0.0
local possessedControllerID = 0
local remotePawns = {}

local function setPaused(value)
    paused = value
    Scene.SetPaused(value)
    UI.SetVisible("PauseMenu", value)
    Input.SetCursorVisible(value)
end

local function updatePossessionAndSpawn()
    local controllerID = Controller.GetLocalID()
    if controllerID == 0 or controllerID == possessedControllerID then return end

    Controller.Possess(self.id, controllerID)
    possessedControllerID = controllerID

    local start = Controller.GetPlayerStart(controllerID)
    if start.valid then
        transform.SetPosition(start.x, start.y, start.z)
    end
end

local function getOrCreateRemotePawn(playerID)
    local entityID = remotePawns[playerID]
    if entityID and entityID ~= 0 then return entityID end

    entityID = Scene.InstantiatePrefab("Assets/Prefabs/RemotePawn.prefab", 0)
    if entityID == 0 then return 0 end

    Controller.Possess(entityID, playerID)
    local start = Controller.GetPlayerStart(playerID)
    if start.valid then Scene.SetPosition(entityID, start.x, start.y, start.z) end
    remotePawns[playerID] = entityID
    return entityID
end

local function updateNetworking(dt)
    if not Network.IsConnected() then return end

    updatePossessionAndSpawn()
    if not Network.IsReady() or not Controller.IsLocallyControlled(self.id) then return end

    sendTimer = sendTimer + dt
    if sendTimer >= (1.0 / 30.0) then
        sendTimer = 0.0
        local p = transform.GetPosition()
        local r = self:GetRotation()
        Network.SendTransform(p.x, p.y, p.z, r.x, r.y, r.z)
    end

    local localID = Controller.GetLocalID()
    for _, remote in ipairs(Network.GetRemoteTransforms()) do
        if remote.playerID ~= localID then
            local entityID = getOrCreateRemotePawn(remote.playerID)
            if entityID ~= 0 then
                Scene.SetPosition(entityID, remote.x, remote.y, remote.z)
                Scene.SetRotation(entityID, remote.rx, remote.ry, remote.rz)
            end
        end
    end
end

function OnCreate()
    State.SetNumber("mouse_sensitivity", tonumber(Preferences.LoadString("mouse_sensitivity", "0.01")) or 0.01)
    State.SetBool("invert_y", Preferences.LoadString("invert_y", "0") == "1")

    Controller.Possess(self.id)
    possessedControllerID = Controller.GetLocalID()
    local start = Controller.GetPlayerStart(possessedControllerID)
    if start.valid then transform.SetPosition(start.x, start.y, start.z) end

    UI.Load("Assets/UI/Pause.ui")
    UI.SetVisible("PauseMenu", false)
    Scene.SetPaused(false)
    Input.SetCursorVisible(false)
end

function OnUpdate(dt)
    -- Temporary Duel networking test controls. A proper lobby will replace these.
    if not Network.IsConnected() then
        if Input.IsKeyPressed("H") then
            Network.Host(7777)
            updatePossessionAndSpawn()
        elseif Input.IsKeyPressed("J") then
            Network.Join("127.0.0.1", 7777)
        end
    end

    updateNetworking(dt)
    updatePossessionAndSpawn()
    if not Controller.IsLocallyControlled(self.id) then return end

    if Input.IsKeyPressed("Escape") then
        setPaused(not paused)
        return
    end

    if paused then
        if UI.WasClicked("ResumeButton") then setPaused(false) end
        return
    end

    sensitivity = State.GetNumber("mouse_sensitivity", 0.01)
    local invert = State.GetBool("invert_y", false) and 1.0 or -1.0
    Camera.Rotate(Input.GetMouseDeltaX() * sensitivity, Input.GetMouseDeltaY() * sensitivity * invert)

    local ix, iz = 0.0, 0.0
    if Input.IsKeyDown("W") then iz = iz + 1.0 end
    if Input.IsKeyDown("S") then iz = iz - 1.0 end
    if Input.IsKeyDown("D") then ix = ix + 1.0 end
    if Input.IsKeyDown("A") then ix = ix - 1.0 end

    local forward, right = Camera.GetForward(), Camera.GetRight()
    local mx = right.x * ix + forward.x * iz
    local mz = right.z * ix + forward.z * iz
    local length = math.sqrt(mx * mx + mz * mz)
    local speed = Input.IsKeyDown("Left Shift") and sprintSpeed or walkSpeed
    if length > 0.0 then mx, mz = mx / length * speed, mz / length * speed end

    CharacterController.Move(mx, mz)
    if Input.IsKeyPressed("Space") then CharacterController.Jump() end

    local p = transform.GetPosition()
    Camera.SetPosition(p.x, p.y + cameraHeight, p.z)

    if Input.IsMouseButtonDown(1) then
        local c, f = Camera.GetPosition(), Camera.GetForward()
        local range = 100.0
        local hit = Physics.Raycast(c.x, c.y, c.z, f.x, f.y, f.z, range, self.id)
        if hit.hit then
            Debug.DrawLine(c.x, c.y, c.z, hit.x, hit.y, hit.z, 0.2, 1.0, 0.2, 5.0)
            State.SetNumber("last_hit_entity", hit.entityID)
        else
            Debug.DrawLine(c.x, c.y, c.z, c.x + f.x * range, c.y + f.y * range, c.z + f.z * range, 1.0, 0.2, 0.2, 5.0)
        end
    end
end
