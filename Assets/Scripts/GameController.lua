local inventoryOpen = false
local paused = false
local pauseSettingsOpen = false
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

local function RefreshGraphicsLabels()
    UI.SetText("PauseAAStatus", "ANTI-ALIASING: " .. Graphics.GetAntiAliasingSamples() .. "X")
    UI.SetText("PauseFogStatus", "FOG: " .. (Graphics.GetFog() and "ON" or "OFF"))
    UI.SetText("PauseBloomStatus", "BLOOM: UNAVAILABLE")
    UI.SetText("PauseViewStatus", "VIEW DISTANCE: " .. math.floor(Graphics.GetViewDistance()))
end

local function ApplyGraphics()
    Graphics.SetAntiAliasing(aa)
    Graphics.SetAntiAliasingSamples(aaSamples[aaIndex])
    -- Shadows and bloom stay disabled until their real render passes exist.
    Graphics.SetShadowQuality(0)
    Graphics.SetFog(fog)
    Graphics.SetBloom(false)
    Graphics.SetViewDistance(viewDistances[viewIndex])
    RefreshGraphicsLabels()
end

local function SetPaused(value)
    if CrystalGame and CrystalGame.won then return end
    paused = value
    pauseSettingsOpen = false
    Scene.SetPaused(paused)
    UI.SetVisible("PauseMenu", paused)
    UI.SetVisible("PauseSettingsPanel", false)
    Input.SetCursorVisible(paused)
end

local function RefreshGameUI()
    if not CrystalGame then return end
    UI.SetText("CrystalCount", "ENERGY CRYSTALS: " .. CrystalGame.collected .. " / " .. CrystalGame.required)

    if CrystalGame.won then
        UI.SetText("Objective", "COURTYARD RESTORED - YOU WIN!")
        UI.SetVisible("WinScreen", true)
        Scene.SetPaused(true)
        Input.SetCursorVisible(true)
    elseif CrystalGame.collected >= CrystalGame.required then
        UI.SetText("Objective", "RETURN TO THE CENTER ALTAR AND PRESS E")
    else
        UI.SetText("Objective", "FIND 3 ENERGY CRYSTALS - LOOK AT ONE AND PRESS E")
    end
    CrystalGame.uiDirty = false
end

function OnCreate()
    UI.Load("Assets/UI/GameHUD.ui")
    UI.SetVisible("Inventory", false)
    UI.SetVisible("WinScreen", false)
    UI.SetVisible("PauseMenu", false)
    UI.SetVisible("PauseSettingsPanel", false)
    Scene.SetPaused(false)
    Input.SetCursorVisible(false)
    ApplyGraphics()
    RefreshGameUI()
end

function OnUpdate(deltaTime)
    if CrystalGame and CrystalGame.uiDirty then RefreshGameUI() end

    if Input.IsKeyPressed("Escape") and not (CrystalGame and CrystalGame.won) then
        if pauseSettingsOpen then
            pauseSettingsOpen = false
            UI.SetVisible("PauseSettingsPanel", false)
            UI.SetVisible("PauseMenu", true)
        else
            SetPaused(not paused)
        end
    end

    if paused then
        if UI.WasClicked("ResumeButton") then SetPaused(false) end

        if UI.WasClicked("PauseSettingsButton") then
            pauseSettingsOpen = true
            UI.SetVisible("PauseMenu", false)
            UI.SetVisible("PauseSettingsPanel", true)
            RefreshGraphicsLabels()
        end

        if UI.WasClicked("PauseSettingsBackButton") then
            pauseSettingsOpen = false
            UI.SetVisible("PauseSettingsPanel", false)
            UI.SetVisible("PauseMenu", true)
        end

        if UI.WasClicked("PauseAAToggle") then
            aaIndex = aaIndex % #aaSamples + 1
            aa = aaSamples[aaIndex] > 1
            ApplyGraphics()
        end
        if UI.WasClicked("PauseFogToggle") then fog = not fog; ApplyGraphics() end
        if UI.WasClicked("PauseBloomToggle") then
            UI.SetText("PauseBloomStatus", "BLOOM: UNAVAILABLE")
        end
        if UI.WasClicked("PauseViewToggle") then
            viewIndex = viewIndex % #viewDistances + 1
            ApplyGraphics()
        end

        if UI.WasClicked("PauseMainMenuButton") then
            Scene.SetPaused(false)
            Scene.Load("Assets/Scenes/MainMenu.scene")
            return
        end
        return
    end

    if Input.IsKeyPressed("Tab") then
        inventoryOpen = not inventoryOpen
        UI.SetVisible("Inventory", inventoryOpen)
    end

    if Input.IsKeyPressed("M") then
        Scene.Load("Assets/Scenes/MainMenu.scene")
        return
    end

    if CrystalGame and CrystalGame.won then
        if UI.WasClicked("PlayAgainButton") then
            Scene.SetPaused(false)
            Scene.Load("Assets/Scenes/CrystalCourtyard.scene")
            return
        end
        if UI.WasClicked("MainMenuButton") then
            Scene.SetPaused(false)
            Scene.Load("Assets/Scenes/MainMenu.scene")
            return
        end
    end
end
