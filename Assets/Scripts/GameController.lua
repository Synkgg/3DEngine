local inventoryOpen = false

function OnCreate()
    UI.Load("Assets/UI/GameHUD.ui")
    UI.SetVisible("Inventory", false)
    UI.SetVisible("WinScreen", false)

    if CrystalGame then
        CrystalGame.RefreshUI()
    end
end

function OnUpdate(deltaTime)
    if Input.IsKeyPressed("Tab") then
        inventoryOpen = not inventoryOpen
        UI.SetVisible("Inventory", inventoryOpen)
    end

    if Input.IsKeyPressed("M") then
        Scene.Load("Assets/Scenes/MainMenu.scene")
        return
    end

    if CrystalGame and CrystalGame.won then
        if UI.WasClicked("PlayAgainButton") then
            Scene.Load("Assets/Scenes/CrystalCourtyard.scene")
            return
        end

        if UI.WasClicked("MainMenuButton") then
            Scene.Load("Assets/Scenes/MainMenu.scene")
            return
        end
    end
end
