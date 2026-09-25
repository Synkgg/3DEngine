local collected=false

function OnCreate()
 self:SetInteractablePrompt("Recover ward seal")
end

function OnUpdate(deltaTime)
 if collected then return end
 local r=self:GetRotation()
 self:SetRotation(r.x,r.y+30.0*deltaTime,r.z)
end

function OnInteract()
 if collected then return end
 collected=true

 -- Disable interaction immediately, then hide the actual rendered object.
 self:SetInteractableEnabled(false)
 self:SetScale(0.0,0.0,0.0)
end
