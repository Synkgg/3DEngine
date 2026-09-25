local CrystalGame = require("Assets.Scripts.Systems.CrystalGame")

function OnCreate()
    Interactable.SetPrompt("Unlock cemetery gate")
end

function OnInteract()
    if not CrystalGame then return end
    local result = CrystalGame.TryActivateAltar()
    if result == "missing" then
        UI.SetText("Objective", "THE GATE NEEDS ALL 3 KEYS")
    elseif result == "won" then
        UI.SetText("Objective", "THE CEMETERY GATE IS UNLOCKED")
        UI.SetVisible("WinScreen", true)
        Input.SetCursorVisible(true)
    end
end
