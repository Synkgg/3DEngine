local GraveyardGame = { seals=0, required=3, gateOpen=false, uiDirty=true }
function GraveyardGame.Reset() GraveyardGame.seals=0 GraveyardGame.gateOpen=false GraveyardGame.uiDirty=true end
function GraveyardGame.CollectSeal()
 if GraveyardGame.gateOpen or GraveyardGame.seals>=GraveyardGame.required then return false end
 GraveyardGame.seals=GraveyardGame.seals+1 GraveyardGame.uiDirty=true return true
end
function GraveyardGame.TryOpenGate()
 if GraveyardGame.gateOpen then return "open" end
 if GraveyardGame.seals<GraveyardGame.required then GraveyardGame.uiDirty=true return "missing" end
 GraveyardGame.gateOpen=true GraveyardGame.uiDirty=true return "open"
end
return GraveyardGame
