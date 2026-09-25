local collected = false

function OnCreate()
    Interactable.SetPrompt("Take cemetery key")
end

function OnUpdate(deltaTime)
    if not collected then
        local rotation = transform.GetRotation()
        transform.SetRotation(rotation.x, rotation.y + (24.0 * deltaTime), rotation.z)
    end
end

function OnInteract()
    if collected then return end
    if CrystalGame and CrystalGame.Collect() then
        collected = true
        Collider.SetEnabled(false)
        transform.SetPosition(0.0, -100.0, 0.0)
    end
end
