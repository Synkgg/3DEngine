local walkSpeed = 5.0
local sprintSpeed = 8.0
local sensitivity = 0.01
local cameraHeight = 0.55
local paused = false

local function setPaused(value)
    paused = value
    Scene.SetPaused(value)
    UI.SetVisible("PauseMenu", value)
    Input.SetCursorVisible(value)
end
local paused = false

local function setPaused(value)
    paused = value
    Scene.SetPaused(value)
    UI.SetVisible("PauseMenu", value)
    Input.SetCursorVisible(value)
end

function OnCreate()
    State.SetNumber("mouse_sensitivity", tonumber(Preferences.LoadString("mouse_sensitivity", "0.01")) or 0.01)
    State.SetBool("invert_y", Preferences.LoadString("invert_y", "0") == "1")
    Controller.Possess(self.id)
    UI.Load("Assets/UI/Pause.ui")
    UI.SetVisible("PauseMenu", false)
    Scene.SetPaused(false)
    Input.SetCursorVisible(false)
end

function OnUpdate(dt)
    if not Controller.IsLocallyControlled(self.id) then return end
    if Input.IsKeyPressed("Escape") then setPaused(not paused); return end
    if paused then
        if UI.WasClicked("ResumeButton") then setPaused(false) end
        return
    end

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
        local hit = Physics.Raycast(c.x,c.y,c.z,f.x,f.y,f.z,range,self.id)
        if hit.hit then
            Debug.DrawLine(c.x,c.y,c.z,hit.x,hit.y,hit.z,0.2,1.0,0.2,5.0)
            State.SetNumber("last_hit_entity", hit.entityID)
        else
            Debug.DrawLine(c.x,c.y,c.z,c.x+f.x*range,c.y+f.y*range,c.z+f.z*range,1.0,0.2,0.2,5.0)
        end
    end
end
