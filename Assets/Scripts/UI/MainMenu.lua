local aaSamples = { 1, 2, 4, 8 }
local viewDistances = { 220.0, 500.0, 1000.0 }
local aaIndex, viewIndex = 1, 1
local pendingFog, pendingBloom = false, false
local joinAddress = "127.0.0.1"
local editingAddress = false

local function nearestIndex(values, current)
    local best, distance = 1, math.abs(values[1] - current)
    for i = 2, #values do local d = math.abs(values[i] - current); if d < distance then best, distance = i, d end end
    return best
end

local function refreshPendingUI()
    UI.SetText("AAStatus", "ANTI-ALIASING")
    UI.SetText("AAValue", aaSamples[aaIndex] == 1 and "OFF" or (aaSamples[aaIndex] .. "X"))
    UI.SetText("FogStatus", "VOLUMETRIC FOG")
    UI.SetText("FogValue", pendingFog and "ENABLED" or "DISABLED")
    UI.SetText("BloomStatus", "BLOOM")
    UI.SetText("BloomValue", pendingBloom and "ENABLED" or "DISABLED")
    UI.SetText("ViewStatus", "VIEW DISTANCE")
    UI.SetText("ViewValue", tostring(math.floor(viewDistances[viewIndex])))
end

local function loadPending()
    aaIndex = nearestIndex(aaSamples, Graphics.GetAntiAliasingSamples())
    viewIndex = nearestIndex(viewDistances, Graphics.GetViewDistance())
    pendingFog = Graphics.GetFog()
    pendingBloom = Graphics.GetBloom()
    refreshPendingUI()
end

local function applyPending()
    Graphics.SetAntiAliasingSamples(aaSamples[aaIndex])
    Graphics.SetFog(pendingFog)
    Graphics.SetBloom(pendingBloom)
    Graphics.SetViewDistance(viewDistances[viewIndex])
    if not Graphics.Save() then print("Failed to save project graphics settings.") end
    UI.SetText("ApplyLabel", "SAVED")
end

function OnCreate()
    if Network.IsConnected() then Network.Disconnect() end
    Input.SetCursorVisible(true)
    UI.Load("Assets/UI/Main.ui")
    UI.SetVisible("MainMenu", true)
    UI.SetVisible("SettingsPanel", false)
    Camera.Reset()
    loadPending()
end

function OnUpdate(deltaTime)
    if UI.WasClicked("PlayButton") then
        if Network.IsConnected() then Network.Disconnect() end
        Input.SetCursorVisible(false)
        Scene.Load("Assets/Scenes/Graveyard.scene")
        return
    end

    if UI.WasClicked("AddressField") then editingAddress = true end
    if editingAddress then
        for i = 0, 9 do
            local key = tostring(i)
            if Input.IsKeyPressed(key) and #joinAddress < 15 then joinAddress = joinAddress .. key end
        end
        -- SDL reports the main keyboard '.' key as the literal "." name.
        -- Keep "Period" as a fallback for layouts/backends that expose that name.
        if (Input.IsKeyPressed(".") or Input.IsKeyPressed("Period")) and #joinAddress < 15 then joinAddress = joinAddress .. "." end
        if Input.IsKeyPressed("Backspace") and #joinAddress > 0 then joinAddress = string.sub(joinAddress, 1, #joinAddress - 1) end
        if Input.IsKeyPressed("Return") or Input.IsKeyPressed("Escape") then editingAddress = false end
        UI.SetText("AddressText", "IP: " .. joinAddress .. (editingAddress and " _" or ""))
    end

    if UI.WasClicked("HostButton") then
        editingAddress = false
        if Network.Host(7777) then
            Input.SetCursorVisible(false)
            Scene.Load("Assets/Scenes/Graveyard.scene")
            return
        else
            UI.SetText("NetworkStatus", "HOST FAILED / " .. Network.GetLastError())
        end
    end

    if UI.WasClicked("JoinButton") then
        editingAddress = false
        if joinAddress == "" then
            UI.SetText("NetworkStatus", "ENTER A HOST IP ADDRESS")
        elseif Network.Join(joinAddress, 7777) then
            Input.SetCursorVisible(false)
            Scene.Load("Assets/Scenes/Graveyard.scene")
            return
        else
            UI.SetText("NetworkStatus", "JOIN FAILED / " .. Network.GetLastError())
        end
    end
    if UI.WasClicked("SettingsButton") then
        loadPending()
        UI.SetText("ApplyLabel", "APPLY & SAVE")
        UI.SetVisible("MainMenu", false); UI.SetVisible("SettingsPanel", true)
    end
    if UI.WasClicked("AAToggle") then aaIndex = aaIndex % #aaSamples + 1; refreshPendingUI(); UI.SetText("ApplyLabel","APPLY & SAVE") end
    if UI.WasClicked("FogToggle") then pendingFog = not pendingFog; refreshPendingUI(); UI.SetText("ApplyLabel","APPLY & SAVE") end
    if UI.WasClicked("BloomToggle") then pendingBloom = not pendingBloom; refreshPendingUI(); UI.SetText("ApplyLabel","APPLY & SAVE") end
    if UI.WasClicked("ViewToggle") then viewIndex = viewIndex % #viewDistances + 1; refreshPendingUI(); UI.SetText("ApplyLabel","APPLY & SAVE") end
    if UI.WasClicked("ApplyButton") then applyPending() end
    if UI.WasClicked("BackButton") then loadPending(); UI.SetVisible("SettingsPanel", false); UI.SetVisible("MainMenu", true) end
end
