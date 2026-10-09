local joining=false
local loadoutSlot="primary"
local pendingScreen=nil
local menuStatus=""
local loadoutPrimary="rifle"
local loadoutSecondary="pistol"
local valid={pistol=true,rifle=true,shotgun=true,smg=true}
local guns={
    pistol={name="MAKO P12",role="PRECISION SIDEARM",stats="12 ROUNDS  /  HIGH CONTROL"},
    rifle={name="KESTREL AR4",role="ASSAULT RIFLE",stats="30 ROUNDS  /  BALANCED RANGE"},
    shotgun={name="BREACH S8",role="PUMP SHOTGUN",stats="8 SHELLS  /  CLOSE RANGE"},
    smg={name="VECTOR K9",role="SUBMACHINE GUN",stats="36 ROUNDS  /  HIGH RATE"}
}
local function restoreLoadout()
    local primary=Preferences.LoadString("breakbulk_primary","rifle")
    local secondary=Preferences.LoadString("breakbulk_secondary","pistol")
    loadoutPrimary=valid[primary] and primary or "rifle"
    loadoutSecondary=valid[secondary] and secondary or "pistol"
    if loadoutPrimary==loadoutSecondary then loadoutSecondary=loadoutPrimary=="pistol" and "rifle" or "pistol" end
end
local function refreshLoadout()
    UI.SetText("PrimaryValue",guns[loadoutPrimary].name)
    UI.SetText("SecondaryValue",guns[loadoutSecondary].name)
    UI.SetText("SlotHint",loadoutSlot=="primary" and "SELECT YOUR PRIMARY WEAPON" or "SELECT YOUR SECONDARY WEAPON")
    UI.SetText("PrimaryTabLabel",(loadoutSlot=="primary" and "> " or "").."01  PRIMARY")
    UI.SetText("SecondaryTabLabel",(loadoutSlot=="secondary" and "> " or "").."02  SECONDARY")
    for _,id in ipairs({"pistol","rifle","shotgun","smg"}) do
        local equipped=loadoutPrimary==id or loadoutSecondary==id
        local selected=(loadoutSlot=="primary" and loadoutPrimary==id) or (loadoutSlot=="secondary" and loadoutSecondary==id)
        UI.SetText(id.."Status",selected and "SELECTED IN THIS SLOT" or (equipped and "EQUIPPED IN OTHER SLOT" or "CLICK TO EQUIP"))
        UI.SetColor(id.."Status",selected and 0.97 or 0.54,selected and 0.66 or 0.64,selected and 0.28 or 0.65,1)
    end
end
local function openLoadout()
    loadoutSlot="primary"
    restoreLoadout()
    pendingScreen="loadout"
end
local function selectGun(id)
    if loadoutSlot=="primary" then
        if id==loadoutSecondary then loadoutSecondary=loadoutPrimary end
        loadoutPrimary=id
    else
        if id==loadoutPrimary then loadoutPrimary=loadoutSecondary end
        loadoutSecondary=id
    end
    refreshLoadout()
end
function OnLoadoutClicked() openLoadout() end
function OnPrimarySlotClicked() loadoutSlot="primary";refreshLoadout() end
function OnSecondarySlotClicked() loadoutSlot="secondary";refreshLoadout() end
function OnPistolSelected() selectGun("pistol") end
function OnRifleSelected() selectGun("rifle") end
function OnShotgunSelected() selectGun("shotgun") end
function OnSMGSelected() selectGun("smg") end
function OnLoadoutSaved()
    Preferences.SaveString("breakbulk_primary",loadoutPrimary)
    Preferences.SaveString("breakbulk_secondary",loadoutSecondary)
    restoreLoadout()
    menuStatus="LOADOUT SAVED / "..guns[loadoutPrimary].name.." + "..guns[loadoutSecondary].name
    pendingScreen="main"
end
function OnLoadoutBack()
    restoreLoadout()
    menuStatus="LOADOUT CHANGES NOT SAVED"
    pendingScreen="main"
end

local joinElapsed=0
function OnCreate()
    restoreLoadout()
    UI.Load("Assets/UI/MainMenu.ui")
    Input.SetCursorVisible(true)
    Scene.SetPaused(false)
    if State.GetBool("breakbulk_disconnected",false) then UI.SetText("NetworkStatus","SESSION ENDED. CREATE OR JOIN A NEW DUEL.");State.SetBool("breakbulk_disconnected",false) end
    if Network.IsConnected() then Network.Disconnect() end
end
function OnPracticeClicked()
    if joining then Network.Disconnect();joining=false end
    Input.SetCursorVisible(false)
    Scene.Load("Assets/Scenes/TerminalDrill.scene")
end
function OnHostClicked()
    joining=false
    if Network.Host(7777,2) then
        Input.SetCursorVisible(false)
        Scene.Load("Assets/Scenes/Arena.scene")
    else UI.SetText("NetworkStatus",Network.GetLastError()) end
end
function OnJoinClicked()
    if joining then return end
    local address=UI.GetText("AddressField"):gsub("%s","")
    if address=="" then address="127.0.0.1" end
    if Network.Join(address,7777) then
        joining=true;joinElapsed=0
        UI.SetText("NetworkStatus","CONTACTING "..address.." ...")
    else UI.SetText("NetworkStatus",Network.GetLastError()) end
end
function OnUpdate(dt)
    if pendingScreen then
        local nextScreen=pendingScreen
        pendingScreen=nil
        if nextScreen=="loadout" then
            UI.Load("Assets/UI/Loadout.ui")
            refreshLoadout()
        else
            UI.Load("Assets/UI/MainMenu.ui")
            UI.SetText("NetworkStatus",menuStatus)
        end
    end
    if not joining then return end
    joinElapsed=joinElapsed+dt
    if Network.IsReady() and Network.IsConnected() then
        Input.SetCursorVisible(false);Scene.Load("Assets/Scenes/Arena.scene");return
    end
    if not Network.IsConnected() or joinElapsed>12 then
        local reason=Network.GetLastError()
        if reason=="" then reason="NO RESPONSE. CHECK ADDRESS AND UDP 7777." end
        Network.Disconnect();joining=false;UI.SetText("NetworkStatus",reason)
    end
end
