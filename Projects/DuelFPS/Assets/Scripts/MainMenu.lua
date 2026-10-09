local Profiles = require("Scripts.Weapons.LoadoutProfiles")
local joining=false
local joinElapsed=0
local browserOpen=false
local browserJoinPending=false
local browserRows={}
local browserRefreshElapsed=0
local browserLastSignature=""
local maxBrowserRows=6

local loadoutSlot="primary"
local loadoutIndex=1
local selectedCategory="rifle"
local pendingScreen=nil
local menuStatus=""
local loadoutPrimary="rifle"
local loadoutSecondary="pistol"
local valid={pistol=true,rifle=true,shotgun=true,smg=true}
local guns={
    pistol={
        name="MAKO P12",role="HANDGUN  /  SEMI-AUTOMATIC",
        desc="Fast handling and clean follow-up shots. Built for the clutch.",
        trait="PRECISION / QUICK DRAW / RELIABLE",index="04 / 04",
        damage=7,range=4,control=8,mobility=10
    },
    rifle={
        name="KESTREL AR4",role="ASSAULT RIFLE  /  AUTOMATIC",
        desc="Adaptable modular carbine for medium-range control.",
        trait="STABLE / VERSATILE / MODULAR",index="01 / 04",
        damage=7,range=8,control=7,mobility=6
    },
    shotgun={
        name="BREACH S8",role="SHOTGUN  /  PUMP ACTION",
        desc="Close-quarters stopping power. Own the doorway.",
        trait="BREACH / CLOSE RANGE / HIGH IMPACT",index="03 / 04",
        damage=10,range=3,control=4,mobility=5
    },
    smg={
        name="VECTOR K9",role="SUBMACHINE GUN  /  AUTOMATIC",
        desc="Aggressive mobility with blistering close-range fire.",
        trait="RUSH / HIP FIRE / FAST HANDLING",index="02 / 04",
        damage=5,range=5,control=6,mobility=9
    }
}
local gunOrder={"rifle","smg","shotgun","pistol"}
local function setPlate(name,active)
    UI.SetColor(name,active and .19 or .075,active and .22 or .093,active and .22 or .105,1)
end
local function previewWeapon(id)
    local gun=guns[id]
    UI.SetText("PreviewName",gun.name)
    UI.SetText("PreviewClass",gun.role)
    UI.SetText("PreviewDesc",gun.desc)
    UI.SetText("PreviewTrait",gun.trait)
    UI.SetText("PreviewIndex",gun.index)
    for _,other in ipairs(gunOrder) do
        UI.SetVisible("PreviewGun_"..other,other==id)
    end
    for _,stat in ipairs({"Damage","Range","Control","Mobility"}) do
        local value=gun[string.lower(stat)]
        for segment=1,10 do
            UI.SetVisible("Stat"..stat.."Segment"..segment,segment<=value)
        end
    end
end

local function restoreLoadout()
    loadoutIndex,loadoutPrimary,loadoutSecondary=Profiles.GetActive(Preferences)
    selectedCategory=loadoutPrimary
end
local function refreshLoadout()
    local selectedID=loadoutSlot=="primary" and loadoutPrimary or loadoutSecondary
    UI.SetText("SlotsTitle","CUSTOM LOADOUT "..loadoutIndex)
    UI.SetText("PrimaryValue",guns[loadoutPrimary].name)
    UI.SetText("SecondaryValue",guns[loadoutSecondary].name)
    UI.SetText("SlotHint",loadoutSlot=="primary" and "CHOOSE A PRIMARY WEAPON" or "CHOOSE A SECONDARY WEAPON")
    UI.SetText("PrimaryTabLabel",loadoutSlot=="primary" and "01  PRIMARY / EDITING" or "01  PRIMARY")
    UI.SetText("SecondaryTabLabel",loadoutSlot=="secondary" and "02  SECONDARY / EDITING" or "02  SECONDARY")
    setPlate("PrimaryPlate",loadoutSlot=="primary")
    setPlate("SecondaryPlate",loadoutSlot=="secondary")
    UI.SetColor("PrimaryPlateAccent",loadoutSlot=="primary" and .96 or .31,loadoutSlot=="primary" and .72 or .37,loadoutSlot=="primary" and .32 or .4,1)
    UI.SetColor("SecondaryPlateAccent",loadoutSlot=="secondary" and .96 or .31,loadoutSlot=="secondary" and .72 or .37,loadoutSlot=="secondary" and .32 or .4,1)
    for _,id in ipairs(gunOrder) do
        local selected=selectedID==id
        local equipped=loadoutPrimary==id or loadoutSecondary==id
        setPlate(id.."CardBack",selected)
        UI.SetColor(id.."CardAccent",selected and .96 or .28,selected and .72 or .36,selected and .32 or .39,1)
        UI.SetText(id.."Status",selected and "SELECTED" or (equipped and "EQUIPPED" or "AVAILABLE"))
        UI.SetColor(id.."Status",selected and .96 or .52,selected and .72 or .59,selected and .32 or .62,1)
    end
    for i=1,Profiles.count do
        local selected=i==loadoutIndex
        UI.SetColor("Profile"..i.."Plate",selected and .62 or .11,selected and .41 or .14,selected and .17 or .15,1)
        UI.SetText("Profile"..i.."ButtonLabel",(selected and "> " or "")..string.format("%02d",i))
    end
    for _,id in ipairs(gunOrder) do
        local selected=id==selectedCategory
        UI.SetColor("Category_"..id.."_Plate",selected and .55 or .10,selected and .38 or .13,selected and .19 or .14,1)
        UI.SetVisible("WeaponCategory_"..id,selected)
    end
    previewWeapon(selectedCategory)
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
    selectedCategory=id
    refreshLoadout()
end
local function chooseProfile(index)
    -- Switching presets keeps edits to the previous slot.
    Profiles.Save(loadoutIndex,loadoutPrimary,loadoutSecondary,Preferences)
    Profiles.Select(index,Preferences)
    restoreLoadout()
    refreshLoadout()
end
function OnProfile1Clicked() chooseProfile(1) end
function OnProfile2Clicked() chooseProfile(2) end
function OnProfile3Clicked() chooseProfile(3) end
function OnProfile4Clicked() chooseProfile(4) end
function OnProfile5Clicked() chooseProfile(5) end
function OnCategoryRifle() selectedCategory="rifle";refreshLoadout() end
function OnCategorySMG() selectedCategory="smg";refreshLoadout() end
function OnCategoryShotgun() selectedCategory="shotgun";refreshLoadout() end
function OnCategoryPistol() selectedCategory="pistol";refreshLoadout() end
function OnLoadoutClicked() openLoadout() end
function OnPrimarySlotClicked() loadoutSlot="primary";refreshLoadout() end
function OnSecondarySlotClicked() loadoutSlot="secondary";refreshLoadout() end
function OnPistolSelected() selectGun("pistol") end
function OnRifleSelected() selectGun("rifle") end
function OnShotgunSelected() selectGun("shotgun") end
function OnSMGSelected() selectGun("smg") end
function OnLoadoutSaved()
    Profiles.Save(loadoutIndex,loadoutPrimary,loadoutSecondary,Preferences)
    Profiles.Select(loadoutIndex,Preferences)
    restoreLoadout()
    menuStatus="CUSTOM "..loadoutIndex.." SAVED / "..guns[loadoutPrimary].name.." + "..guns[loadoutSecondary].name
    pendingScreen="main"
end
function OnLoadoutBack()
    restoreLoadout()
    menuStatus="LOADOUT CHANGES NOT SAVED"
    pendingScreen="main"
end

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
    if Network.Host(7777,2,"BREAKBULK / PRIVATE DUEL") then
        Input.SetCursorVisible(false)
        Scene.Load("Assets/Scenes/Arena.scene")
    else UI.SetText("NetworkStatus",Network.GetLastError()) end
end
local function showBrowserRows()
    local servers=Network.GetServers()
    local signature=tostring(#servers)..":"..tostring(Network.IsSearchingServers())
    for i=1,maxBrowserRows do
        local server=servers[i]
        if server then
            signature=signature..":"..server.address..":"..server.port..":"..server.players
        end
    end
    if signature==browserLastSignature then return end
    browserLastSignature=signature
    browserRows=servers
    for i=1,maxBrowserRows do
        local server=servers[i]
        UI.SetVisible("ServerRow"..i,server~=nil)
        if server then
            UI.SetText("Server"..i.."Title",server.name)
            UI.SetText("Server"..i.."Info",server.address..":"..server.port..
                (server.full and "   /   MATCH FULL" or "   /   OPEN SESSION"))
            UI.SetText("Server"..i.."Players",server.players.." / "..server.maxPlayers)
            UI.SetText("Server"..i.."ButtonLabel",server.full and "FULL" or "JOIN SERVER  >")
        end
    end
    local count=#servers
    if count==0 then
        UI.SetText("BrowserStatus",Network.IsSearchingServers()
            and "SEARCHING LOCAL NETWORK...  NO SESSIONS YET"
            or "NO LAN SERVERS FOUND. HOST A DUEL OR USE DIRECT IP.")
    else
        UI.SetText("BrowserStatus",count.." LOCAL "..(count==1 and "SESSION" or "SESSIONS")..
            " FOUND  /  "..(Network.IsSearchingServers() and "SCANNING..." or "SCAN COMPLETE"))
    end
end

function OnBrowseClicked()
    if joining then return end
    browserJoinPending=false
    browserOpen=true
    pendingScreen="browser"
end

function OnBrowserRefresh()
    if joining then return end
    browserJoinPending=false
    browserLastSignature=""
    local started=Network.SearchServers(7777)
    showBrowserRows()
    if not started then
        UI.SetText("BrowserStatus","SEARCH FAILED / "..Network.GetLastError())
    end
end

function OnBrowserBack()
    if joining then return end
    Network.StopServerSearch()
    browserOpen=false
    browserJoinPending=false
    browserRows={}
    browserLastSignature=""
    menuStatus="READY TO DEPLOY"
    pendingScreen="main"
end

local function joinServer(index)
    if joining then return end
    local server=browserRows[index]
    if not server then return end
    if server.full then
        UI.SetText("BrowserStatus","MATCH IS FULL. CHOOSE ANOTHER SERVER.")
        return
    end
    browserJoinPending=true
    if Network.Join(server.address,server.port) then
        joining=true
        joinElapsed=0
        UI.SetText("BrowserStatus","CONNECTING TO "..server.address..":"..server.port.." ...")
    else
        UI.SetText("BrowserStatus","JOIN FAILED / "..Network.GetLastError())
    end
end
function OnServer1Clicked() joinServer(1) end
function OnServer2Clicked() joinServer(2) end
function OnServer3Clicked() joinServer(3) end
function OnServer4Clicked() joinServer(4) end
function OnServer5Clicked() joinServer(5) end
function OnServer6Clicked() joinServer(6) end

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
        elseif nextScreen=="browser" then
            UI.Load("Assets/UI/ServerBrowser.ui")
            browserRefreshElapsed=0
            browserLastSignature=""
            if Network.SearchServers(7777) then
                showBrowserRows()
            else
                UI.SetText("BrowserStatus","SEARCH FAILED / "..Network.GetLastError())
            end
        else
            UI.Load("Assets/UI/MainMenu.ui")
            UI.SetText("NetworkStatus",menuStatus)
        end
    end
    if browserOpen and not joining and not browserJoinPending then
        browserRefreshElapsed=browserRefreshElapsed+dt
        if browserRefreshElapsed>=0.2 then
            browserRefreshElapsed=0
            showBrowserRows()
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
        Network.Disconnect();joining=false
        UI.SetText(browserOpen and "BrowserStatus" or "NetworkStatus",reason)
    end
end
