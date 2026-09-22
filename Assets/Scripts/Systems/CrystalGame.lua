CrystalGame = {
    collected = 0,
    required = 3,
    altarUnlocked = false,
    won = false
}

function CrystalGame.RefreshUI()
    UI.SetText("CrystalCount", "ENERGY CRYSTALS: " .. CrystalGame.collected .. " / " .. CrystalGame.required)

    if CrystalGame.won then
        UI.SetText("Objective", "COURTYARD RESTORED - YOU WIN!")
    elseif CrystalGame.collected >= CrystalGame.required then
        UI.SetText("Objective", "RETURN TO THE CENTER ALTAR AND PRESS E")
    else
        UI.SetText("Objective", "FIND 3 ENERGY CRYSTALS - LOOK AT ONE AND PRESS E")
    end
end

function CrystalGame.Collect()
    if CrystalGame.won then return false end
    if CrystalGame.collected >= CrystalGame.required then return false end

    CrystalGame.collected = CrystalGame.collected + 1
    CrystalGame.RefreshUI()
    return true
end

function CrystalGame.TryActivateAltar()
    if CrystalGame.won then return false end

    if CrystalGame.collected < CrystalGame.required then
        UI.SetText("Objective", "THE ALTAR NEEDS ALL 3 ENERGY CRYSTALS")
        return false
    end

    CrystalGame.won = true
    UI.SetVisible("WinScreen", true)
    CrystalGame.RefreshUI()
    return true
end
