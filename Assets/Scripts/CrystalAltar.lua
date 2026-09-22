function OnCreate()
    Interactable.SetPrompt("Activate crystal altar")
end

function OnInteract()
    if CrystalGame then
        CrystalGame.TryActivateAltar()
    end
end
