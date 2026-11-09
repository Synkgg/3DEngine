local Settings=require("Scripts.Settings")
local settings
local Profiles = require("Scripts.Weapons.LoadoutProfiles")
local map=1
local arenas={'Arena','Depot'}
local drills={'TerminalDrill','DepotDrill'}
local mapNames={'Terminal','Freight Depot'}
local limits={3,5,7};local times={60,90,120};local limit,time=2,2
local lan=false
local cloud=require('Scripts.CloudHost')
local cloudRequest,cloudRetry
local serverAddress
local quickPlay=false
local teamSize=1
local function modeLabel()
    UI.SetText('ModeLabel',('Mode: %dv%d'):format(teamSize,teamSize))
end
function OnModeClicked()
    if not cloudRequest then teamSize=teamSize%3+1;modeLabel() end
end
local function showSearch(active)
    quickPlay=active
    UI.SetText('PlayLabel',active and 'Cancel search' or 'Play')
    for _,name in ipairs({'CustomButton','DrillButton','LoadoutButton','SettingsButton','ModeButton'}) do
        UI.SetVisible(name,not active)
    end
end
local function customLabels()
    UI.SetText('TransportLabel',lan and 'LAN' or 'DuelFPS server')
    UI.SetText('HostLabel',lan and 'CREATE LAN GAME' or 'CREATE SERVER GAME')
    UI.SetVisible('BrowseButton',lan)
    for _,name in ipairs({'AddressTitle','AddressField','JoinButton'}) do UI.SetVisible(name,not lan) end
end
local function setupLabels()
    UI.SetText('MapLabel',mapNames[map]);UI.SetText('LimitLabel','First to '..limits[limit]);UI.SetText('TimeLabel',times[time]..' seconds')
    UI.SetText('StartLabel',lan and 'START LAN GAME' or 'CREATE SERVER GAME')
    UI.SetVisible('ModeButton',not lan)
    modeLabel()
    UI.SetVisible('MapTerminal',map==1);UI.SetVisible('MapDepot',map==2)
end
function OnMapClicked() map=3-map;setupLabels() end
function OnLimitClicked() limit=limit%3+1;setupLabels() end
function OnTimeClicked() time=time%3+1;setupLabels() end
function OnQuitClicked() Scene.QuitGame() end
local joining=false
function OnTransportClicked()
    if joining then return end
    lan=not lan
    customLabels()
    UI.SetText('NetworkStatus','')
end
local joinElapsed=0
local browserOpen=false
local browserJoinPending=false
local browserRows={}
local browserRefreshElapsed=0
local browserLastSignature=""
local maxBrowserRows=6

local loadoutSlot="primary"
local loadoutIndex=1
local draft={}
local selectedCategory="rifle"
local pendingScreen=nil
local menuStatus=""
local loadoutPrimary="rifle"
local loadoutSecondary="pistol"
local function restoreLoadout()
    loadoutIndex,loadoutPrimary,loadoutSecondary=Profiles.GetActive(Preferences)
    selectedCategory=loadoutPrimary
end
local ArmoryView=require('Scripts.Weapons.ArmoryView')
local picking=false
local function refreshLoadout()
    ArmoryView.refresh(UI,loadoutIndex,loadoutPrimary,loadoutSecondary,loadoutSlot,selectedCategory,picking)
end
function OnPickerBack() picking=false;refreshLoadout() end

local function openLoadout()
    picking=false
    draft={}
    loadoutSlot="primary"
    restoreLoadout()
    pendingScreen="loadout"
end
local function selectGun(id)
    picking=false
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
    loadoutPrimary,loadoutSecondary=Profiles.Browse(draft,loadoutIndex,loadoutPrimary,loadoutSecondary,index,Preferences)
    loadoutIndex=index;selectedCategory=loadoutSlot=='primary' and loadoutPrimary or loadoutSecondary
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
function OnPrimarySlotClicked() picking=true;loadoutSlot="primary";selectedCategory=loadoutPrimary;refreshLoadout() end
function OnSecondarySlotClicked() picking=true;loadoutSlot="secondary";selectedCategory=loadoutSecondary;refreshLoadout() end
function OnPistolSelected() selectGun("pistol") end
function OnRifleSelected() selectGun("rifle") end
function OnShotgunSelected() selectGun("shotgun") end
function OnSMGSelected() selectGun("smg") end
function OnLoadoutSaved()
    Profiles.Apply(draft,loadoutIndex,loadoutPrimary,loadoutSecondary,Preferences)
    restoreLoadout()
    menuStatus="Loadout "..loadoutIndex.." saved."
    pendingScreen="main"
end
function OnLoadoutBack()
    restoreLoadout()
    menuStatus="LOADOUT CHANGES NOT SAVED"
    pendingScreen="main"
end

function OnCreate()
    State.SetNumber('duel_cloud_code',0)
    State.SetBool('duel_public_match',false)
    State.SetNumber('duel_team_size',1)
    serverAddress=State.GetBool('duel_test_loopback',false) and '127.0.0.1' or cloud.address
    settings=Settings.new(_ENV,function() pendingScreen="main" end)
    restoreLoadout()
    UI.Load("Assets/UI/MainMenu.ui")
    Input.SetCursorVisible(true)
    Scene.SetPaused(false)
    if State.GetBool("breakbulk_disconnected",false) then UI.SetText("NetworkStatus","SESSION ENDED. CREATE OR JOIN A NEW DUEL.");State.SetBool("breakbulk_disconnected",false) end
    if Network.IsConnected() then Network.Disconnect() end
end
local function cancelCloud()
    if cloudRequest then
        if quickPlay and cloudRequest~='RULES' and Network.IsReady() then Network.SendMessage(22,'CANCEL') end
        if quickPlay then showSearch(false) end
        Network.Disconnect();cloudRequest=nil;joining=false
        State.SetNumber('duel_cloud_code',0)
    end
end
function OnPlayClicked() cancelCloud();pendingScreen="play" end
function OnPlayBack() cancelCloud();pendingScreen="main" end
function OnHostSetup() if not joining then pendingScreen='host' end end
function OnDrillSetup() if not joining then pendingScreen='drill' end end
local function requestCloud(request)
    if not serverAddress or serverAddress=='' then
        UI.SetText('NetworkStatus','Online play is not configured for this build.')
        return
    end
    if Network.Join(serverAddress,cloud.port) then
        cloudRequest=request;cloudRetry=0;joinElapsed=0;joining=true
        UI.SetText('NetworkStatus','Contacting cloud lobby...')
        if request:match('^PLAY') then showSearch(true) end
    else UI.SetText('NetworkStatus',Network.GetLastError()) end
end
function OnQuickPlay()
    if quickPlay then cancelCloud();UI.SetText('NetworkStatus','Search canceled.');return end
    if not joining then requestCloud(teamSize==1 and 'PLAY' or 'PLAY:'..teamSize) end
end
function OnStartHost()
    if joining then return end
    State.SetNumber('duel_limit',limits[limit]);State.SetNumber('duel_seconds',times[time])
    if not lan then
        local request=('CREATE:%d:%d:%d'):format(map,limits[limit],times[time])
        requestCloud(teamSize==1 and request or request..':'..teamSize)
    else OnHostClicked() end
end
function OnPracticeClicked()
    if joining then Network.Disconnect();joining=false end
    Input.SetCursorVisible(false)
    Scene.Load("Assets/Scenes/"..drills[map]..".scene")
end
function OnHostClicked()
    if not lan or joining then return end
    joining=false
    if Network.Host(7777,2,"BREAKBULK / PRIVATE DUEL") then
        Input.SetCursorVisible(false)
        Scene.Load("Assets/Scenes/"..arenas[map]..".scene")
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
            or "NO LAN GAMES FOUND.")
    else
        UI.SetText("BrowserStatus",count.." LOCAL "..(count==1 and "SESSION" or "SESSIONS")..
            " FOUND  /  "..(Network.IsSearchingServers() and "SCANNING..." or "SCAN COMPLETE"))
    end
end

function OnBrowseClicked()
    if joining or not lan then return end
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
    pendingScreen="play"
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
    if joining or lan then return end
    local code=UI.GetText("AddressField"):gsub("%s","")
    if #code==8 and code:match('^%d+$') then requestCloud('JOIN:'..code)
    else UI.SetText('NetworkStatus','Enter the eight-digit DuelFPS lobby code.') end
end
function OnUpdate(dt)
    if settings:tick(dt) then return end
    if pendingScreen then
        local nextScreen=pendingScreen
        pendingScreen=nil
        if nextScreen=="play" then
            UI.Load("Assets/UI/Play.ui")
            customLabels()
        elseif nextScreen=='host' or nextScreen=='drill' then
            UI.Load(nextScreen=='host' and 'Assets/UI/HostSetup.ui' or 'Assets/UI/DrillSetup.ui');setupLabels()
            if nextScreen=='host' then UI.SetText('Heading',lan and 'LAN game (1v1)' or 'DuelFPS server game') end
        elseif nextScreen=="loadout" then
            UI.Load("Assets/UI/LoadoutLobby.ui")
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
            modeLabel()
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
    if cloudRequest and Network.IsReady() then
        cloudRetry=cloudRetry-dt
        if cloudRetry<=0 then Network.SendMessage(22,cloudRequest);cloudRetry=.3 end
        for _,m in ipairs(Network.ConsumeMessages()) do
            if m.senderID==1 and m.channel==22 then
                local port,code,ticket=m.payload:match('^MATCH:(%d+):(%d+):(%x+)$')
                local slot,id,w,t,size=m.payload:match('^ADMITTED:([1-6]):([12]):([357]):(%d+):([23])$')
                if not slot then
                    slot,id,w,t=m.payload:match('^ADMITTED:([12]):([12]):([357]):(%d+)$')
                    size=1
                end
                if port and #code==8 and #ticket==32 and cloudRequest~='RULES' then
                    if not Network.Join(serverAddress,tonumber(port),ticket) then
                        cancelCloud();UI.SetText('NetworkStatus','Could not join the match');return
                    end
                    State.SetNumber('duel_cloud_code',tonumber(code))
                    State.SetBool('duel_public_match',quickPlay)
                    cloudRequest='RULES';cloudRetry=0;joinElapsed=0
                    return
                elseif quickPlay and cloudRequest~='RULES' and m.payload:sub(1,5)=='WAIT:' then
                    joinElapsed=0
                    UI.SetText('NetworkStatus',m.payload:sub(6))
                elseif slot and cloudRequest=='RULES' and tonumber(slot)==Network.GetLocalPlayerID() then
                    State.SetNumber('duel_limit',tonumber(w));State.SetNumber('duel_seconds',tonumber(t))
                    State.SetNumber('duel_team_size',tonumber(size))
                    cloudRequest=nil;Input.SetCursorVisible(false)
                    Scene.Load('Assets/Scenes/'..arenas[tonumber(id)]..'.scene');return
                elseif m.payload:sub(1,6)=='ERROR:' then
                    cancelCloud();UI.SetText('NetworkStatus',m.payload:sub(7));return
                end
            end
        end
    end
    if not cloudRequest and Network.IsReady() and Network.IsConnected() then
        for _,m in ipairs(Network.ConsumeMessages()) do
            if m.senderID==1 and m.channel==21 then
                local id,w,t=m.payload:match('^([12]):([357]):(%d+)$');t=tonumber(t)
                if id and (t==60 or t==90 or t==120) then
                    State.SetNumber('duel_limit',tonumber(w));State.SetNumber('duel_seconds',t)
                    Input.SetCursorVisible(false);Scene.Load('Assets/Scenes/'..arenas[tonumber(id)]..'.scene');return
                end
            end
        end
    end
    if not Network.IsConnected() or joinElapsed>12 then
        local reason=Network.GetLastError()
        if cloudRequest and reason=="" then reason="Cloud lobby unreachable. Check the server address and UDP ports." end
        if cloudRequest then State.SetNumber('duel_cloud_code',0) end
        if quickPlay then showSearch(false) end
        cloudRequest=nil
        if reason=="" then reason="NO RESPONSE. CHECK ADDRESS AND UDP 7777." end
        Network.Disconnect();joining=false
        UI.SetText(browserOpen and "BrowserStatus" or "NetworkStatus",reason)
    end
end

function OnCategorySniper() selectedCategory="sniper";refreshLoadout() end
function OnSniperSelected() selectGun("sniper") end

function OnCategoryMarksman() selectedCategory="marksman";refreshLoadout() end
function OnMarksmanSelected() selectGun("marksman") end

_ENV["OnBrowse_rifle"]=function() selectedCategory="rifle";refreshLoadout() end

_ENV["OnBrowse_smg"]=function() selectedCategory="smg";refreshLoadout() end

_ENV["OnBrowse_shotgun"]=function() selectedCategory="shotgun";refreshLoadout() end

_ENV["OnBrowse_pistol"]=function() selectedCategory="pistol";refreshLoadout() end

_ENV["OnBrowse_sniper"]=function() selectedCategory="sniper";refreshLoadout() end

_ENV["OnBrowse_marksman"]=function() selectedCategory="marksman";refreshLoadout() end

_ENV["OnBrowse_rifle2"]=function() selectedCategory="rifle2";refreshLoadout() end

_ENV["OnBrowse_smg2"]=function() selectedCategory="smg2";refreshLoadout() end

_ENV["OnBrowse_shotgun2"]=function() selectedCategory="shotgun2";refreshLoadout() end

_ENV["OnBrowse_pistol2"]=function() selectedCategory="pistol2";refreshLoadout() end

_ENV["OnBrowse_sniper2"]=function() selectedCategory="sniper2";refreshLoadout() end

_ENV["OnBrowse_marksman2"]=function() selectedCategory="marksman2";refreshLoadout() end

function OnFalconSelected() selectGun("rifle2") end

function OnWaspSelected() selectGun("smg2") end

function OnHammerSelected() selectGun("shotgun2") end

function OnViperSelected() selectGun("pistol2") end

function OnLongbowSelected() selectGun("sniper2") end

function OnScoutSelected() selectGun("marksman2") end
