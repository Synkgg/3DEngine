function OnCreate()
    UI.Load("Assets/UI/Main.ui")
    UI.SetVisible("MainMenu", true)
    UI.SetVisible("SettingsPanel", false)
    UI.SetText("Subtitle", "CRYSTAL COURTYARD")
    UI.SetText("StatusText", "ENTER THE COURTYARD")
end

function OnUpdate(deltaTime)
    if UI.WasClicked("PlayButton") then
        Scene.Load("Assets/Scenes/CrystalCourtyard.scene")
        return
    end

    if UI.WasClicked("SettingsButton") then
        UI.SetVisible("MainMenu", false)
        UI.SetVisible("SettingsPanel", true)
    end

    if UI.WasClicked("BackButton") then
        UI.SetVisible("SettingsPanel", false)
        UI.SetVisible("MainMenu", true)
    end
end
