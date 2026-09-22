local aa = true
local aaIndex = 3
local aaSamples = { 1, 2, 4, 8 }
local aaNames = { "OFF", "2X", "4X", "8X" }
local shadowIndex = 3
local shadowNames = { "OFF", "LOW", "MEDIUM", "HIGH" }
local fog = false
local bloom = false
local viewIndex = 3
local viewDistances = { 250.0, 600.0, 1500.0 }
local viewNames = { "LOW", "MEDIUM", "HIGH" }

local function ApplyGraphics()
    Graphics.SetAntiAliasing(aa)
    Graphics.SetAntiAliasingSamples(aaSamples[aaIndex])
    -- Shadows and bloom stay disabled until their real render passes exist.
    Graphics.SetShadowQuality(0)
    Graphics.SetFog(fog)
    Graphics.SetBloom(false)
    Graphics.SetViewDistance(viewDistances[viewIndex])
    Graphics.SetExposure(1.0)
    Graphics.SetFogDensity(0.003)
    Graphics.SetBloomStrength(0.0)

    UI.SetText("AAStatus", "ANTI-ALIASING: " .. Graphics.GetAntiAliasingSamples() .. "X")
    UI.SetText("FogStatus", "FOG: " .. (Graphics.GetFog() and "ON" or "OFF"))
    UI.SetText("BloomStatus", "BLOOM: UNAVAILABLE")
    UI.SetText("ViewStatus", "VIEW DISTANCE: " .. math.floor(Graphics.GetViewDistance()))
end

function OnCreate()
    UI.Load("Assets/UI/Main.ui")
    UI.SetVisible("MainMenu", true)
    UI.SetVisible("SettingsPanel", false)
    UI.SetText("Subtitle", "CRYSTAL COURTYARD")
    UI.SetText("StatusText", "ENTER THE COURTYARD")
    ApplyGraphics()
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

    if UI.WasClicked("AAToggle") then
        aaIndex = aaIndex % #aaSamples + 1
        aa = aaSamples[aaIndex] > 1
        ApplyGraphics()
    end

    if UI.WasClicked("FogToggle") then
        fog = not fog
        ApplyGraphics()
    end

    if UI.WasClicked("BloomToggle") then
        UI.SetText("BloomStatus", "BLOOM: UNAVAILABLE")
    end

    if UI.WasClicked("ViewToggle") then
        viewIndex = viewIndex % #viewDistances + 1
        ApplyGraphics()
    end

    if UI.WasClicked("BackButton") then
        UI.SetVisible("SettingsPanel", false)
        UI.SetVisible("MainMenu", true)
    end
end
