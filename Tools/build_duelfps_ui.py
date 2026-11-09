"""Rebuild the compact DuelFPS UI; no gameplay or saved preferences are changed."""
from pathlib import Path
import json
A=Path(__file__).resolve().parents[1]/'Projects/DuelFPS/Assets'
# UI is generated from scratch; IDs are the gameplay API, not reused layouts.
INK=(.025,.035,.043,1); PANEL=(.045,.06,.07,.97); TEXT=(.90,.92,.88,1)
MUTED=(.48,.58,.59,1); ACCENT=(.91,.61,.23,1); TEAL=(.14,.61,.59,1)
def q(s): return json.dumps(s)
class UI:
    def __init__(self): self.rows=['ENGINE_UI 10','1920 1080']
    def w(self,depth,kind,name,x,y,w,h,color=PANEL,z=0,visible=True,hit=False,tail=''):
        row=[depth,kind,q(name),x,y,w,h,0,0,0,0,0,0,*color,int(visible),1,int(hit),z,0,0,0,0,0,0,0,1]
        self.rows.append(' '.join(map(str,row))+(' '+tail if tail else ''))
    def panel(self,name,x,y,w,h,color=PANEL,depth=0,z=0,visible=True):self.w(depth,0,name,x,y,w,h,color,z,visible)
    def text(self,name,text,x,y,w,h,size=22,color=TEXT,depth=0,z=2,align=0):self.w(depth,1,name,x,y,w,h,color,z,tail=f'{q(text)} {size} {align} 1')
    def button(self,name,label,x,y,w,h,callback,script='Player.lua',depth=0,z=5,primary=False):
        normal=ACCENT if primary else (.22,.30,.39,1)
        hover=(.56,.84,1,1) if primary else (.30,.43,.55,1)
        text=INK if primary else TEXT
        tail=' '.join(map(str,[*normal,*hover,.10,.40,.40,1,.07,.08,.09,1,1,*text,*text,*TEXT,*MUTED]))
        tail+=f' "Assets/Audio/Breakbulk/interface.wav" "Assets/Scripts/{script}" {q(callback)} "" "" "" ""'
        self.w(depth,3,name,x,y,w,h,normal,z,hit=True,tail=tail)
        self.text((name.replace('Button','Label') if 'Button' in name else name+'Label'),label,24,0,w-48,h,22,text,depth+1,z+1)
    def save(self,name): (A/'UI'/name).write_text('\n'.join(self.rows)+'\n')
INK=(.12,.17,.23,1);PANEL=(.19,.25,.32,1);TEXT=(.95,.97,1,1);MUTED=(.70,.79,.87,1);ACCENT=(.32,.72,1,1)
def page(title,hint=''):
    u=UI();u.panel('Background',0,0,1920,1080,INK);u.text('Title',title,100,60,1600,85,54);u.text('Subtitle',hint,104,153,1650,45,24,MUTED);return u
def button(u,name,label,x,y,w,callback,script='MainMenu.lua',depth=0,z=5,primary=False):u.button(name,label,x,y,w,64,callback,script,depth,z,primary)
def wrapper(name,source,script):
    (A/'UI'/name).write_text('ENGINE_UI 12\n1920 1080\n0 8 "'+source+'Page" 0 0 1920 1080 0 0 0 0 0 0 0 0 0 0 1 1 0 0 0 0 0 0 0 0 0 1 "Assets/UI/'+source+'.ui" "Assets/Scripts/'+script+'.lua"\n')
u=page('BREAKBULK','A one-on-one duel at the terminal.')
u.text('Hero','One rival. First to five.',104,285,1500,90,48)
for i,(name,label,cb) in enumerate([('PlayButton','PLAY','OnPlayClicked'),('LoadoutButton','LOADOUT','OnLoadoutClicked'),('SettingsButton','SETTINGS','OnSettingsClicked')]):button(u,name,label,160,480+i*94,600,cb,primary=i==0)
u.text('NetworkStatus','',104,895,1650,60,24,MUTED);u.text('Footer','TERMINAL 07  /  1V1',104,990,1400,40,22,MUTED);u.save('MainMenu.ui')
u=page('Loadout','Choose a slot, then choose a weapon.')
for i in range(1,6):button(u,f'Profile{i}Button',f'{i}',104+(i-1)*110,225,90,f'OnProfile{i}Clicked',script='Player.lua')
u.text('SlotsTitle','LOADOUT',740,225,1070,55,25,MUTED)
button(u,'PrimaryTab','PRIMARY',104,325,810,'OnPrimarySlotClicked',script='Player.lua')
u.text('PrimaryValue','',128,400,760,50,32)
button(u,'SecondaryTab','SECONDARY',1000,325,810,'OnSecondarySlotClicked',script='Player.lua')
u.text('SecondaryValue','',1024,400,760,50,32)
u.text('SlotHint','CHOOSE PRIMARY',104,478,1500,45,23,ACCENT)
for i,(id,name,desc,cb) in enumerate([('rifle','KESTREL AR4','Balanced / automatic','OnRifleSelected'),('smg','VECTOR K9','Fast / close range','OnSMGSelected'),('shotgun','BREACH S8','Powerful / pump action','OnShotgunSelected'),('pistol','MAKO P12','Precise / semi-automatic','OnPistolSelected')]):
    x=104+i*432
    u.panel(id+'CardBack',x,550,410,205,PANEL)
    button(u,id+'Button',name,x+14,565,382,cb,script='Player.lua')
    u.text(id+'Description',desc,x+22,646,366,40,22,MUTED)
    u.text(id+'Status','',x+22,702,366,30,20,ACCENT)
u.text('PreviewName','',104,800,1300,50,30);u.text('PreviewDesc','',104,855,1600,40,23,MUTED)
button(u,'BackButton','CANCEL',104,950,300,'OnLoadoutBack',script='Player.lua');button(u,'SaveButton','SAVE LOADOUT',1380,950,430,'OnLoadoutSaved',script='Player.lua',primary=True)
u.save('Loadout.ui');wrapper('LoadoutLobby.ui','Loadout','MainMenu');wrapper('LoadoutInGame.ui','Loadout','Player')
u=page('Local matches','Choose an open match on your local network.')
for i in range(1,7):
    y=240+(i-1)*100;u.panel('ServerRow'+str(i),104,y,1706,86,PANEL,visible=False)
    u.text(f'Server{i}Title','',20,4,770,40,26,depth=1);u.text(f'Server{i}Info','',20,44,1100,34,21,MUTED,depth=1)
    u.text(f'Server{i}Players','',1130,15,170,50,24,depth=1)
    button(u,f'Server{i}Button','JOIN',1370,11,310,f'OnServer{i}Clicked',depth=1)
u.text('BrowserStatus','',104,860,1700,50,24,MUTED)
button(u,'BackButton','BACK',104,950,300,'OnBrowserBack');button(u,'RefreshButton','REFRESH',1380,950,430,'OnBrowserRefresh');u.save('ServerBrowser.ui')
# Sparse HUD: health, ammunition, score and clock; contextual overlays only.
u=UI();u.text('ScoreText','0 / 0',700,35,520,45,30,align=1);u.text('RoundClock','01:30',850,85,220,40,25,align=1)
u.text('MatchStatus','',44,276,560,45,22,MUTED)
u.text('HealthText','100 / 100',80,950,350,55,36);u.text('HealthLabel','HEALTH',80,1010,350,30,19,MUTED)
u.text('AmmoText','30 / 90',1430,950,410,55,38,align=2);u.text('AmmoLabel','',1300,910,540,35,22,MUTED,align=2)
u.text('Slot1Text','',1170,1010,340,30,18,MUTED);u.text('Slot2Text','',1520,1010,320,30,18,MUTED)
for area in ['Terminal','Depot']:
    u.w(0,2,'Radar'+area,44,42,224,224,(1,1,1,.91),1,tail=q('Assets/UI/Art/Radar'+area+'.png'))
u.panel('RadarPlayer',151,149,8,8,(.99,.76,.23,1),z=4)
u.text('Compass','N   000',720,4,480,34,25,TEXT,z=5,align=1)
u.panel('VitalsPlate',55,940,390,96,(.038,.055,.065,.68),z=0)
u.w(0,6,'HealthBar',80,1041,340,8,(.14,.18,.19,.9),4,tail='1 .22 .68 .89 1 0')
u.text('Stance','STANDING',255,992,180,28,17,MUTED,z=5)
u.panel('AmmoPlate',1380,904,485,108,(.038,.055,.065,.68),z=0)
u.panel('DamageFlash',0,0,1920,1080,(.45,.01,.01,.16),z=9,visible=False)
u.panel('CrosshairH',954,539,12,2,TEXT,z=10);u.panel('CrosshairV',959,534,2,12,TEXT,z=10)
u.text('Hitmarker','x',940,519,40,40,25,ACCENT,z=12,align=1)
u.text('CenterMessage','',510,710,900,50,26,align=1);u.text('ReloadText','RELOADING',760,650,400,40,22,ACCENT,align=1)
for group,fields in [('RoundIntro',[('RoundIntroEyebrow','',25),('RoundIntroTitle','3',65),('RoundIntroHint','GET READY',23)]),('RoundResult',[('RoundResultTitle','ROUND WON',48),('RoundResultScore','',34),('RoundResultNext','',23)]),('MatchResult',[('MatchResultTitle','VICTORY',60),('MatchResultScore','',40),('MatchResultHint','',23)])]:
    u.panel(group,510,300,900,290,(.12,.17,.23,.95),z=20,visible=False)
    for i,(name,label,size) in enumerate(fields):u.text(name,label,30,25+i*85,840,75,size,depth=1,z=21,align=1)
u.panel('MatchActions',510,620,900,80,(0,0,0,0),z=28,visible=False)
button(u,'RematchButton','REMATCH',0,0,435,'OnRematchClicked',script='Player.lua',depth=1,z=29,primary=True)
button(u,'ReturnToMenuButton','LEAVE MATCH',465,0,435,'OnReturnToMenuClicked',script='Player.lua',depth=1,z=29)
u.panel('DeathOverlay',0,0,0,0,(0,0,0,0),visible=False)
# Shared pause layout is also written as a standalone asset for the editor.
pause=UI();pause.panel('PauseMenu',0,0,1920,1080,(.12,.17,.23,.97),z=50,visible=False)
pause.text('PauseTitle','Paused',550,190,820,85,52,depth=1,z=51)
pause.text('PauseHint','Online matches continue while this menu is open.',550,290,850,65,25,MUTED,depth=1,z=51)
for i,(name,label,cb) in enumerate([('ResumeButton','RESUME','OnResumeClicked'),('OpenArmoryButton','LOADOUT','OnLoadoutClicked'),('SettingsButton','SETTINGS','OnSettingsClicked'),('DisconnectButton','LEAVE MATCH','OnDisconnectClicked')]):button(pause,name,label,550,400+i*100,820,cb,script='Player.lua',depth=1,z=53,primary=i==0)
pause.save('Pause.ui');u.rows+=pause.rows[2:];u.save('Duel.ui')
# Reused settings canvas; wrappers select the active scene's callback owner.
u=page('Settings','Make it yours.')
for i,tab in enumerate(['Controls','Audio','Graphics']):
    button(u,'Tab'+tab+'Button',tab.upper(),104+i*320,220,290,'OnSettings'+tab,script='Player.lua')
    u.panel('TabIndicator'+tab,104+i*320,288,290,3,ACCENT)
def section(name):u.panel('Settings'+name,104,315,1706,560,(0,0,0,0))
def slider(key,label,x,y,width=620):
    u.text('Label_'+key,label,x,y,width,36,25,depth=1)
    u.w(1,5,'Setting_'+key,x,y+48,width-115,28,(.14,.17,.2,1),5,hit=True,tail='0.5 0.60 0.72 0.61 1 0.94 0.96 0.94 1')
    u.text('Value_'+key,'',x+width-105,y+40,105,45,24,ACCENT,depth=1,align=2)
def toggle(key,label,x,y,width=620):
    u.text('Label_'+key,label,x,y+10,width-220,45,25,depth=1)
    button(u,'Toggle_'+key+'Button','',x+width-200,y,200,'OnToggle_'+key,script='Player.lua',depth=1)
section('Controls');slider('sensitivity','Mouse sensitivity',0,0);slider('ads','Aim sensitivity multiplier',0,110);slider('fov','Field of view',0,220);toggle('invert','Invert mouse Y',0,345)
u.text('BindingHelp','Select a binding, then press a key or mouse button.',770,0,936,40,23,MUTED,depth=1)
actions=[('forward','Forward'),('back','Backward'),('left','Strafe left'),('right','Strafe right'),('sprint','Sprint'),('jump','Jump'),('fire','Fire'),('aim','Aim'),('reload','Reload'),('primary','Primary'),('secondary','Secondary'),('interact','Interact'),('loadout','Loadout'),('crouch','Crouch')]
for i,(id,label) in enumerate(actions):
    col=i//7;row=i%7;x=770+col*470;y=60+row*66
    u.text('BindName_'+id,label,x,y+10,210,44,22,depth=1)
    u.button('Bind_'+id+'Button','',x+210,y,230,54,'OnBind_'+id,'Player.lua',1)
u.text('Reserved','Escape always opens/closes menus. Conflicts swap bindings.',770,530,936,35,21,MUTED,depth=1)
section('Audio');slider('master','Master volume',0,0,1000);slider('sfx','Game sounds',0,135,1000);slider('ui','Menu sounds',0,270,1000)
button(u,'AudioTestButton','TEST GAME SOUND',0,435,550,'OnSettingsAudioTest',script='Player.lua',depth=1)
section('Graphics');toggle('quality','Quality preset',0,0,760);toggle('shadow','Shadows',0,100,760);toggle('aa','Anti-aliasing',0,200,760);toggle('bloom','Bloom',0,300,760);toggle('fog','Fog',0,400,760)
slider('distance','View distance',920,0,760);slider('brightness','Brightness',920,145,760)
u.text('QualityHint','Presets adjust shadows, reflections and lighting.',920,320,760,80,24,MUTED,depth=1)
u.text('SettingsStatus','',104,895,1706,45,23,MUTED)
button(u,'SettingsCancelButton','CANCEL',104,970,300,'OnSettingsCancel',script='Player.lua')
button(u,'SettingsResetButton','RESET DEFAULTS',435,970,380,'OnSettingsReset',script='Player.lua')
button(u,'SettingsApplyButton','APPLY & BACK',1380,970,430,'OnSettingsApply',script='Player.lua',primary=True)
u.save('Settings.ui');wrapper('SettingsLobby.ui','Settings','MainMenu');wrapper('SettingsInGame.ui','Settings','Player')
print('Rebuilt DuelFPS menus, loadout, browser, HUD, pause and shared settings.')

# Tactical armory: category navigation, actual model portraits, and readable stats.
weapons=[('rifle','Rifle','ASSAULT RIFLES','KESTREL AR4'),('smg','SMG','SUBMACHINE GUNS','VECTOR K9'),('shotgun','Shotgun','SHOTGUNS','BREACH S8'),('pistol','Pistol','HANDGUNS','MAKO P12'),('sniper','Sniper','SNIPER RIFLES','SENTINEL S98'),('marksman','Marksman','MARKSMAN RIFLES','REVENANT M14')]
def portrait(u,name,id,x,y,w,h,depth=0):
    u.w(depth,2,name,x,y,w,h,(1,1,1,1),3,tail=q('Assets/UI/Art/'+id+'.png'))
u=page('Loadout','Choose a slot, browse a category, then equip your weapon.')
# Presets stay available, but the main flow is the two slots and one preview.
u.text('SlotsTitle','LOADOUT 1',104,215,600,42,23,MUTED)
for i in range(1,6):
    x=1330+(i-1)*96
    u.button(f'Profile{i}Button',str(i),x,215,80,48,f'OnProfile{i}Clicked','Player.lua')
    u.panel(f'Profile{i}Plate',x,269,80,3,ACCENT)
for i,(slot,label) in enumerate([('Primary','Primary weapon'),('Secondary','Secondary weapon')]):
    x=104+i*864
    u.button(slot+'Tab',label,x,300,842,124,'On'+slot+'SlotClicked','Player.lua')
    # Put both the slot and its current weapon inside the clickable card.
    u.rows[-1]=u.rows[-1].replace(' 0 794 124 ', ' 8 794 42 ')
    u.text(slot+'Value','',24,58,790,48,30,depth=1,z=7)
    u.panel(slot+'PlateAccent',x,424,842,4,ACCENT,z=8)
u.text('SlotHint','Choose your primary weapon',104,457,1706,44,26)
u.text('CategoryHeading','WEAPON TYPE',104,531,340,35,20,MUTED)
for i,(id,cb,category,name) in enumerate(weapons):
    y=582+i*54
    u.button('Category_'+id+'_Button',category.title(),104,y,350,46,'OnCategory'+cb,'Player.lua')
    u.panel('Category_'+id+'_Plate',104,y,4,46,ACCENT,z=7)
alternatives=[('rifle2', 'Falcon', 'rifle', 'FALCON AR7'), ('smg2', 'Wasp', 'smg', 'WASP M9'), ('shotgun2', 'Hammer', 'shotgun', 'HAMMER S12'), ('pistol2', 'Viper', 'pistol', 'VIPER R6'), ('sniper2', 'Longbow', 'sniper', 'LONGBOW X50'), ('marksman2', 'Scout', 'marksman', 'SCOUT DMR')]
for base,_,_,name in weapons:
    alt=next(v for v in alternatives if v[2]==base)
    u.panel('Choices_'+base,530,615,1240,54,(0,0,0,0),z=6,visible=base=='rifle')
    for j,(id,label) in enumerate([(base,name),(alt[0],alt[3])]):
        u.button('Choice_'+id,label,j*610,0,590,50,'OnBrowse_'+id,'Player.lua',depth=1,z=7)
        u.panel('ChoiceMark_'+id,j*610,51,590,3,ACCENT,depth=1,z=8)
weapons += [(id,cls,'',name) for id,cls,base,name in alternatives]
u.panel('PreviewBackdrop',490,520,1320,390,(.17,.23,.30,1))
u.text('PreviewName','',530,548,1210,65,42,z=4)
for id,cb,_,name in weapons:
    portrait(u,'PreviewGun_'+id,id,870,660,560,200)
    u.panel('WeaponCategory_'+id,530,820,1220,70,(0,0,0,0),z=5,visible=id=='rifle')
    u.text(id+'Status','',0,12,720,48,22,MUTED,depth=1,z=6)
    u.button(id+'Button','Equip weapon',790,0,430,64,'On'+cb+'Selected','Player.lua',depth=1,z=7,primary=True)
button(u,'BackButton','CANCEL',104,970,300,'OnLoadoutBack',script='Player.lua')
button(u,'SaveButton','SAVE & BACK',1380,970,430,'OnLoadoutSaved',script='Player.lua',primary=True)
u.save('Loadout.ui')

# Add a circular scope surround and fine etched reticle above the game, below menus.
import math
p=A/'UI/Duel.ui';scope=UI()
scope.panel('ScopeOverlay',0,0,1920,1080,(0,0,0,0),z=15,visible=False)
for i in range(180):
    y=i*6;dy=y+3-540
    half=math.sqrt(max(0,465**2-dy**2));edge=960-half
    scope.panel('ScopeLeft'+str(i),0,y,edge,6,(.004,.005,.006,1),depth=1,z=16)
    scope.panel('ScopeRight'+str(i),1920-edge,y,edge,6,(.004,.005,.006,1),depth=1,z=16)
scope.panel('ScopeHorizontal',495,539,930,2,(.015,.018,.015,1),depth=1,z=17)
scope.panel('ScopeVertical',959,75,2,930,(.015,.018,.015,1),depth=1,z=17)
for i in [-3,-2,-1,1,2,3]:
    scope.panel('ScopeTickX'+str(i),960+i*60,532,2,16,(.01,.01,.01,1),depth=1,z=17)
    scope.panel('ScopeTickY'+str(i),952,540+i*60,16,2,(.01,.01,.01,1),depth=1,z=17)
scope.panel('ScopeDot',958,538,4,4,(.85,.16,.08,1),depth=1,z=18)
scope.text('ScopeZoom','6x',1250,900,150,40,23,ACCENT,depth=1,z=18)
p.write_text(p.read_text()+'\n'.join(scope.rows[2:])+'\n')

u=page('BREAKBULK','TACTICAL DUEL  /  TERMINAL 07')
u.panel('HeroPanel',900,220,910,650,(.17,.23,.30,1))
for x in range(920,1810,55):u.panel('HeroGrid'+str(x),x,220,1,650,(.085,.095,.09,1))
portrait(u,'HeroWeapon','rifle',880,290,920,330)
u.text('HeroWeaponName','KESTREL / AR4',960,650,810,64,42)
u.text('HeroCopy','SIX PLATFORMS. ONE RIVAL.',960,730,810,50,24,ACCENT)
u.text('Hero','HOLD YOUR GROUND.',160,290,700,100,44)
for i,(name,label,cb) in enumerate([('PlayButton','DEPLOY','OnPlayClicked'),('LoadoutButton','WEAPONS & LOADOUT','OnLoadoutClicked'),('SettingsButton','SETTINGS','OnSettingsClicked')]):button(u,name,label,160,480+i*94,600,cb,primary=i==0)
u.text('NetworkStatus','',104,895,1650,60,24,MUTED)
button(u,'QuitButton','QUIT GAME',160,762,600,'OnQuitClicked')
u.panel('FooterRule',104,975,1706,2,ACCENT)
u.text('Footer','BREAKBULK  /  FIRST TO FIVE  /  PRECISION UNDER PRESSURE',104,1000,1700,38,20,MUTED)
u.save('MainMenu.ui')
from duel_lobby import build as build_lobby
build_lobby(UI,A,q,weapons,alternatives)
