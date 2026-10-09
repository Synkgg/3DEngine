-- Execute the shipping scripts against two deterministic engine API adapters.
package.path='Projects/DuelFPS/Assets/?.lua;'..package.path
local function peer(host)
    local e=setmetatable({},{__index=_G})
    local t={messages={},sent={},text={},values={},visible={},pos={[1]={x=0,y=1,z=0}},count=2,fire=false,target=10,nextEntity=10,host=host}
    local nop=function()end
    e.self={id=1};e.State={SetNumber=function(k,v)t.values[k]=v end,GetNumber=function(k,d)return t.values[k] or d end,SetBool=nop,GetBool=function(k,d)return d end}
    e.Preferences={LoadString=function(k,d)return d end}
    e.UI={Load=nop,SetText=function(k,v)t.text[k]=v end,SetValue=nop,SetVisible=function(k,v)t.visible[k]=v end,SetColor=nop}
    e.Input={SetCursorVisible=nop,IsKeyPressed=function()return false end,IsKeyDown=function()return false end,IsMouseButtonDown=function(b)return b==1 and t.fire end,GetMouseDeltaX=function()return 0 end,GetMouseDeltaY=function()return 0 end}
    e.Camera={SetActive=nop,RotateEntity=nop,SetEntityFOV=nop,GetPosition=function()return {x=0,y=1.6,z=0}end,GetForward=function()return {x=0,y=0,z=-1}end,GetRight=function()return {x=1,y=0,z=0}end}
    e.CharacterController={Move=nop,Jump=nop};e.Audio={PlaySFX=nop};e.Debug={DrawLine=nop}
    e.transform={SetPosition=function(x,y,z)t.pos[1]={x=x,y=y,z=z}end,GetPosition=function()return t.pos[1]end}
    e.Scene={FindEntity=function(name)return {id=name=='FirstPersonCamera' and 2 or 0,IsValid=function()return false end}end,
        InstantiatePrefab=function()t.nextEntity=t.nextEntity+1;t.pos[t.nextEntity]={x=0,y=1,z=0};return t.nextEntity end,
        DestroyEntity=nop,SetPosition=function(id,x,y,z)t.pos[id]={x=x,y=y,z=z}end,GetPosition=function(id)return t.pos[id] or {x=0,y=1,z=0}end,
        SetRotation=nop,Load=function(path)t.loaded=path end}
    e.Controller={GetLocalID=function()return host and 1 or 2 end,Possess=nop,IsLocallyControlled=function()return true end,
        GetPlayerStart=function()return {valid=true,x=0,y=1,z=0}end}
    e.Physics={Raycast=function()return {hit=t.target~=0,entityID=t.target,x=0,y=1,z=-2}end}
    e.Network={IsHost=function()return host end,IsConnected=function()return true end,IsReady=function()return true end,
        GetPlayerCount=function()return t.count end,GetLastError=function()return '' end,Disconnect=nop,SendTransform=nop,
        GetRemoteTransforms=function()return {{playerID=host and 2 or 1,x=0,y=1,z=-2,rx=0,ry=0,rz=0}}end,
        SendMessage=function(channel,payload)t.sent[#t.sent+1]={channel=channel,payload=payload,senderID=host and 1 or 2}end,
        ConsumeMessages=function()local m=t.messages;t.messages={};return m end}
    assert(loadfile('Projects/DuelFPS/Assets/Scripts/Player.lua','t',e))();e.OnCreate()
    t.env=e;return t
end
local h,c=peer(true),peer(false)
local function deliver(from,to)for _,m in ipairs(from.sent)do to.messages[#to.messages+1]=m end;from.sent={}end
local function tick(dt)h.env.OnUpdate(dt);deliver(h,c);c.env.OnUpdate(dt);deliver(c,h)end
tick(.1);tick(3.1)
assert(h.text.AmmoText=='30  /  90','reserve ammo display must use both Lua return values')
assert(c.text.AmmoText=='30  /  90','client receives same loadout')
assert(h.text.RoundClock~='00:00','active round has a timer')
-- Two viewmodels are instantiated before the replicated operator (entity 13).
h.target=13;c.target=0
for round=1,5 do
    h.fire=true
    for shot=1,5 do tick(.15) end
    h.fire=false;tick(.1)
    assert(h.values.duel_score==round,'host validates ray hits and awards round exactly once')
    assert(c.values.duel_opponent_score==round,'client score matches host')
    if round<5 then tick(3.1);tick(3.1);assert(h.text.AmmoText=='30  /  90','all weapons replenish each round') end
end
assert(h.visible.MatchResult and c.visible.MatchResult,'both peers show match result')
h.env.OnRematchClicked();assert(h.visible.MatchResult,'one rematch vote must not restart')
c.env.OnRematchClicked();deliver(c,h);tick(.1)
assert(h.values.duel_score==0 and c.values.duel_opponent_score==0,'mutual rematch resets scores')
-- Timeout with equal health is a draw, not a fabricated win.
tick(3.1);tick(91)
assert(h.values.duel_score==0,'tied timeout does not award points')
assert(h.text.RoundResultTitle=='STALEMATE','draw round has correct presentation')
-- Module-level reload, inventory and cooldown regressions.
local W=require('Scripts.Weapons.WeaponSystem')
local w=W.new(h.env);w:Register(require('Scripts.Weapons.Rifle'));w:Give('rifle',90)
assert(w:Fire(1,2));assert(not w:Fire(1,2),'fire rate must be enforced')
w:Reload();w:Update(2,2);local ammo,reserve=w:GetAmmo();assert(ammo==30 and reserve==89,'reload consumes reserve')
print('PASS: BREAKBULK two-peer gameplay, victory, rematch, timeout, loadout and reload')

-- Client-originated fire is resolved by the host; duplicates cannot deal damage twice.
h,c=peer(true),peer(false);tick(.1);tick(3.1)
h.target=1;c.target=13;c.fire=true;tick(.15);c.fire=false;tick(.15)
assert(h.values.duel_health==80,'host resolves client ray using weapon damage')
h.messages[#h.messages+1]={senderID=2,channel=20,payload='SHOT:1:1:rifle:0:1.6:0:0:0:-1'}
tick(.15);assert(h.values.duel_health==80,'duplicate shot sequence must be ignored')
h.target=0;c.fire=true;tick(.15);c.fire=false;tick(.15)
assert(h.values.duel_health==80,'host rejects client hit claims through cover')
print('PASS: host-authoritative client hit, duplicate suppression and cover validation')
