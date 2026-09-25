local moveSpeed = 4.0
local sprintSpeed = 7.0
local mouseSensitivity = 0.01
local cameraHeight = 0.4
local sprintToggled = false
local bobTime = 0.0

function OnCreate()
    print("Player controller started.")
end

function OnUpdate(deltaTime)

    mouseSensitivity = GameSettings.GetMouseSensitivity()

    -- Camera look
    local mouseX =
        Input.GetMouseDeltaX()

    local mouseY =
        Input.GetMouseDeltaY()

    local pitchDirection = GameSettings.GetInvertY() and 1.0 or -1.0
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
    if GameSettings.GetSprintToggle() and shiftPressed then sprintToggled = not sprintToggled end
    local sprinting = GameSettings.GetSprintToggle() and sprintToggled or shiftDown
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
    if GameSettings.GetCameraBob() and length > 0.0 and CharacterController.IsGrounded() then
        bobTime = bobTime + deltaTime * (sprinting and 12.0 or 8.0)
        bob = math.sin(bobTime) * 0.035
    end
    Camera.SetPosition(
        position.x,
        position.y + cameraHeight + bob,
        position.z
    )
end

function OnDestroy()
    print("Player controller stopped.")
end