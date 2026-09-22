local playing = false
local inventoryOpen = false
local crystals = 0
local requiredCrystals = 3
local eWasDown = false

local function updateHud()
    UI.SetText("ObjectiveText", "CRYSTALS  //  " .. crystals .. " / " .. requiredCrystals)
    UI.SetText("InventoryCount", "CRYSTALS  //  " .. crystals .. " / " .. requiredCrystals)
end

local function beginGame()
    playing = true
    inventoryOpen = false
    crystals = 0
    UI.SetVisible("MainMenu", false)
    UI.SetVisible("WinScreen", false)
    UI.SetVisible("Inventory", false)
    UI.SetVisible("GameHUD", true)
    updateHud()
end

function OnCreate()
    UI.Load("Assets/UI/GameDemo.ui")
    UI.SetVisible("MainMenu", true)
    UI.SetVisible("GameHUD", false)
    UI.SetVisible("Inventory", false)
    UI.SetVisible("WinScreen", false)
end

function OnUpdate(deltaTime)
    if not playing then
        if UI.WasClicked("StartButton") or UI.WasClicked("RestartButton") then
            beginGame()
        end
        return
    end

    if Input.IsKeyPressed("Tab") then
        inventoryOpen = not inventoryOpen
        UI.SetVisible("Inventory", inventoryOpen)
    end

    if inventoryOpen then
        return
    end

    -- Simple beatable demo: each deliberate E press collects one crystal.
    -- This exercises game state, input, HUD updates, inventory, and a win state.
    local eDown = Input.IsKeyDown("E")
    if eDown and not eWasDown then
        crystals = crystals + 1
        updateHud()

        if crystals >= requiredCrystals then
            playing = false
            UI.SetVisible("GameHUD", false)
            UI.SetVisible("Inventory", false)
            UI.SetVisible("WinScreen", true)
        end
    end
    eWasDown = eDown
end

function OnDestroy()
    UI.Clear()
end
