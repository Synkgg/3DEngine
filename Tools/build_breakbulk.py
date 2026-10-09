"""Build original BREAKBULK geometry, collision scene and UI. No external assets."""
from pathlib import Path
import math
import json

ROOT = Path(__file__).resolve().parents[1] / 'Projects/DuelFPS'
A = ROOT / 'Assets'
MODEL = A / 'Models/Breakbulk'
MODEL.mkdir(parents=True, exist_ok=True)

COLORS = {
    'asphalt': (.055, .068, .075), 'concrete': (.24, .26, .25),
    'teal': (.045, .23, .25), 'rust': (.42, .105, .045),
    'ivory': (.65, .61, .45), 'steel': (.11, .14, .15),
    'rubber': (.018, .025, .028), 'yellow': (.75, .42, .045),
    'white': (.72, .78, .76), 'wood': (.24, .15, .07),
    'lamp': (1, .62, .22), 'gun': (.065, .083, .095),
}

class Model:
    def __init__(self):
        self.vertices = []
        self.faces = {k: [] for k in COLORS}

    def box(self, pos, size, material, yaw=0):
        x,y,z = pos; w,h,d = (v/2 for v in size)
        points = [(-w,-h,-d),(w,-h,-d),(w,h,-d),(-w,h,-d),
                  (-w,-h,d),(w,-h,d),(w,h,d),(-w,h,d)]
        c,s = math.cos(yaw),math.sin(yaw)
        base = len(self.vertices)+1
        self.vertices += [(x+px*c+pz*s,y+py,z-px*s+pz*c) for px,py,pz in points]
        for face in [(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(3,7,6,2),(0,1,5,4)]:
            self.faces[material].append([base+i for i in face])

    def save(self, name):
        out = [f'mtllib {name}.mtl', 's off']
        out += ['v '+' '.join(f'{v:.5f}' for v in point) for point in self.vertices]
        for material,faces in self.faces.items():
            if faces:
                out += [f'g {material}', f'usemtl {material}']
                out += ['f '+' '.join(map(str,face)) for face in faces]
        (MODEL/f'{name}.obj').write_text('\n'.join(out)+'\n')
        materials=[]
        for name2,color in COLORS.items():
            metal = name2 in ('steel','gun','teal','rust')
            materials += [f'newmtl {name2}', 'Kd '+' '.join(map(str,color)),
                          'Ks '+('0.32 0.32 0.32' if metal else '0.04 0.04 0.04'),
                          'Ns '+('85' if metal else '8'), 'd 1']
        (MODEL/f'{name}.mtl').write_text('\n'.join(materials)+'\n')

def entity(number,name,pos=(0,0,0),scale=(1,1,1),mesh=None,collider=None,
           script=None,parent=0,extra='',rotation=(0,0,0),player=False,start=0):
    def xyz(v): return ' '.join(str(round(x,5)) for x in v)
    return f'''Entity {number}
Name {name}
Position {xyz(pos)}
Rotation {xyz(rotation)}
Scale {xyz(scale)}
Parent {parent}
PrefabSource ""
Mesh {('1 0 "Assets/Models/Breakbulk/'+mesh+'.obj" 0') if mesh else '0'}
Color {'1 1 1 1 1' if mesh else '0'}
Texture 0
Material 0
Interactable 0
Player {'1 0 0' if player else '0'}
PlayerStart {('1 '+str(start)) if start else '0'}
CharacterController {'1 18 6.5 0 1' if player else '0'}
Light 0
{extra + chr(10) if extra else ''}Collider {('1 '+xyz(collider)) if collider else '0'}
Scripts {1 if script else 0}
{('Script Assets/Scripts/'+script+chr(10)) if script else ''}'''

def write_scene(path, entities, environment=True):
    header='MyEngineScene\n'
    if environment: header+='Environment 1 4 1 0 1 220 0.9 0.001 0.13 1 90\n'
    path.write_text(header+f'Entities {len(entities)}\n'+''.join(entities)+'HierarchyFolders 0\n')

yard=Model(); collisions=[]
def block(name,pos,size,material,yaw=0,solid=True):
    yard.box(pos,size,material,yaw)
    if solid: collisions.append((name,pos,size,yaw))

block('Terminal pavement',(0,-.3,0),(30,.6,36),'asphalt')
for x in (-14.7,14.7): block('Perimeter barrier',(x,1.8,0),(.6,3.6,36),'concrete')
for z in (-17.7,17.7): block('End barrier',(0,1.8,z),(30,3.6,.6),'concrete')
# Three routes with blocked spawn-to-spawn sightlines and two open tunnel cuts.
containers=[(-8,-8,0,'rust',False),(8,8,0,'teal',False),
            (-8,5,0,'teal',True),(8,-5,0,'rust',True),
            (-1.8,-5,math.pi/2,'ivory',False),(1.8,5,math.pi/2,'teal',False)]
for index,(x,z,angle,paint,opened) in enumerate(containers):
    def part(name,p,size,mat,solid=False):
        px,py,pz=p; c,s=math.cos(angle),math.sin(angle)
        block(f'C{index+1} {name}',(x+px*c+pz*s,py,z-px*s+pz*c),size,mat,angle,solid)
    if opened:
        part('left shell',(-1.55,1.55,0),(.18,3.1,7.4),paint,True)
        part('right shell',(1.55,1.55,0),(.18,3.1,7.4),paint,True)
        part('roof',(0,3.02,0),(3.2,.18,7.4),paint,True)
        part('floor',(0,.07,0),(3.2,.14,7.4),'steel',True)
    else: part('shell',(0,1.55,0),(3.2,3.1,7.4),paint,True)
    for side in (-1,1):
        for rib in range(25): part('rib',(side*1.635,1.55,-3.45+rib*.285),(.07,2.76,.055),paint)
        for end in (-1,1): part('corner',(side*1.55,1.55,end*3.62),(.20,3.16,.20),'steel')
        part('top rail',(side*1.57,3.08,0),(.15,.17,7.4),'steel')
        part('bottom rail',(side*1.57,.09,0),(.15,.18,7.4),'steel')
    if not opened:
        for end in (-1,1):
            part('door seam',(0,1.55,end*3.72),(.035,2.85,.04),'rubber')
            for dx in (-1.12,-.42,.42,1.12):
                part('locking bar',(dx,1.52,end*3.75),(.045,2.7,.055),'white')
                part('latch',(dx,1.25,end*3.79),(.30,.06,.075),'steel')
    for side in (-1,1):
        part('ID plate',(side*1.68,2.2,1.8),(.015,.5,1.2),'ivory')
        for n in range(index+1): part('ID stencil',(side*1.695,2.2,1.35+n*.13),(.015,.30,.045),'rubber')
# Pallets, waist-high cover and lane furniture, deliberately staggered.
for index,(x,z) in enumerate([(-3,0),(3,0),(-11,12),(11,-12),(-11,-1),(11,1)]):
    block('Crate cover',(x,.7,z),(1.5,1.4,1.5),'wood')
    for y in (.18,1.2): yard.box((x,y,z),(1.57,.12,1.57),'steel')
    for dx in (-.55,.55): yard.box((x+dx,.72,z),( .09,1.4,1.58),'ivory')
for x in (-12.7,12.7):
    for z in range(-15,16,3): yard.box((x,.015,z),(.10,.025,1.7),'yellow')
for z in (-13,13):
    yard.box((0,.015,z),(7,.025,.12),'white')
    for x in range(-3,4): yard.box((x,.02,z+(.45 if z<0 else -.45)),(.45,.03,.7),'yellow')
for x,z in [(-13,-15),(13,15),(-13,15),(13,-15)]:
    block('Light mast',(x,4,z),(.16,8,.16),'steel')
    yard.box((x,7.8,z),(1.6,.15,.5),'steel')
    yard.box((x,7.69,z),(1.3,.08,.35),'lamp')
# Distant crane and stacked freight establish the terminal beyond playable bounds.
for x in (-20,20):
    yard.box((x,9,-23),(.65,18,.65),'yellow')
yard.box((0,17.5,-23),(42,.8,.8),'yellow')
yard.box((-4,14,-23),(.12,7,.12),'steel')
yard.box((-4,10.5,-23),(4,.18,.3),'steel')
for x in range(-20,25,8):
    for level in range(2): yard.box((x,1.7+level*3.3,23),(7.5,3.2,3.5),'rust' if x%16 else 'teal')
yard.save('Terminal')

entities=[entity(1,'Operator',(0,1.2,14),player=True,collider=(.65,1.8,.65),script='Player.lua'),
          entity(2,'FirstPersonCamera',(0,.65,0),parent=1,rotation=(0,-math.pi/2,0),extra='Camera 1 90 0.08 220 1'),
          entity(3,'BREAKBULK - Terminal',mesh='Terminal'),
          entity(4,'South insertion',(-4,1.2,14),start=1),
          entity(5,'North insertion',(4,1.2,-14),start=2)]
for name,pos,size,yaw in collisions:
    entities.append(entity(len(entities)+1,name,pos,collider=size,rotation=(0,yaw,0)))
sun=entity(len(entities)+1,'Late shift sun').replace('Light 0','Light 1 1 0.84 0.64 -0.48 -0.78 -0.38 2.3 0 90 23 35 1')
entities.append(sun)
for x,z in [(-11,-12),(11,12)]:
    entities.append(entity(len(entities)+1,'Terminal work light',(x,4,z)).replace('Light 0','Light 1 0.52 0.75 1 0 -1 0 4 1 10 23 35 0'))
write_scene(A/'Scenes/Arena.scene',entities)
training=list(entities)
training.append(entity(len(training)+1,'PracticeMode'))
for n,(x,z) in enumerate([(-4,-11),(4,11),(-11,1),(11,-1),(0,0),(0,-12)]):
    training.append(entity(len(training)+1,['Target_10m','Target_15m','Target_20m','Target_25m_Left','Target_25m_Right','Target_35m'][n],(x,1.1,z),mesh='Target',collider=(.75,2.2,.25)))
write_scene(A/'Scenes/TerminalDrill.scene',training)
target=Model();target.box((0,0,0),(.75,2.2,.2),'steel');target.box((0,.38,-.12),(.50,.65,.035),'yellow');target.box((0,-.2,-.12),(.12,.12,.04),'white');target.save('Target')

# Original compact weapon and operator meshes, all authored in engine coordinates.
for name,pistol in [('Kestrel',False),('Mako',True)]:
    m=Model();length=.42 if pistol else .72
    m.box((0,0,-.1),(.13,.14,length),'gun')
    m.box((0,-.16,.06),(.10,.25,.14),'rubber')
    m.box((0,-.18,-.13),(.09,.28 if not pistol else .12,.12),'steel')
    m.box((0,.015,-length*.65),(.065,.065,.22),'steel')
    m.box((0,.015,-length*.65-.12),(.085,.09,.08),'gun')
    for z in [i*.045-.29 for i in range(8 if not pistol else 4)]:
        m.box((0,.082,z),(.14,.018,.015),'steel')
    for z in (-.24,.11): m.box((0,.11,z),(.065,.075,.035),'gun')
    m.box((0,.13,.105),(.026,.023,.042),'teal')
    if not pistol:
        m.box((0,-.015,.38),(.085,.10,.3),'steel')
        m.box((0,-.07,.53),(.14,.25,.08),'rubber')
    m.save(name)
    write_scene(A/f'Prefabs/{"Pistol" if pistol else "Rifle"}Viewmodel.prefab',[
        entity(1,name+' Viewmodel'),entity(2,name,mesh=name,parent=1)],False)
m=Model()
for p,s,c in [((0,.05,0),(.60,.75,.33),'teal'),((0,.14,-.20),(.48,.48,.13),'steel'),
              ((0,.67,0),(.34,.38,.34),'gun'),((0,.7,-.19),(.29,.10,.04),'yellow'),
              ((-.42,.06,-.08),(.22,.65,.24),'teal'),((.42,.06,-.08),(.22,.65,.24),'teal'),
              ((-.18,-.61,0),(.24,.60,.28),'rubber'),((.18,-.61,0),(.24,.60,.28),'rubber'),
              ((-.18,-.94,-.08),(.28,.16,.42),'gun'),((.18,-.94,-.08),(.28,.16,.42),'gun')]: m.box(p,s,c)
m.save('Operator')
write_scene(A/'Prefabs/RemotePawn.prefab',[entity(1,'Remote Operator',collider=(.8,2,.8)).replace('Player 0','Player 1 0 0'),entity(2,'Operator armor',mesh='Operator',parent=1)],False)

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
        normal=ACCENT if primary else (.075,.105,.12,1)
        hover=(1,.76,.35,1) if primary else (.13,.28,.30,1)
        text=INK if primary else TEXT
        tail=' '.join(map(str,[*normal,*hover,.10,.40,.40,1,.07,.08,.09,1,1,*text,*text,*TEXT,*MUTED]))
        tail+=f' "Assets/Audio/Breakbulk/interface.wav" "Assets/Scripts/{script}" {q(callback)} "" "" "" ""'
        self.w(depth,3,name,x,y,w,h,normal,z,hit=True,tail=tail)
        self.text(name.replace('Button','Label'),label,24,0,w-48,h,22,text,depth+1,z+1)
    def save(self,name): (A/'UI'/name).write_text('\n'.join(self.rows)+'\n')

u=UI();u.panel('Dispatch',0,0,1920,1080,INK)
# One continuous dispatch screen: top identity, map hero, focused action column.
u.panel('HeaderLine',64,132,1792,2,(.16,.22,.24,1))
u.text('Brand','BREAKBULK',64,38,1100,78,58)
u.text('BrandTag','PRIVATE MATCH / TERMINAL 07',1230,51,626,48,22,MUTED,align=2)
u.text('MapEyebrow','01 / THE CARGO YARD',64,177,940,38,24,ACCENT)
u.text('MapTitle','TERMINAL',60,220,1000,112,80)
u.text('MapDescription','Tight angles. Three lanes. One rival.',64,344,1000,50,30,MUTED)
u.panel('DiagramFrame',64,430,990,452,(.043,.067,.075,1))
for i in range(16):u.panel('GridX'+str(i),80+i*64,446,1,418,(.065,.11,.12,1))
for i in range(7):u.panel('GridY'+str(i),80,446+i*64,958,1,(.065,.11,.12,1))
for i,(x,z,yaw,paint,opened) in enumerate(containers):
    w,h=(148,48) if yaw else (62,105)
    u.panel('CargoDiagram'+str(i),556+x*25-w/2,656+z*12-h/2,w,h,(*COLORS[paint],1),z=2)
    u.text('CargoNumber'+str(i),str(i+1).zfill(2),556+x*25-w/2+8,656+z*12-h/2+5,w-16,26,18,TEXT,z=3)
u.panel('SpawnA',446,830,16,16,ACCENT,z=4);u.panel('SpawnB',646,460,16,16,TEAL,z=4)
u.text('MapLegend','A  INSERTION',88,452,280,32,20,ACCENT,z=3)
u.text('MapLegendB','B  INSERTION',786,820,244,32,20,TEAL,z=3,align=2)
u.text('MapFacts','FIRST TO 5',64,915,310,38,25)
u.text('MapFacts2','90 SEC / ROUND',390,915,340,38,25)
u.text('MapFacts3','EQUAL LOADOUTS',746,915,350,38,25)
u.panel('ActionRail',1124,177,732,787,(.045,.06,.07,1))
u.text('PlayEyebrow','PLAY / 1V1',1164,210,650,36,23,ACCENT)
u.text('PlayTitle','Settle it here.',1160,255,650,70,46)
u.text('PlayHint','Invite a rival or learn the yard solo.',1164,339,650,40,25,MUTED)
u.button('HostButton','HOST A DUEL     +',1164,415,652,78,'OnHostClicked','MainMenu.lua',primary=True)
u.button('PracticeButton','PRACTICE / EXPLORE TERMINAL',1164,511,652,68,'OnPracticeClicked','MainMenu.lua')
u.panel('JoinRule',1164,621,652,2,(.16,.22,.24,1))
u.text('JoinCaption','JOIN A FRIEND',1164,651,650,38,24)
u.text('AddressCaption','HOST IP ADDRESS',1164,704,650,30,19,MUTED)
u.w(0,4,'AddressField',1164,750,432,66,(.075,.10,.115,1),4,hit=True,tail='"127.0.0.1" "HOST IPv4" 27 15 0')
u.button('JoinButton','CONNECT',1612,750,204,66,'OnJoinClicked','MainMenu.lua')
u.text('NetworkStatus','READY TO DEPLOY',1164,856,652,72,22,TEAL)
u.panel('FooterLine',64,994,1792,2,(.16,.22,.24,1))
u.text('Footer','TWO OPERATORS. NO SECOND CHANCES.',64,1010,1160,40,22,MUTED)
u.text('FooterMode','DIRECT IP / UDP 7777',1430,1010,426,40,20,MUTED,align=2)
u.save('MainMenu.ui')

u=UI();u.panel('DuelHUD',0,0,1920,1080,(0,0,0,0))
# Restrained score strip, open center and aligned lower-corner readouts.
u.panel('ScoreBacking',670,32,580,82,(.025,.035,.043,.82),1,2)
u.panel('ScoreAccent',670,32,4,82,ACCENT,1,3)
u.text('ScoreText','YOU 0   /   0 RIVAL',690,37,540,40,32,TEXT,1,3,1)
u.text('MatchStatus','WAITING FOR RIVAL',690,78,540,27,19,ACCENT,1,3,1)
u.text('LocationTag','TERMINAL / 07',56,42,460,40,23,TEXT,1)
u.panel('ClockBacking',1740,32,124,60,(.025,.035,.043,.82),1,2)
u.text('RoundClock','01:30',1746,38,112,44,29,TEXT,1,3,1)
u.text('CenterMessage','',560,270,800,50,28,ACCENT,1,12,1)
u.panel('Vitals',56,936,290,92,(.025,.035,.043,.70),1,4)
u.text('HealthLabel','HEALTH',72,943,250,26,18,MUTED,1,5)
u.text('HealthText','100 / 100',72,970,250,42,34,TEXT,1,5)
u.w(1,6,'HealthBar',56,1036,290,5,(.10,.15,.16,1),6,tail='1 0.14 0.61 0.59 1 0')
u.panel('AmmoPlate',1514,906,350,135,(.025,.035,.043,.70),1,4)
u.text('AmmoLabel','KESTREL / CARBINE',1530,916,316,28,22,ACCENT,1,5)
u.text('AmmoText','30 / 90',1530,946,316,50,43,TEXT,1,5)
for slot,x in [(1,1530),(2,1696)]:
    u.panel(f'Slot{slot}Plate',x,1001,150,28,(.07,.12,.13,1),1,5)
    u.text(f'Slot{slot}Text',f'{slot}  LOADOUT',x+8,1001,134,28,17,TEXT,1,6)
u.panel('DamageFlash',0,0,1920,1080,(.45,.015,.008,.16),1,9,False)
u.panel('CrosshairH',949,539,22,2,TEXT,1,10);u.panel('CrosshairV',959,529,2,22,TEXT,1,10)
u.text('Hitmarker','X',937,518,46,46,30,ACCENT,1,15,1)
u.text('ReloadText','RELOADING',760,653,400,30,18,ACCENT,1,12,1)
u.panel('MuzzleFlash',1090,635,8,8,(1,.6,.2,.8),1,11,False)
for root,title,y,height in [('RoundIntro','STAND BY',350,250),('RoundResult','ROUND SECURED',350,310),('MatchResult','VICTORY',270,320),('DeathOverlay','ELIMINATED',740,80)]:
    u.panel(root,0,0,1920,1080,(.012,.02,.025,.48 if root!='DeathOverlay' else 0),z=20,visible=False)
    u.panel(root+'Plate',520,y,880,height,INK,1,21)
    u.panel(root+'Accent',520,y,5,height,ACCENT,1,22)
    if root=='RoundIntro':
        u.text('RoundIntroEyebrow','ROUND 01',560,y+24,800,32,22,ACCENT,1,23,1)
        u.text('RoundIntroTitle','3',560,y+65,800,110,80,TEXT,1,23,1)
        u.text('RoundIntroHint','CHECK YOUR CORNERS',560,y+187,800,32,18,MUTED,1,23,1)
    elif root=='DeathOverlay':
        u.text('DeathTitle',title,560,y+7,800,36,24,ACCENT,1,23,1)
        u.text('DeathStatus','REGROUP NEXT ROUND',560,y+46,800,24,14,MUTED,1,23,1)
    else:
        u.text(root+'Title',title,560,y+37,800,80,54,TEXT,1,23,1)
        u.text(root+'Score','YOU 0 / 0 RIVAL',560,y+136,800,50,32,ACCENT,1,23,1)
        u.text('RoundResultNext' if root=='RoundResult' else 'MatchResultHint','',560,y+224,800,40,20,MUTED,1,23,1)
u.panel('MatchActions',0,0,1920,1080,(0,0,0,0),z=28,visible=False)
u.button('RematchButton','REQUEST REMATCH',520,620,424,72,'OnRematchClicked',depth=1,z=29,primary=True)
u.button('ReturnToMenuButton','LEAVE SESSION',964,620,436,72,'OnReturnToMenuClicked',depth=1,z=29)
u.panel('PauseMenu',0,0,1920,1080,(.01,.02,.025,.70),z=50,visible=False)
u.panel('PauseSidebar',0,0,690,1080,INK,1,51)
u.panel('PauseRule',64,206,562,3,ACCENT,1,52)
u.text('PauseEyebrow','BREAKBULK / SESSION',64,105,562,40,24,ACCENT,1,52)
u.text('PauseTitle','IN THE FIELD',60,254,570,85,48,TEXT,1,52)
u.text('PauseHint','The duel stays live while this menu is open.',64,366,562,110,25,MUTED,1,52)
u.button('ResumeButton','RESUME MATCH',64,534,562,76,'OnResumeClicked',depth=1,z=53,primary=True)
u.button('DisconnectButton','LEAVE SESSION',64,634,562,76,'OnDisconnectClicked',depth=1,z=53)
u.text('PauseFooter','ESC / BACK TO MATCH',64,970,562,40,20,MUTED,1,52)
u.text('ControlsTitle','FIELD MANUAL',810,230,1000,60,36,TEXT,1,52)
for i,(key,action) in enumerate([('W A S D','MOVE'),('SHIFT / SPACE','SPRINT / JUMP'),('MOUSE 1 / 2','FIRE / AIM'),('R','RELOAD'),('1 / 2','SWITCH WEAPON')]):
    y=340+i*96
    u.panel('ControlRule'+str(i),810,y+72,1000,1,(.20,.25,.27,.5),1,52)
    u.text('ControlKey'+str(i),key,810,y,370,52,28,ACCENT,1,52)
    u.text('ControlAction'+str(i),action,1220,y,590,52,26,TEXT,1,52)
u.save('Duel.ui')
print(f'Built BREAKBULK: {len(yard.vertices)} map vertices, {len(collisions)} collision bodies, fresh menu and HUD.')


# Short original synthesized effects; deterministic and redistribution-safe.
import wave, struct, random
sound_dir=A/'Audio/Breakbulk';sound_dir.mkdir(parents=True,exist_ok=True)
for effect,duration in [('carbine',.24),('sidearm',.30),('hit',.10),('interface',.09),('reload',.30),('step',.14)]:
    rng=random.Random(effect);rate=44100;frames=[];filtered=0
    for sample in range(int(rate*duration)):
        t=sample/rate;noise=rng.uniform(-1,1);filtered=.72*filtered+.28*noise
        if effect in ('carbine','sidearm'):
            value=(noise*.6*math.exp(-t*55)+filtered*.8*math.exp(-t*16)+math.sin(2*math.pi*(92 if effect=='carbine' else 70)*t)*.6*math.exp(-t*22))
        elif effect=='hit':value=math.sin(2*math.pi*1300*t)*math.exp(-t*45)*.35
        elif effect=='interface':value=math.sin(2*math.pi*(600+300*t/duration)*t)*math.exp(-t*60)*.3
        elif effect=='reload':value=filtered*math.exp(-((t-.025)/.02)**2)*.9+noise*.35*math.exp(-((t-.19)/.01)**2)
        else:value=filtered*.65*math.exp(-t*30)+math.sin(2*math.pi*90*t)*.25*math.exp(-t*35)
        frames.append(struct.pack('<h',int(max(-1,min(1,value)) * 27000)))
    with wave.open(str(sound_dir/(effect+'.wav')),'wb') as out:
        out.setnchannels(1);out.setsampwidth(2);out.setframerate(rate);out.writeframes(b''.join(frames))
