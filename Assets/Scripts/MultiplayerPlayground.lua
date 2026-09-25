local elapsed = 0.0

function OnCreate()
    UI.Load("Assets/UI/MultiplayerPlayground.ui")
    Input.SetCursorVisible(false)
    Scene.SetPaused(false)
end

function OnUpdate(deltaTime)
    elapsed = elapsed + deltaTime
    if elapsed >= 0.25 then
        elapsed = 0.0
        local role = Network.IsHost() and "HOST" or "CLIENT"
        UI.SetText("Status", role .. "  //  PLAYERS " .. tostring(Network.GetPlayerCount()) .. "  //  UDP 7777")
    end

    if Input.IsKeyPressed("Escape") then
        Scene.SetPaused(false)
        Network.Disconnect()
        Scene.Load("Assets/Scenes/MainMenu.scene")
    end
end
