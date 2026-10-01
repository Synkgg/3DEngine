local inventoryOpen=false
local paused=false
local seals=0
local required=3
local gateOpen=false

local function Refresh()
 UI.SetText("RelicCount",string.format("%02d / %02d  WARD SEALS",seals,required))
 UI.SetText("JournalProgress",string.format("RECOVERED   %d OF %d",seals,required))
 if gateOpen then
  UI.SetText("Objective","DESCEND INTO THE MAUSOLEUM")
  UI.SetText("ObjectiveDetail","The ward is broken. The cemetery releases you.")
  UI.SetVisible("WinScreen",true)
  Input.SetCursorVisible(true)
 elseif seals>=required then
  UI.SetText("Objective","RETURN TO THE MAUSOLEUM")
  UI.SetText("ObjectiveDetail","All seals recovered. Break the iron ward.")
 else
  UI.SetText("Objective","FIND THE MISSING WARD SEALS")
  UI.SetText("ObjectiveDetail",string.format("%d remain somewhere among the graves.",required-seals))
 end
end

function OnCreate()
 UI.Load("Assets/UI/GraveyardHUD.ui")
 UI.SetVisible("Inventory",false)
 UI.SetVisible("WinScreen",false)
 UI.SetVisible("PauseMenu",false)
 Scene.SetPaused(false)
 Input.SetCursorVisible(false)
 Refresh()
end

function OnUpdate(deltaTime)
 local prompt=Scene.GetInteractionPrompt()
 local showPrompt=prompt~=nil and prompt~="" and not paused and not gateOpen
 UI.SetVisible("InteractPrompt",showPrompt)
 if showPrompt then UI.SetText("InteractText",prompt) end

 if Input.IsKeyPressed("E") and showPrompt then
  if prompt=="Recover ward seal" and seals<required then
   seals=seals+1
   Refresh()
  elseif prompt=="Break the mausoleum ward" then
   if seals>=required then
    gateOpen=true
    Refresh()
   else
    UI.SetText("Objective","THE IRON WARD HOLDS")
    UI.SetText("ObjectiveDetail",string.format("Recover %d more seal%s.",required-seals,(required-seals)==1 and "" or "s"))
   end
  end
 end

 if Input.IsKeyPressed("Escape") and not gateOpen then
  paused=not paused
  Scene.SetPaused(paused)
  UI.SetVisible("PauseMenu",paused)
  Input.SetCursorVisible(paused)
 end

 if paused then
  if UI.WasClicked("ResumeButton") then
   paused=false Scene.SetPaused(false) UI.SetVisible("PauseMenu",false) Input.SetCursorVisible(false)
  elseif UI.WasClicked("PauseMainMenuButton") then
   Scene.SetPaused(false) Network.Disconnect() Scene.Load("Assets/Scenes/MainMenu.scene")
  end
  return
 end

 if Input.IsKeyPressed("Tab") and not gateOpen then
  inventoryOpen=not inventoryOpen
  UI.SetVisible("Inventory",inventoryOpen)
  Input.SetCursorVisible(inventoryOpen)
 end

 if gateOpen then
  if UI.WasClicked("PlayAgainButton") then Scene.SetPaused(false) Scene.Load("Assets/Scenes/Graveyard.scene")
  elseif UI.WasClicked("MainMenuButton") then Scene.SetPaused(false) Network.Disconnect() Scene.Load("Assets/Scenes/MainMenu.scene") end
 end
end
