local settingsOpen = false

function OnCreate()
    UI.SetText("StatusText", "READY - BUTTONS ARE LIVE")
end

function OnUpdate(deltaTime)
    if UI.WasClicked("PlayButton") then
        UI.SetText("StatusText", "PLAY CLICKED")
        UI.SetColor("PlayButton", 0.10, 0.65, 0.32, 1.0)
    end

    if UI.WasClicked("SettingsButton") then
        settingsOpen = not settingsOpen
        if settingsOpen then
            UI.SetText("StatusText", "SETTINGS CLICKED")
            UI.SetColor("SettingsButton", 0.12, 0.45, 0.80, 1.0)
        else
            UI.SetText("StatusText", "SETTINGS CLOSED")
            UI.SetColor("SettingsButton", 0.12, 0.14, 0.18, 1.0)
        end
    end

    if UI.WasClicked("QuitButton") then
        UI.SetText("StatusText", "QUIT CLICKED - TEST PASSED")
        UI.SetColor("QuitButton", 0.70, 0.16, 0.16, 1.0)
    end
end

function OnDestroy()
end
