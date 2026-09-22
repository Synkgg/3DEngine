local moveSpeed = 4.0
local mouseSensitivity = 0.01
local cameraHeight = 0.4

function OnCreate()
    print("Player controller started.")
end

function OnUpdate(deltaTime)

    -- Camera look
    local mouseX =
        Input.GetMouseDeltaX()

    local mouseY =
        Input.GetMouseDeltaY()

    Camera.Rotate(
        mouseX * mouseSensitivity,
        -mouseY * mouseSensitivity
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

    -- Prevent diagonal movement from being faster
    local length =
        math.sqrt(
            moveX * moveX +
            moveZ * moveZ
        )

    if length > 0.0 then
        moveX =
            moveX / length * moveSpeed

        moveZ =
            moveZ / length * moveSpeed
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

    Camera.SetPosition(
        position.x,
        position.y + cameraHeight,
        position.z
    )
end

function OnDestroy()
    print("Player controller stopped.")
end