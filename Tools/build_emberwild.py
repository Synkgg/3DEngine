from pathlib import Path
import math, json, random
ROOT = Path(__file__).resolve().parents[1] / 'Projects/Emberwild'
A = ROOT / 'Assets'
MODEL = A / 'Models/Emberwild'
COLORS = {'earth':(.16,.20,.13),'wood':(.24,.13,.065),'leaf':(.08,.23,.14),'rock':(.34,.40,.38),'ember':(1,.36,.055),'water':(.09,.32,.38),'berry':(.65,.15,.13),'cloth':(.64,.52,.29)}
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
        (MODEL/f'{name}.modelasset').unlink(missing_ok=True)
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
Mesh {('1 0 "Assets/Models/Emberwild/'+mesh+'.obj" 0') if mesh else '0'}
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
        tail+=f' "Assets/Audio/Emberwild/interface.wav" "Assets/Scripts/{script}" {q(callback)} "" "" "" ""'
        self.w(depth,3,name,x,y,w,h,normal,z,hit=True,tail=tail)
        self.text(name.replace('Button','Label'),label,24,0,w-48,h,22,text,depth+1,z+1)
    def save(self,name): (A/'UI'/name).write_text('\n'.join(self.rows)+'\n')
# Original low-poly forest, reproducible from a fixed seed.
MODEL.mkdir(parents=True,exist_ok=True)
yard=Model(); collisions=[]
def block(name,p,s,mat,solid=True):
    yard.box(p,s,mat)
    if solid: collisions.append((name,p,s))
block('Forest floor',(0,-.4,0),(100,.8,100),'earth')
for x in (-49,49): block('Cliff',(x,3,0),(2,6,100),'rock')
for z in (-49,49): block('Cliff',(0,3,z),(100,6,2),'rock')
rng=random.Random(41)
for i in range(95):
    x,z=rng.uniform(-46,46),rng.uniform(-46,46)
    if x*x+z*z<100 or (abs(x-12)<7 and abs(z)<8): continue
    h=rng.uniform(4,8)
    block('Pine trunk',(x,h/2,z),(.65,h,.65),'wood')
    for j in range(3): yard.box((x,h-1+j*1.25,z),(3.8-j*.8,1.8,3.8-j*.8),'leaf')
# Campsite, water pool and rescue mast provide navigation landmarks.
for i in range(8):
    a=i*math.pi/4; yard.box((math.cos(a)*1.3,.2,math.sin(a)*1.3),(.6,.4,.6),'rock')
yard.box((0,.25,0),(1.6,.3,.4),'wood');yard.box((0,.4,0),(.4,.3,1.6),'wood')

block('Spring',(12,.02,0),(8,.04,8),'water',False)
block('Beacon mast',(-5,3,0),(.25,6,.25),'wood')
yard.box((-5,5.7,0),(2,.3,.3),'cloth')
block('Shelter post',(-4,1.6,5),(.2,3.2,.2),'wood')
block('Shelter post',(0,1.6,5),(.2,3.2,.2),'wood')
yard.box((-2,3.2,5),(5,.15,4),'cloth')
yard.save('Forest')
entities=[entity(1,'Survivor',(0,1.2,5),player=True,collider=(.65,1.8,.65),script='Survival.lua'),entity(2,'FirstPersonCamera',(0,.65,0),parent=1,extra='Camera 1 85 0.08 180 1'),entity(3,'Forest',mesh='Forest')]
for name,p,s in collisions: entities.append(entity(len(entities)+1,name,p,collider=s))
# Shared node layout is also the host's interaction validation data.
nodes=[]
for kind,points in [('wood',[(-7,4),(7,7),(-10,-6),(8,-12),(-18,10),(21,14),(-27,-20),(30,-25)]),('stone',[(6,2),(-8,-3),(16,-8),(-16,17),(28,10),(-26,-11)]),('food',[(4,9),(-6,11),(13,10),(-12,-10),(21,-18),(-20,2)])]:
    for x,z in points:
        nodes.append({'kind':kind,'x':x,'z':z})
        m=Model()
        if kind=='wood':
            for offset in (-.4,0,.4):m.box((offset,.25,0),(.3,.4,1.6),'wood')
        elif kind=='stone':
            m.box((0,.35,0),(1.2,.7,1.1),'rock',.3);m.box((.25,.75,0),(.7,.4,.7),'rock',-.2)
        else:m.box((0,.45,0),(1.4,.9,1.4),'leaf')
        if kind=='food':m.box((0,1,0),(.8,.3,.8),'berry')
        m.save(kind)
        entities.append(entity(len(entities)+1,'Node'+str(len(nodes)),(x,0,z),mesh=kind))
entities.append(entity(len(entities)+1,'Forest sunlight').replace('Light 0','Light 1 1 0.88 0.70 -0.48 -0.78 -0.38 2.0 0 90 23 35 1'))
m=Model();m.box((0,.7,0),(.5,.9,.5),'ember');m.box((.25,.5,.15),(.35,.5,.35),'ember');m.save('Flame')
entities.append(entity(len(entities)+1,'FireGlow',mesh='Flame'))
m=Model();m.box((0,0,0),(1.8,1.3,.1),'ember');m.save('Flag')
entities.append(entity(len(entities)+1,'RescueFlag',(-4.3,5,0),mesh='Flag',scale=(0,0,0)))
write_scene(A/'Scenes/Forest.scene',entities)
write_scene(A/'Scenes/MainMenu.scene',[entity(1,'Menu',script='Menu.lua')],False)
(A/'Scripts/Nodes.lua').write_text('return {\n'+''.join('{kind="%s",x=%s,z=%s},\n'%(n['kind'],n['x'],n['z']) for n in nodes)+'}\n')
m=Model();m.box((0,.8,0),(.65,1.2,.4),'cloth');m.box((0,1.6,0),(.4,.4,.4),'wood');m.save('Survivor')
write_scene(A/'Prefabs/Survivor.prefab',[entity(1,'Partner',mesh='Survivor')],False)
u=UI();u.panel('Background',0,0,1920,1080,(.025,.045,.035,1))
u.text('Eyebrow','A COOPERATIVE WILDERNESS SURVIVAL',160,180,1400,50,24,ACCENT)
u.text('Title','EMBERWILD',150,245,1500,130,100)
u.text('Description','Keep the fire alive. Bring each other home.',160,400,1400,60,32)
u.button('SoloButton','PLAY SOLO',160,530,490,74,'OnSolo',script='Menu.lua',primary=True)
u.button('HostButton','HOST CAMP',160,625,490,74,'OnHost',script='Menu.lua')
u.button('RelayButton','JOIN VIA RELAY',680,530,490,74,'OnRelay',script='Menu.lua')
u.w(0,4,'Address',680,625,300,74,(.08,.12,.10,1),4,hit=True,tail='"127.0.0.1" "Host IPv4" 25 15 0')
u.button('JoinButton','JOIN IP',1000,625,230,74,'OnJoin',script='Menu.lua')
u.text('Status','Solo or two players. Internet play: start the relay helper first.',160,755,1600,60,25,MUTED)
u.text('Instructions','Gather wood, stone and berries. Tend the fire. Build a rescue signal.',160,880,1600,45,24)
u.text('Hint','WASD move  /  Mouse look  /  E interact  /  Tab recipes  /  Escape menu',160,940,1600,45,23,MUTED)
u.save('Menu.ui')
u=UI()
u.text('Objective','',50,35,1600,50,26)
u.text('Vitals','',50,970,1800,45,26)
u.text('Inventory','',50,1015,1800,35,22,MUTED)
u.text('Prompt','',510,810,900,50,26,align=1)
u.text('Notice','',410,875,1100,50,24,ACCENT,align=1)
u.text('Crosshair','+',950,520,20,30,22,align=1)
u.panel('RecipePanel',40,140,780,460,(.025,.045,.035,.92),visible=False)
u.text('Recipes','FIELD GUIDE',24,20,730,50,30,ACCENT,depth=1)
for i,t in enumerate(['E  Gather nearby supplies / drink at the blue spring','At fire: E add wood / Q cook berries (1 food, 1 wood)','1  Craft axe: 3 wood + 2 stone (double gathering)','2  Coat: 4 wood + 3 food / F eat / V revive (2 food)','At mast: R build signal: 12 wood + 8 stone','Survive 3 days; keep fire above 20 for rescue.']):u.text('Recipe'+str(i),t,24,90+i*50,730,44,22,depth=1)
u.panel('Pause',520,280,880,530,(.025,.045,.035,.97),z=10,visible=False)
u.text('PauseTitle','CAMP MENU',40,30,800,70,42,depth=1,z=11)
u.text('PauseHint','The wilderness continues while online.',40,120,800,50,25,depth=1,z=11)
u.button('ResumeButton','RETURN',40,220,800,70,'OnResume',script='Survival.lua',depth=1,z=12)
u.button('ExitButton','LEAVE CAMP',40,320,800,70,'OnExit',script='Survival.lua',depth=1,z=12)
u.save('HUD.ui')
# No external audio dependency in this project.
for p in (A/'UI').glob('*.ui'):p.write_text(p.read_text().replace('Assets/Audio/Emberwild/interface.wav',''))
(ROOT/'Emberwild.project').write_text('Version 1\nName "Emberwild"\nAssetDirectory "Assets"\nStartupScene "Assets/Scenes/MainMenu.scene"\nSettings "ProjectSettings.cfg"\n')
print('Built Emberwild forest, 20 resources, survivor, menu and HUD')
