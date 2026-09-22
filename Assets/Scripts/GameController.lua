local inventoryOpen = false

local function RefreshGameUI()
    if not CrystalGame then return end

    UI.SetText("CrystalCount", "ENERGY CRYSTALS: " .. CrystalGame.collected .. " / " .. CrystalGame.required)

    if CrystalGame.won then
        UI.SetText("Objective", "COURTYARD RESTORED - YOU WIN!")
        UI.SetVisible("WinScreen", true)
        Input.SetCursorVisible(true)
    elseif CrystalGame.collected >= CrystalGame.required then
        UI.SetText("Objective", "RETURN TO THE CENTER ALTAR AND PRESS E")
    else
        UI.SetText("Objective", "FIND 3 ENERGY CRYSTALS - LOOK AT ONE AND PRESS E")
    end

    CrystalGame.uiDirty = false
end

function OnCreate()
    UI.Load("Assets/UI/GameHUD.ui")
    UI.SetVisible("Inventory", false)
    UI.SetVisible("WinScreen", false)
    Input.SetCursorVisible(false)
    RefreshGameUI()
end

function OnUpdate(deltaTime)
    if CrystalGame and CrystalGame.uiDirty then
        RefreshGameUI()
    end

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
