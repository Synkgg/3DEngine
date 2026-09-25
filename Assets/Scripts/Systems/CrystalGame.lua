local CrystalGame = {
    collected = 0,
    required = 3,
    won = false,
    uiDirty = true
}

function CrystalGame.Collect()
    if CrystalGame.won or CrystalGame.collected >= CrystalGame.required then return false end
    CrystalGame.collected = CrystalGame.collected + 1
    CrystalGame.uiDirty = true
    return true
end

function CrystalGame.TryActivateAltar()
    if CrystalGame.won then return "won" end
    if CrystalGame.collected < CrystalGame.required then
        CrystalGame.uiDirty = true
        return "missing"
    end
    CrystalGame.won = true
    CrystalGame.uiDirty = true
    return "won"
end

return CrystalGame
