local aaSamples = { 1, 2, 4, 8 }
local viewDistances = { 220.0, 500.0, 1000.0 }
local aaIndex = 1
local viewIndex = 1

local function nearestIndex(values, current)
    local best, distance = 1, math.abs(values[1] - current)
    for i = 2, #values do
        local d = math.abs(values[i] - current)
        if d < distance then best, distance = i, d end
    end
    return best
end

local function RefreshGraphicsUI()
    aaIndex = nearestIndex(aaSamples, Graphics.GetAntiAliasingSamples())
    viewIndex = nearestIndex(viewDistances, Graphics.GetViewDistance())
    UI.SetText("AAStatus", "ANTI-ALIASING: " .. Graphics.GetAntiAliasingSamples() .. "X")
    UI.SetText("FogStatus", "FOG: " .. (Graphics.GetFog() and "ON" or "OFF"))
    UI.SetText("BloomStatus", "BLOOM: " .. (Graphics.GetBloom() and "ON" or "OFF"))
    UI.SetText("ViewStatus", "VIEW DISTANCE: " .. math.floor(Graphics.GetViewDistance()))
end

local function SaveGraphics()
    if not Graphics.Save() then print("Failed to save project graphics settings.") end
    RefreshGraphicsUI()
end

function OnCreate()
    Input.SetCursorVisible(true)
    UI.Load("Assets/UI/Main.ui")
    UI.SetVisible("MainMenu", true)
    UI.SetVisible("SettingsPanel", false)
    Camera.Reset()
    RefreshGraphicsUI()
end

function OnUpdate(deltaTime)
    if UI.WasClicked("PlayButton") then
        Input.SetCursorVisible(false)
        Scene.Load("Assets/Scenes/Graveyard.scene")
        return
    end
    if UI.WasClicked("SettingsButton") then UI.SetVisible("MainMenu", false); UI.SetVisible("SettingsPanel", true) end
    if UI.WasClicked("AAToggle") then
        aaIndex = aaIndex % #aaSamples + 1
        Graphics.SetAntiAliasingSamples(aaSamples[aaIndex])
        SaveGraphics()
    end
    if UI.WasClicked("FogToggle") then Graphics.SetFog(not Graphics.GetFog()); SaveGraphics() end
    if UI.WasClicked("BloomToggle") then Graphics.SetBloom(not Graphics.GetBloom()); SaveGraphics() end
    if UI.WasClicked("ViewToggle") then
        viewIndex = viewIndex % #viewDistances + 1
        Graphics.SetViewDistance(viewDistances[viewIndex])
        SaveGraphics()
    end
    if UI.WasClicked("BackButton") then UI.SetVisible("SettingsPanel", false); UI.SetVisible("MainMenu", true) end
end
