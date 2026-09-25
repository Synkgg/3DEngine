local Game=require("Assets.Scripts.Systems.GraveyardGame")
function OnCreate() self:SetInteractablePrompt("Break the mausoleum ward") end
function OnInteract()
 local result=Game.TryOpenGate()
 if result=="missing" then UI.SetText("Objective","THE WARD REQUIRES ALL THREE SEALS")
 elseif result=="open" then self:SetInteractableEnabled(false) self:SetPosition(0.0,-100.0,0.0) end
end
