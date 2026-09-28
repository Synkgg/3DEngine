function OnCreate() self:SetInteractablePrompt("Break the mausoleum ward") end
function OnInteract()
 self:SetInteractableEnabled(false)
 self:SetPosition(0.0,-100.0,0.0)
end
