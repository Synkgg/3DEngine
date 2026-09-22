local aa = true
local fog = true
local bloom = true
local viewIndex = 3
local viewDistances = { 250.0, 600.0, 1500.0 }
local viewNames = { "LOW", "MEDIUM", "HIGH" }

local function ApplyGraphics()
    Graphics.SetAntiAliasing(aa)
    Graphics.SetFog(fog)
    Graphics.SetBloom(bloom)
    Graphics.SetViewDistance(viewDistances[viewIndex])
    Graphics.SetExposure(1.0)
    Graphics.SetFogDensity(0.006)
    Graphics.SetBloomStrength(0.12)

    UI.SetText("AAStatus", "ANTI-ALIASING: " .. (aa and "ON" or "OFF"))
    UI.SetText("FogStatus", "FOG: " .. (fog and "ON" or "OFF"))
    UI.SetText("BloomStatus", "BLOOM: " .. (bloom and "ON" or "OFF"))
    UI.SetText("ViewStatus", "VIEW DISTANCE: " .. viewNames[viewIndex])
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
        aa = not aa
        ApplyGraphics()
    end

    if UI.WasClicked("FogToggle") then
        fog = not fog
        ApplyGraphics()
    end

    if UI.WasClicked("BloomToggle") then
        bloom = not bloom
        ApplyGraphics()
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
