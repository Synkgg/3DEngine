function OnCreate()
    Interactable.SetPrompt("Activate crystal altar")
end

function OnInteract()
    if not CrystalGame then return end

    local result = CrystalGame.TryActivateAltar()

    if result == "missing" then
        UI.SetText("Objective", "THE ALTAR NEEDS ALL 3 ENERGY CRYSTALS")
    elseif result == "won" then
        UI.SetText("Objective", "COURTYARD RESTORED - YOU WIN!")
        UI.SetVisible("WinScreen", true)
        Input.SetCursorVisible(true)
    end
end
