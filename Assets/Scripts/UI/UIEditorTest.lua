local settingsOpen = false

local function showMainMenu()
    UI.SetVisible("MainMenu", true)
    UI.SetVisible("SettingsPanel", false)
end

function OnCreate()
    showMainMenu()
    UI.SetText("StatusText", "READY")
end

function OnUpdate(deltaTime)
    if UI.WasClicked("PlayButton") then
        UI.SetVisible("MainMenu", false)
    end

    if UI.WasClicked("SettingsButton") then
        UI.SetVisible("MainMenu", false)
        UI.SetVisible("SettingsPanel", true)
        settingsOpen = true
    end

    if UI.WasClicked("BackButton") then
        settingsOpen = false
        showMainMenu()
    end

    if UI.WasClicked("QuitButton") then
        UI.SetText("StatusText", "QUIT REQUESTED")
    end
end

function OnDestroy()
end
