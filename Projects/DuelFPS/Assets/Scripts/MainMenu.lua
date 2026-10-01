local joining=false

function OnCreate()
    UI.Load("Assets/UI/MainMenu.ui")
    Input.SetCursorVisible(true)
    Scene.SetPaused(false)
    if Network.IsConnected() then Network.Disconnect() end
end

function OnUpdate(dt)
    if joining then
        if Network.IsReady() then
            Input.SetCursorVisible(false)
            Scene.Load("Assets/Scenes/Arena.scene")
            return
        end
        local err=Network.GetLastError()
        if err~="" then UI.SetText("NetworkStatus",err) end
        return
    end

    if UI.WasClicked("HostButton") then
        if Network.Host(7777) then
            UI.SetText("NetworkStatus","HOSTING // WAITING FOR OPPONENT")
            Input.SetCursorVisible(false)
            Scene.Load("Assets/Scenes/Arena.scene")
        else
            UI.SetText("NetworkStatus",Network.GetLastError())
        end
    elseif UI.WasClicked("JoinButton") then
        local address=UI.GetText("AddressField")
        if address=="" then address="127.0.0.1" end
        if Network.Join(address,7777) then
            joining=true
            UI.SetText("NetworkStatus","CONNECTING TO "..address.." ...")
        else
            UI.SetText("NetworkStatus",Network.GetLastError())
        end
    end
end
