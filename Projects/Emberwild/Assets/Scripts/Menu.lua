local joining=false
local elapsed=0
function OnCreate() UI.Load('Assets/UI/Menu.ui');Input.SetCursorVisible(true) end
local function enter() Scene.Load('Assets/Scenes/Forest.scene') end
function OnSolo() Network.Disconnect();State.SetBool('ember_online',false);enter() end
function OnHost()
 if Network.Host(7777,2,'Emberwild camp') then State.SetBool('ember_online',true);enter() else UI.SetText('Status',Network.GetLastError()) end
end
local function join(ip,port)
 if Network.Join(ip,port) then joining=true;elapsed=0;UI.SetText('Status','Connecting to camp...') else UI.SetText('Status',Network.GetLastError()) end
end
function OnJoin() join(UI.GetText('Address'),7777) end
function OnRelay() join('127.0.0.1',17777) end
function OnUpdate(dt)
 if not joining then return end
 elapsed=elapsed+dt
 if Network.IsReady() then joining=false;State.SetBool('ember_online',true);enter()
 elseif elapsed>12 or not Network.IsConnected() then joining=false;Network.Disconnect();UI.SetText('Status','Could not connect. Check the host and relay helper, then retry.') end
end
