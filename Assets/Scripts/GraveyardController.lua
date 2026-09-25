local Game=require("Assets.Scripts.Systems.GraveyardGame")
local inventoryOpen=false
local paused=false
local function Refresh()
 UI.SetText("RelicCount",string.format("WARD SEALS  %d / %d",Game.seals,Game.required))
 if Game.gateOpen then UI.SetText("Objective","THE MAUSOLEUM WARD IS BROKEN") UI.SetVisible("WinScreen",true) Scene.SetPaused(true) Input.SetCursorVisible(true)
 elseif Game.seals>=Game.required then UI.SetText("Objective","RETURN TO THE MAUSOLEUM GATE")
 else UI.SetText("Objective","RECOVER THE THREE WARD SEALS") end
 Game.uiDirty=false
end
function OnCreate()
 Game.Reset() UI.Load("Assets/UI/GraveyardHUD.ui") UI.SetVisible("Inventory",false) UI.SetVisible("WinScreen",false) UI.SetVisible("PauseMenu",false)
 Scene.SetPaused(false) Input.SetCursorVisible(false) Refresh()
end
function OnUpdate(deltaTime)
 local prompt=Scene.GetInteractionPrompt()
 local showPrompt=prompt~=nil and prompt~="" and not paused
 UI.SetVisible("InteractPrompt",showPrompt) if showPrompt then UI.SetText("InteractText",prompt) end
 if Game.uiDirty then Refresh() end
 if Input.IsKeyPressed("Escape") and not Game.gateOpen then paused=not paused Scene.SetPaused(paused) UI.SetVisible("PauseMenu",paused) Input.SetCursorVisible(paused) end
 if paused then
  if UI.WasClicked("ResumeButton") then paused=false Scene.SetPaused(false) UI.SetVisible("PauseMenu",false) Input.SetCursorVisible(false)
  elseif UI.WasClicked("PauseMainMenuButton") then Scene.SetPaused(false) Scene.Load("Assets/Scenes/MainMenu.scene") end
  return
 end
 if Input.IsKeyPressed("Tab") then inventoryOpen=not inventoryOpen UI.SetVisible("Inventory",inventoryOpen) end
 if Game.gateOpen then
  if UI.WasClicked("PlayAgainButton") then Scene.SetPaused(false) Scene.Load("Assets/Scenes/Graveyard.scene")
  elseif UI.WasClicked("MainMenuButton") then Scene.SetPaused(false) Scene.Load("Assets/Scenes/MainMenu.scene") end
 end
end
