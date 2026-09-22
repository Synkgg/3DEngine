function OnCreate()
    print("Transform API is working!")
end

function OnUpdate(deltaTime)
    local rotation = transform.GetRotation()

    rotation.y =
        rotation.y +
        (500.0 * deltaTime)

    transform.SetRotation(
        rotation.x,
        rotation.y,
        rotation.z
    )
end

function OnDestroy()
    print("Transform script stopped!")
end