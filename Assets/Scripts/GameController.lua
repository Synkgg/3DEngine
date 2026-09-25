local CrystalGame = require("Assets.Scripts.Systems.CrystalGame")

local inventoryOpen=false
local paused=false
local function Refresh()
    if not CrystalGame then return end
    UI.SetText("CrystalCount","KEYS  "..CrystalGame.collected.." / "..CrystalGame.required)
    if CrystalGame.won then
        UI.SetText("Objective","THE CEMETERY GATE IS OPEN")
        UI.SetVisible("WinScreen",true)
        Scene.SetPaused(true)
        Input.SetCursorVisible(true)
    elseif CrystalGame.collected>=CrystalGame.required then
        UI.SetText("Objective","RETURN TO THE FRONT GATE")
    else
        UI.SetText("Objective","FIND THE 3 CEMETERY KEYS")
    end
    CrystalGame.uiDirty=false
end
function OnCreate()
    UI.Load("Assets/UI/GameHUD.ui")
    UI.SetVisible("Inventory",false)
    UI.SetVisible("WinScreen",false)
    UI.SetVisible("PauseMenu",false)
    UI.SetVisible("PauseSettingsPanel",false)
    Scene.SetPaused(false)
    Input.SetCursorVisible(false)
    Graphics.SetExposure(0.78)
    Graphics.SetFog(true)
    Graphics.SetFogDensity(0.012)
    Graphics.SetViewDistance(220.0)
    Refresh()
end
function OnUpdate(deltaTime)
    local interactionPrompt=Scene.GetInteractionPrompt()
    local showInteraction=interactionPrompt~=nil and interactionPrompt~="" and not paused
    UI.SetVisible("InteractPrompt",showInteraction)
    if showInteraction then UI.SetText("InteractText",interactionPrompt) end
    if CrystalGame and CrystalGame.uiDirty then Refresh() end
    if Input.IsKeyPressed("Escape") and not (CrystalGame and CrystalGame.won) then
        paused=not paused
        Scene.SetPaused(paused)
        UI.SetVisible("PauseMenu",paused)
        Input.SetCursorVisible(paused)
    end
    if paused then
        if UI.WasClicked("ResumeButton") then paused=false Scene.SetPaused(false) UI.SetVisible("PauseMenu",false) Input.SetCursorVisible(false) end
        if UI.WasClicked("PauseMainMenuButton") then Scene.SetPaused(false) Scene.Load("Assets/Scenes/MainMenu.scene") return end
        return
    end
    if Input.IsKeyPressed("Tab") then inventoryOpen=not inventoryOpen UI.SetVisible("Inventory",inventoryOpen) end
    if CrystalGame and CrystalGame.won then
        if UI.WasClicked("PlayAgainButton") then Scene.SetPaused(false) Scene.Load("Assets/Scenes/CrystalCourtyard.scene") return end
        if UI.WasClicked("MainMenuButton") then Scene.SetPaused(false) Scene.Load("Assets/Scenes/MainMenu.scene") return end
    end
end
