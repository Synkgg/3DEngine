local Sim=require('Scripts.Simulation')
local nodes=require('Scripts.Nodes')
local function integer(v)return math.floor(v)end
local s,online,authority,id,camera,paused,guide
local sendTimer,actionSeq,noticeTime,lastSnapshot=0,0,0,0
local pawns,nodeEntities={},{}
local fireEntity,signalEntity
local notice='Gather supplies before nightfall. Tab opens the field guide.'
local function say(t) notice=t;noticeTime=3 end
function OnCreate()
 online=State.GetBool('ember_online',false);authority=not online or Network.IsHost();id=online and Controller.GetLocalID() or 1
 s=Sim.new();Sim.add(s,id);paused=false;guide=false;sendTimer=0;actionSeq=0;lastSnapshot=0;noticeTime=8
 UI.Load('Assets/UI/HUD.ui');Input.SetCursorVisible(false)
 Controller.Possess(self.id,id);camera=Scene.FindEntity('FirstPersonCamera').id;Camera.SetActive(camera)
 transform.SetPosition(id==1 and 0 or 2,1.2,5)
 for i=1,#nodes do nodeEntities[i]=Scene.FindEntity('Node'..i).id end
 fireEntity=Scene.FindEntity('FireGlow').id;signalEntity=Scene.FindEntity('RescueFlag').id
 Graphics.SetFog(true);Graphics.SetFogDensity(.009)
end
function OnResume() paused=false;UI.SetVisible('Pause',false);Input.SetCursorVisible(false) end
function OnExit() Network.Disconnect();Scene.Load('Assets/Scenes/MainMenu.scene') end
local function action(name,positions)
 actionSeq=actionSeq+1
 if authority then
  say(Sim.action(s,id,actionSeq,name,positions[id]) and 'Done.' or 'Get closer or gather the required supplies.')
 else Network.SendMessage(41,actionSeq..':'..name);say('Action sent to camp.') end
end
function OnUpdate(dt)
 dt=math.min(dt,.1)
 if online and not Network.IsConnected() then UI.SetText('Notice','Connection lost. Escape to leave camp.');UI.SetVisible('Pause',true);Input.SetCursorVisible(true);CharacterController.Move(0,0);return end
 local positions={[id]=transform.GetPosition()};local remotes=Network.GetRemoteTransforms();local seen={}
 for _,p in ipairs(remotes)do
  positions[p.playerID]=p;seen[p.playerID]=true
  if not pawns[p.playerID] then pawns[p.playerID]=Scene.InstantiatePrefab('Assets/Prefabs/Survivor.prefab',0) end
  Scene.SetPosition(pawns[p.playerID],p.x,p.y-.9,p.z);Scene.SetRotation(pawns[p.playerID],0,p.ry,0)
  if authority then Sim.add(s,p.playerID) end
 end
 for other,pawn in pairs(pawns)do if not seen[other] then Scene.DestroyEntity(pawn);pawns[other]=nil;if authority then s.players[other]=nil;s.last[other]=nil;s.actions[other]=nil end end end
 for _,m in ipairs(Network.ConsumeMessages())do
  if authority and m.channel==41 and positions[m.senderID] then
   local seq,name=m.payload:match('^(%d+):(%a+)$');if seq then Sim.action(s,m.senderID,tonumber(seq),name,positions[m.senderID]) end
  elseif not authority and m.channel==42 and m.senderID==1 then local snapshot=Sim.decode(m.payload,s.seq);if snapshot then s=snapshot;lastSnapshot=0 end end
 end
 lastSnapshot=lastSnapshot+dt
 if Input.IsKeyPressed('Escape') then paused=not paused;UI.SetVisible('Pause',paused);Input.SetCursorVisible(paused) end
 if Input.IsKeyPressed('Tab') then guide=not guide;UI.SetVisible('RecipePanel',guide) end
 if authority and (online or not paused) then Sim.tick(s,dt,positions) end
 local v=s.players[id] or {health=0,hunger=0,water=0,axe=0,coat=0}
 if not paused and s.result==0 and v.health>0 then
  Camera.RotateEntity(camera,Input.GetMouseDeltaX()*.003,-Input.GetMouseDeltaY()*.003)
  local f,r=Camera.GetForward(),Camera.GetRight();local x,z=0,0
  if Input.IsKeyDown('W')then x=x+f.x;z=z+f.z end;if Input.IsKeyDown('S')then x=x-f.x;z=z-f.z end
  if Input.IsKeyDown('D')then x=x+r.x;z=z+r.z end;if Input.IsKeyDown('A')then x=x-r.x;z=z-r.z end
  local len=math.sqrt(x*x+z*z);local speed=Input.IsKeyDown('Left Shift') and 7 or 4.5
  if len>0 then CharacterController.Move(x/len*speed,z/len*speed)else CharacterController.Move(0,0)end
  if Input.IsKeyPressed('Space')then CharacterController.Jump()end
  for key,name in pairs({E='use',F='eat',Q='cook',['1']='axe',['2']='coat',R='signal',V='revive'})do if Input.IsKeyPressed(key)then action(name,positions)end end
 else CharacterController.Move(0,0) end
 sendTimer=sendTimer+dt
 if online and sendTimer>=.1 then
  sendTimer=0;local p=transform.GetPosition();local f=Camera.GetForward();Network.SendTransform(p.x,p.y,p.z,0,math.atan(f.x,f.z),0)
  if authority then Network.SendMessage(42,Sim.encode(s))end
 end
 for i,e in ipairs(nodeEntities)do local ready=(s.cooldowns[i] or 0)<=s.time;Scene.SetScale(e,ready and 1 or .12,ready and 1 or .12,ready and 1 or .12)end
 local flame=s.fire>0 and (.8+.15*math.sin(s.time*9)) or 0
 Scene.SetScale(fireEntity,flame,flame,flame);Scene.SetScale(signalEntity,s.signal,s.signal,s.signal)
 local night=s.time%180>=110;Graphics.SetExposure(night and .32 or 1)
 local target=Sim.target(s,positions[id]);local prompts={wood='E  Gather wood',stone='E  Gather stone',food='E  Gather berries',water='E  Drink spring water',fire='E  Fuel fire   /   Q  Cook meal',signal='R  Build rescue signal (12 wood + 8 stone)'}
 UI.SetText('Prompt',prompts[target] or '');noticeTime=noticeTime-dt
 UI.SetText('Notice',noticeTime>0 and notice or (not authority and lastSnapshot>3 and 'Waiting for camp connection...' or ''))
 UI.SetText('Objective',string.format('DAY %d  /  %s     FIRE %d%%     %s',math.floor(s.time/180)+1,night and 'NIGHT' or 'DAYLIGHT',integer(s.fire),s.signal==1 and 'SIGNAL READY - SURVIVE TO DAY 4' or 'BUILD THE RESCUE SIGNAL'))
 UI.SetText('Vitals',string.format('HEALTH %d    FOOD %d    WATER %d',integer(v.health),integer(v.hunger),integer(v.water)))
 UI.SetText('Inventory',string.format('WOOD %d    STONE %d    BERRIES %d     %s  %s     F eat / V revive / Tab guide',integer(s.wood),integer(s.stone),integer(s.food),v.axe==1 and 'AXE' or '',v.coat==1 and 'COAT' or ''))
 if v.health<=0 then UI.SetText('Notice','DOWNED - your partner can revive you with V and 2 berries.') end
 if s.result~=0 then UI.SetText('Notice',s.result==1 and 'RESCUED. You made it home. Escape to start a new expedition.' or 'THE WILDERNESS WON. Escape to try again.');UI.SetText('Prompt','')end
end
function OnDestroy() Input.SetCursorVisible(true) end

