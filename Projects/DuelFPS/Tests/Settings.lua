local Settings=require('Scripts.Settings')
local saved={};local values,text,visible={},{},{};local keys,mouse={},{}
local nop=function()end
local api={Preferences={LoadString=function(k,d)return saved[k] or d end,SaveString=function(k,v)saved[k]=v;return true end},State={SetNumber=nop,SetBool=nop},Audio={SetMasterVolume=function(v)values.master=v end,SetSFXVolume=nop,SetUIVolume=nop},Input={IsKeyDown=function(k)return keys[k] or false end,IsKeyPressed=function(k)return keys[k] or false end,IsMouseButtonDown=function(k)return mouse[k] or false end,IsMouseButtonPressed=function(k)return mouse[k] or false end,SetCursorVisible=nop,IsValidBinding=function()return true end,SetInteractionBinding=function(k)values.interact=k end},UI={Load=nop,SetVisible=function(k,v)visible[k]=v end,SetColor=nop,SetText=function(k,v)text[k]=v end,SetValue=function(k,v)values[k]=v end,GetValue=function(k)return values[k]end}}
local closed=0;local s=Settings.new(api,function()closed=closed+1 end)
api.OnSettingsClicked();assert(s:tick(.01) and s.open)
values.Setting_master=.25;s:tick(.01);assert(values.master==.25,'audio preview applies')
s:rebind('forward','S');assert(s.draft.bind_forward=='S' and s.draft.bind_back=='W','conflicts swap')
assert(not s:rebind('jump','Escape'),'escape reserved')
s:rebind('reload','Mouse 4');api.OnSettingsApply();s:tick(.01);assert(closed==1 and not s.open)
local restored=Settings.new(api,nop);assert(restored.values.master==.25 and restored:binding('forward')=='S' and restored:binding('reload')=='Mouse 4','preferences survive fresh settings instance')
keys.S=true;assert(restored:down('forward'));keys.S=false;mouse[4]=true;assert(restored:pressed('reload'));mouse[4]=false
api.OnSettingsClicked();restored:tick(.01);values.Setting_master=.9;restored:tick(.01);api.OnSettingsCancel();restored:tick(.01);assert(restored.values.master==.25 and values.master==.25,'cancel restores audio and config')
api.OnSettingsClicked();restored:tick(.01);api.OnSettingsReset();restored:tick(.01);assert(restored.draft.bind_forward=='W');api.OnSettingsCancel();restored:tick(.01);assert(restored.values.bind_forward=='S','reset is cancelable')
api.OnSettingsClicked();restored:tick(.01);api.Preferences.SaveString=function()return false end;api.OnSettingsApply();restored:tick(.01);assert(restored.open and restored.saveError,'save error does not silently discard settings')
print('PASS: settings preview, persistence, cancel/reset, duplicate binding swaps, keyboard/mouse actions and save failure')
