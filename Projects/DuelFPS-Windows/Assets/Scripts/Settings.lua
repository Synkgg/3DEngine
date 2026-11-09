-- One shared implementation for lobby and in-game settings.
local S={};S.__index=S
S.actions={{'forward','Forward','W'},{'back','Backward','S'},{'left','Strafe left','A'},{'right','Strafe right','D'},{'sprint','Sprint','Left Shift'},{'jump','Jump','Space'},{'fire','Fire','Mouse 1'},{'aim','Aim','Mouse 3'},{'reload','Reload','R'},{'primary','Primary weapon','1'},{'secondary','Secondary weapon','2'},{'interact','Interact','E'},{'loadout','Open loadout','L'},{'crouch','Crouch','Left Ctrl'}}
S.ranges={sensitivity={.001,.025,.01},ads={.25,1.5,1},fov={70,110,90},master={0,1,1},sfx={0,1,1},ui={0,1,1},distance={100,1000,1000},brightness={.6,1.5,1},quality={1,3,3},shadow={0,3,2},aa={0,1,1},bloom={0,1,1},fog={0,1,0},invert={0,1,0}}
S.sliders={'sensitivity','ads','fov','master','sfx','ui','distance','brightness'}
local function copy(t)local r={};for k,v in pairs(t)do r[k]=v end;return r end
local function hex(s)return (s:gsub('.',function(c)return string.format('%02x',string.byte(c))end))end
local function unhex(s)if #s%2~=0 or s:find('[^%x]') then return '' end;return (s:gsub('..',function(c)return string.char(tonumber(c,16))end))end
function S.defaults()local t={};for k,r in pairs(S.ranges)do t[k]=r[3]end;for _,a in ipairs(S.actions)do t['bind_'..a[1]]=a[3]end;return t end
function S.serialize(t)local fields={};for k,v in pairs(t)do fields[#fields+1]=k..'='..(type(v)=='string' and hex(v) or tostring(v))end;table.sort(fields);return table.concat(fields,';')end
function S.new(api,onClose)
 local self=setmetatable({api=api,onClose=onClose,values=S.defaults(),open=false,pending=nil,capture=nil,tab='Controls'},S)
 local raw=api.Preferences.LoadString('duel_settings_v1','')
 for k,v in raw:gmatch('([%w_]+)=([^;]*)')do
  if S.ranges[k]then local n=tonumber(v);if n and n==n and math.abs(n)<1e6 then self.values[k]=math.max(S.ranges[k][1],math.min(S.ranges[k][2],n))end
  elseif k:sub(1,5)=='bind_' and self.values[k] then local b=unhex(v);if b~='Escape' and b~='' and (not api.Input.IsValidBinding or api.Input.IsValidBinding(b))then self.values[k]=b end end
 end
 if raw=='' then self.values.sensitivity=math.max(.001,math.min(.025,tonumber(api.Preferences.LoadString('mouse_sensitivity','.01')) or .01));self.values.invert=api.Preferences.LoadString('invert_y','0')=='1' and 1 or 0 end
 for _,k in ipairs({'quality','shadow','aa','bloom','fog','invert'})do self.values[k]=math.floor(self.values[k]+.5)end
 -- Corrupt/hand-edited duplicate bindings are reset as a group.
 local seen={};for _,a in ipairs(S.actions)do local b=self.values['bind_'..a[1]];if seen[b]then for _,d in ipairs(S.actions)do self.values['bind_'..d[1]]=d[3]end;break end;seen[b]=true end
 api.OnSettingsClicked=function()self.pending='open'end
 api.OnSettingsApply=function()self.pending='apply'end
 api.OnSettingsCancel=function()self.pending='cancel'end
 api.OnSettingsReset=function()self.pending='reset'end
 api.OnSettingsAudioTest=function()if api.Audio.PlaySFX then api.Audio.PlaySFX('Assets/Audio/Breakbulk/reload.wav',.6)end end
 for _,tab in ipairs({'Controls','Audio','Graphics'})do api['OnSettings'..tab]=function()self.nextTab=tab end end
 for _,k in ipairs({'invert','quality','shadow','aa','bloom','fog'})do api['OnToggle_'..k]=function()self.toggle=k end end
 for _,a in ipairs(S.actions)do local id=a[1];api['OnBind_'..id]=function()self.capture=id;self.captureDelay=.2;self.captureRemaining=10 end end
 self:applyLive(self.values);return self
end
function S:applyLive(t)
 local a,g=self.api.Audio,self.api.Graphics or {}
 for key,method in pairs({master='SetMasterVolume',sfx='SetSFXVolume',ui='SetUIVolume'})do if a[method]then a[method](t[key])end end
 if g.SetQualityPreset then g.SetQualityPreset(t.quality)end
 if g.SetShadowQuality then g.SetShadowQuality(t.shadow);g.SetAntiAliasingSamples(t.aa==1 and 4 or 1);g.SetBloom(t.bloom==1);g.SetFog(t.fog==1);g.SetViewDistance(t.distance);g.SetExposure(.78*t.brightness)end
 if self.api.Input.SetInteractionBinding then self.api.Input.SetInteractionBinding(t.bind_interact)end
 self.api.State.SetNumber('mouse_sensitivity',t.sensitivity);self.api.State.SetBool('invert_y',t.invert==1)
end
function S:binding(action)return self.values['bind_'..action]end
function S:down(action)
 local b=self:binding(action);local m=b:match('^Mouse ([1-5])$');return m and self.api.Input.IsMouseButtonDown(tonumber(m)) or (not m and self.api.Input.IsKeyDown(b))
end
function S:pressed(action)
 local b=self:binding(action);local m=b:match('^Mouse ([1-5])$');if m then return self.api.Input.IsMouseButtonPressed and self.api.Input.IsMouseButtonPressed(tonumber(m)) or false end;return self.api.Input.IsKeyPressed(b)
end
function S:rebind(action,binding)
 if binding=='Escape' then return false end
 local key='bind_'..action;local old=self.draft[key]
 for _,a in ipairs(S.actions)do local other='bind_'..a[1];if other~=key and self.draft[other]==binding then self.draft[other]=old end end
 self.draft[key]=binding;return true
end
function S:refresh()
 local ui,t=self.api.UI,self.draft
 for _,tab in ipairs({'Controls','Audio','Graphics'})do ui.SetVisible('Settings'..tab,self.tab==tab);ui.SetVisible('TabIndicator'..tab,self.tab==tab);ui.SetColor('Tab'..tab..'Button',self.tab==tab and .28 or .1,self.tab==tab and .31 or .12,self.tab==tab and .32 or .14,1)end
 for _,key in ipairs(S.sliders)do local r=S.ranges[key];ui.SetValue('Setting_'..key,(t[key]-r[1])/(r[2]-r[1]))end
 for _,a in ipairs(S.actions)do local b=t['bind_'..a[1]];local label=({['Mouse 1']='Left mouse',['Mouse 2']='Middle mouse',['Mouse 3']='Right mouse',['Mouse 4']='Mouse side 1',['Mouse 5']='Mouse side 2'})[b] or b;ui.SetText('Bind_'..a[1]..'Label',label)end
 for _,k in ipairs({'invert','aa','bloom','fog'})do ui.SetText('Toggle_'..k..'Label',t[k]==1 and 'On' or 'Off')end
 ui.SetText('Toggle_qualityLabel',({'Low','Medium','High'})[t.quality]);ui.SetText('Toggle_shadowLabel',({'Off','Low','Medium','High'})[t.shadow+1])
end
function S:tick(dt)
 local ui=self.api.UI
 if self.pending=='open' then self.pending=nil;self.open=true;self.draft=copy(self.values);self.tab='Controls';self.capture=nil;ui.Load(self.path or 'Assets/UI/SettingsLobby.ui');self:refresh();self.api.Input.SetCursorVisible(true);return true end
 if not self.open then return false end
 for _,key in ipairs(S.sliders)do local r=S.ranges[key];self.draft[key]=r[1]+ui.GetValue('Setting_'..key)*(r[2]-r[1]) end
 if self.nextTab then self.tab=self.nextTab;self.nextTab=nil;self.capture=nil;self:refresh()end
 if self.toggle then
  local k=self.toggle;self.toggle=nil;local r=S.ranges[k];self.draft[k]=self.draft[k]>=r[2] and r[1] or self.draft[k]+1
  if k=='quality'then local q=self.draft.quality;self.draft.shadow=q==1 and 0 or q-1;self.draft.aa=q==1 and 0 or 1;self.draft.bloom=q==1 and 0 or 1;self.draft.distance=q==1 and 250 or (q==2 and 500 or 1000)end
  self:refresh()
 end
 if self.capture then
  self.captureDelay=self.captureDelay-dt;self.captureRemaining=self.captureRemaining-dt
  local key=self.api.Input.IsKeyPressed('Escape') and 'Escape' or (self.captureDelay<=0 and self.api.Input.GetPressedBinding and self.api.Input.GetPressedBinding() or '')
  if key=='Escape' or self.captureRemaining<=0 then self.capture=nil
  elseif key~='' then self:rebind(self.capture,key);self.capture=nil;self:refresh()end
 elseif self.api.Input.IsKeyPressed('Escape') then self.pending='cancel'end
 if self.pending=='reset'then self.draft=S.defaults();self.pending=nil;self.capture=nil;self:refresh()end
 local signature=S.serialize(self.draft);if signature~=self.preview then self:applyLive(self.draft);self.preview=signature end
 for _,key in ipairs(S.sliders)do
  local v=self.draft[key];local text
  if key=='sensitivity'then text=string.format('%.2f',v*1000)
  elseif key=='ads'then text=string.format('%.2fx',v)
  elseif key=='fov' or key=='distance'then text=tostring(math.floor(v+.5))
  else text=tostring(math.floor(v*100+.5))..'%'end
  ui.SetText('Value_'..key,text)
 end
 ui.SetText('SettingsStatus',self.capture and ('Press a key or mouse button for '..self.capture..'. Escape cancels.') or (self.saveError or 'Changes preview immediately. Apply to save; Cancel to revert.'))
 if self.pending=='apply' or self.pending=='cancel'then
  local apply=self.pending=='apply';self.pending=nil;self.capture=nil
  if apply and not self.api.Preferences.SaveString('duel_settings_v1',S.serialize(self.draft))then self.saveError='Could not save settings. Check write access to the Saved folder.';return true end
  if apply then self.values=copy(self.draft)else self:applyLive(self.values)end
  self.open=false;self.preview=nil;self.saveError=nil;self.onClose();return true
 end
 return true
end
return S
