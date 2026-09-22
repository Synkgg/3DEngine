CrystalGame = {
    collected = 0,
    required = 3,
    won = false,
    uiDirty = true
}

-- This file is loaded into the shared Lua state before per-entity
-- environments are created. It intentionally contains no direct UI calls:
-- UI is an entity-script API and therefore is not available here.
function CrystalGame.Collect()
    if CrystalGame.won or CrystalGame.collected >= CrystalGame.required then
        return false
    end

    CrystalGame.collected = CrystalGame.collected + 1
    CrystalGame.uiDirty = true
    return true
end

function CrystalGame.TryActivateAltar()
    if CrystalGame.won then
        return "won"
    end

    if CrystalGame.collected < CrystalGame.required then
        CrystalGame.uiDirty = true
        return "missing"
    end

    CrystalGame.won = true
    CrystalGame.uiDirty = true
    return "won"
end
