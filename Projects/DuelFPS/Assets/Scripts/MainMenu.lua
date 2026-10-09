local joining=false
local joinElapsed=0
function OnCreate()
    UI.Load("Assets/UI/MainMenu.ui")
    Input.SetCursorVisible(true)
    Scene.SetPaused(false)
    if State.GetBool("breakbulk_disconnected",false) then UI.SetText("NetworkStatus","SESSION ENDED. CREATE OR JOIN A NEW DUEL.");State.SetBool("breakbulk_disconnected",false) end
    if Network.IsConnected() then Network.Disconnect() end
end
function OnPracticeClicked()
    if joining then Network.Disconnect();joining=false end
    Input.SetCursorVisible(false)
    Scene.Load("Assets/Scenes/TerminalDrill.scene")
end
function OnHostClicked()
    joining=false
    if Network.Host(7777,2) then
        Input.SetCursorVisible(false)
        Scene.Load("Assets/Scenes/Arena.scene")
    else UI.SetText("NetworkStatus",Network.GetLastError()) end
end
function OnJoinClicked()
    if joining then return end
    local address=UI.GetText("AddressField"):gsub("%s","")
    if address=="" then address="127.0.0.1" end
    if Network.Join(address,7777) then
        joining=true;joinElapsed=0
        UI.SetText("NetworkStatus","CONTACTING "..address.." ...")
    else UI.SetText("NetworkStatus",Network.GetLastError()) end
end
function OnUpdate(dt)
    if not joining then return end
    joinElapsed=joinElapsed+dt
    if Network.IsReady() and Network.IsConnected() then
        Input.SetCursorVisible(false);Scene.Load("Assets/Scenes/Arena.scene");return
    end
    if not Network.IsConnected() or joinElapsed>12 then
        local reason=Network.GetLastError()
        if reason=="" then reason="NO RESPONSE. CHECK ADDRESS AND UDP 7777." end
        Network.Disconnect();joining=false;UI.SetText("NetworkStatus",reason)
    end
end
