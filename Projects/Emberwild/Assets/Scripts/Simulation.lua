-- Pure host-owned simulation; shared inventory encourages cooperation.
local M={}
local nodes=require('Scripts.Nodes')
local function near(p,x,z,r)return p and (p.x-x)^2+(p.z-z)^2<r*r end
function M.new()
 return {time=0,fire=65,wood=0,stone=0,food=0,signal=0,result=0,seq=0,players={},cooldowns={},last={},actions={}}
end
function M.add(s,id) if not s.players[id] then s.players[id]={health=100,hunger=100,water=100,axe=0,coat=0} end end
function M.target(s,p)
 if near(p,0,0,3) then return 'fire',0 end
 if near(p,-5,0,3) then return 'signal',0 end
 if near(p,12,0,5) then return 'water',0 end
 for i,n in ipairs(nodes) do if (s.cooldowns[i] or 0)<=s.time and near(p,n.x,n.z,2.8) then return n.kind,i end end
 return '',0
end
function M.action(s,id,seq,action,p)
 local v=s.players[id]
 if not v or s.result~=0 or seq<=(s.last[id] or 0) then return false end
 s.last[id]=seq
 if v.health<=0 or s.time<(s.actions[id] or -1)+.25 then return false end
 s.actions[id]=s.time
 local target,index=M.target(s,p)
 if action=='use' and index>0 then
  s[target]=math.min(999,s[target]+(v.axe==1 and target~='food' and 4 or 2));s.cooldowns[index]=s.time+55;return true
 elseif action=='use' and target=='water' then v.water=100;return true
 elseif action=='use' and target=='fire' and s.wood>0 then s.wood=s.wood-1;s.fire=math.min(100,s.fire+24);return true
 elseif action=='eat' and s.food>0 then s.food=s.food-1;v.hunger=math.min(100,v.hunger+22);return true
 elseif action=='cook' and target=='fire' and s.fire>0 and s.food>0 and s.wood>0 then s.food=s.food-1;s.wood=s.wood-1;v.hunger=math.min(100,v.hunger+60);v.health=math.min(100,v.health+15);return true
 elseif action=='axe' and v.axe==0 and s.wood>=3 and s.stone>=2 then s.wood=s.wood-3;s.stone=s.stone-2;v.axe=1;return true
 elseif action=='coat' and v.coat==0 and s.wood>=4 and s.food>=3 then s.wood=s.wood-4;s.food=s.food-3;v.coat=1;return true
 elseif action=='signal' and target=='signal' and s.signal==0 and s.wood>=12 and s.stone>=8 then s.wood=s.wood-12;s.stone=s.stone-8;s.signal=1;return true
 elseif action=='revive' and s.food>=2 then
  for other,w in pairs(s.players) do
   if other~=id and w.health<=0 and s.positions and s.positions[other] and near(p,s.positions[other].x,s.positions[other].z,3) then
    s.food=s.food-2;w.health=45;w.hunger=50;w.water=50;return true
   end
  end
 end
 return false
end
function M.tick(s,dt,positions)
 if s.result~=0 then return end
 s.time=s.time+dt;s.positions=positions;s.fire=math.max(0,s.fire-dt*.22)
 local alive=0;local night=s.time%180>=110
 for id,v in pairs(s.players) do
  if positions[id] and v.health>0 then
   v.hunger=math.max(0,v.hunger-dt*.15);v.water=math.max(0,v.water-dt*.23)
   local warm=near(positions[id],0,0,6) and s.fire>0
   local damage=(v.hunger<=0 and 2 or 0)+(v.water<=0 and 3 or 0)+(night and not warm and (v.coat==1 and .12 or .65) or 0)
   v.health=math.max(0,math.min(100,v.health-dt*damage+(warm and damage==0 and dt*.4 or 0)))
   if v.health>0 then alive=alive+1 end
  end
 end
 if alive==0 then s.result=2 elseif s.time>=540 and s.signal==1 and s.fire>=20 then s.result=1 end
end
-- Full, sequenced snapshots recover automatically after UDP loss and late joins.
function M.encode(s)
 s.seq=s.seq+1
 local a={s.seq,s.time,s.fire,s.wood,s.stone,s.food,s.signal,s.result}
 for id=1,2 do local v=s.players[id] or {health=0,hunger=0,water=0,axe=0,coat=0};for _,k in ipairs({'health','hunger','water','axe','coat'})do a[#a+1]=v[k]end end
 for i=1,#nodes do a[#a+1]=math.ceil(math.max(0,(s.cooldowns[i] or 0)-s.time)) end
 for i,v in ipairs(a)do a[i]=tostring(math.floor(v))end
 return table.concat(a,',')
end
function M.decode(payload,previous)
 if #payload>900 then return nil end
 local a={};for token in payload:gmatch('[^,]+')do local n=tonumber(token);if not n or n~=n or math.abs(n)>1e8 then return nil end;a[#a+1]=n end
 if #a~=18+#nodes or a[1]<=previous then return nil end
 local s=M.new();s.seq=a[1];s.time=a[2];s.fire=a[3];s.wood=a[4];s.stone=a[5];s.food=a[6];s.signal=a[7];s.result=a[8]
 local j=9;for id=1,2 do M.add(s,id);for _,k in ipairs({'health','hunger','water','axe','coat'})do s.players[id][k]=a[j];j=j+1 end end
 for i=1,#nodes do s.cooldowns[i]=s.time+a[j];j=j+1 end;return s
end
return M
