local inventoryOpen = false

function OnCreate()
    UI.Load("Assets/UI/GameHUD.ui")
    UI.SetVisible("Inventory", false)
end

function OnUpdate(deltaTime)
    if Input.IsKeyPressed("Tab") then
        inventoryOpen = not inventoryOpen
        UI.SetVisible("Inventory", inventoryOpen)
    end

    if Input.IsKeyPressed("M") then
        Scene.Load("Assets/Scenes/MainMenu.scene")
    end
end
