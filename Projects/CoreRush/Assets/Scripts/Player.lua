local moveSpeed = 4.0
local sprintSpeed = 7.0
local mouseSensitivity = 0.01
local cameraHeight = 0.4
local sprintToggled = false
local bobTime = 0.0
local smoothedBob = 0.0
local cameraInitialized = false
local cameraX = 0.0
local cameraY = 0.0
local cameraZ = 0.0

function OnCreate()
    print("Player controller started.")
end

function OnUpdate(deltaTime)

    mouseSensitivity = State.GetNumber("mouse_sensitivity", 0.01)

    -- Camera look
    local mouseX =
        Input.GetMouseDeltaX()

    local mouseY =
        Input.GetMouseDeltaY()

    local pitchDirection = State.GetBool("invert_y", false) and 1.0 or -1.0
    Camera.Rotate(
        mouseX * mouseSensitivity,
        mouseY * mouseSensitivity * pitchDirection
    )

    -- WASD input
    local inputX = 0.0
    local inputZ = 0.0

    if Input.IsKeyDown("W") then
        inputZ = inputZ + 1.0
    end

    if Input.IsKeyDown("S") then
        inputZ = inputZ - 1.0
    end

    if Input.IsKeyDown("D") then
        inputX = inputX + 1.0
    end

    if Input.IsKeyDown("A") then
        inputX = inputX - 1.0
    end

    -- Get camera directions
    local forward =
        Camera.GetForward()

    local right =
        Camera.GetRight()

    -- Convert input into camera-relative movement
    local moveX =
        right.x * inputX +
        forward.x * inputZ

    local moveZ =
        right.z * inputX +
        forward.z * inputZ

    -- Sprint can be hold or toggle from Settings.
    local shiftDown = Input.IsKeyDown("Left Shift") or Input.IsKeyDown("LShift")
    local shiftPressed = Input.IsKeyPressed("Left Shift") or Input.IsKeyPressed("LShift")
    if State.GetBool("sprint_toggle", false) and shiftPressed then sprintToggled = not sprintToggled end
    local sprinting = State.GetBool("sprint_toggle", false) and sprintToggled or shiftDown
    local currentSpeed = sprinting and sprintSpeed or moveSpeed

    -- Prevent diagonal movement from being faster
    local length =
        math.sqrt(
            moveX * moveX +
            moveZ * moveZ
        )

    if length > 0.0 then
        moveX =
            moveX / length * currentSpeed

        moveZ =
            moveZ / length * currentSpeed
    end

    CharacterController.Move(
        moveX,
        moveZ
    )

    -- Jump
    if Input.IsKeyPressed("Space") then
        CharacterController.Jump()
    end

    -- Rotate mesh to match camera yaw
    local yaw =
        math.deg(
            math.atan(
                forward.x,
                -forward.z
            )
        )

    Mesh.SetRotation(
        0.0,
        yaw,
        0.0
    )

    -- Camera follows player
    local position =
        transform.GetPosition()

    local bob = 0.0
    if State.GetBool("camera_bob", true) and length > 0.0 and CharacterController.IsGrounded() then
        bobTime = bobTime + deltaTime * (sprinting and 12.0 or 8.0)
        bob = math.sin(bobTime) * 0.035
    end
    -- Smooth the positional camera follow/bob without filtering mouse rotation.
    -- This removes the small vertical/physics-step snaps that read as look jitter,
    -- while keeping aiming responsive.
    local bobBlend = 1.0 - math.exp(-18.0 * math.max(deltaTime, 0.0))
    smoothedBob = smoothedBob + (bob - smoothedBob) * bobBlend

    local targetX = position.x
    local targetY = position.y + cameraHeight + smoothedBob
    local targetZ = position.z

    if not cameraInitialized then
        cameraX, cameraY, cameraZ = targetX, targetY, targetZ
        cameraInitialized = true
    else
        local followBlend = 1.0 - math.exp(-28.0 * math.max(deltaTime, 0.0))
        cameraX = cameraX + (targetX - cameraX) * followBlend
        cameraY = cameraY + (targetY - cameraY) * followBlend
        cameraZ = cameraZ + (targetZ - cameraZ) * followBlend
    end

    Camera.SetPosition(cameraX, cameraY, cameraZ)
end

function OnDestroy()
    print("Player controller stopped.")
end