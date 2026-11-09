"""Author the depot, map diagrams, and fitted operator equipment in project units."""
from pathlib import Path
import math,re
from PIL import Image,ImageDraw
from duel_finishes import surface_uvs
A=Path(__file__).resolve().parents[1]/'Projects/DuelFPS/Assets'
S=A/'Scenes';M=A/'Models/Tactical'
def parts(s):return re.findall(r'^Entity \d+\n.*?(?=^Entity \d+\n|^HierarchyFolders)',s,re.M|re.S)
arena=(S/'Arena.scene').read_text();base=parts(arena);training=parts((S/'TerminalDrill.scene').read_text())
def pos(s,p):return re.sub(r'Position [^\n]+','Position '+' '.join(map(str,p)),s,count=1)
def name(s,n):return re.sub(r'Name [^\n]+','Name '+n,s,count=1)
def block(n,p,size,color):
    s=pos(name(base[5],n),p);s=s.replace('Scale 1 1 1','Scale '+' '.join(map(str,size)))
    s=s.replace('Mesh 0','Mesh 1 1 "" 0').replace('Color 0','Color 1 '+' '.join(map(str,color))+' 1')
    surface='Pavement' if n in ('Service apron','Depot retaining wall','Depot boundary','Jersey barrier') else 'Sheet'
    s=s.replace('Texture 0',f'Texture 1 Assets/Models/Tactical/{surface}.png')
    return re.sub(r'Collider 1 [^\n]+','Collider 1 1 1 1',s)
def write_scene(filename,b):
    data=''.join(re.sub(r'^Entity \d+',f'Entity {i}',s) for i,s in enumerate(b,1))
    (S/filename).write_text(arena.split('Entities ')[0]+f'Entities {len(b)}\n'+data+'HierarchyFolders 0\n')
grey=(.26,.29,.29);steel=(.16,.23,.25);rust=(.32,.18,.10)
depot=[base[0],base[1],pos(base[3],(-7,1.1,14)),pos(base[4],(7,1.1,-14)),next(b for b in base if 'Name Late shift sun' in b)]
depot.append(block('Service apron',(0,-.35,0),(30,.7,36),grey))
for x in [-15,15]:depot.append(block('Depot retaining wall',(x,1.75,0),(.5,3.5,36),grey))
for z in [-18,18]:depot.append(block('Depot boundary',(0,1.75,z),(30,3.5,.5),grey))
for sign in [-1,1]:
    for x,z,w,h,d,n,c in [(5,9,7,2.8,2,'Loading dock',steel),(-7,3,3,2.4,6,'Pump housing',rust),(1,3,3,1.05,2,'Jersey barrier',grey),(10,-5,2.4,1.45,2.4,'Freight crate',rust),(-11,11,2,3.8,2,'Support pier',steel)]:
        depot.append(block(n,(x*sign,h/2,z*sign),(w,h,d),c))
    depot.append(block('Gantry column',(sign*10,3,0),(.6,6,.7),steel))
depot.append(block('Overhead gantry',(0,6,0),(21,.65,.8),steel))
def trim(n,p,size,col):depot.append(block(n,p,size,col).replace('Collider 1 1 1 1','Collider 0'))
for sign in [-1,1]:
    for j in range(8):
        trim('Bay shutter slat',(5*sign,.25+j*.32,9*sign-sign*1.015),(6.68,.047,.035),(.09,.14,.16))
    for j in range(9):
        trim('Ribbed sheet',(-7*sign+sign*1.51,1.18,(3+(j-4)*.65)*sign),(.035,2.3,.07),(.22,.13,.085))
    for j in range(6):trim('Truck lane stripe',(8*sign,.01,(j*5-12)*sign),(.14,.018,2.3),(.69,.56,.28))
    for j in range(4):trim('Dock bumper',((2+j*1.9)*sign,.28,9*sign-sign*1.07),(.37,.42,.14),(.06,.065,.061))
    for j in range(7):trim('Boundary joint',(sign*14.73,1.75,j*5-15),(.055,3.45,.07),(.16,.17,.16))
    trim('Crate brace',(10*sign,1.04,-5*sign+1.22),(2.38,.12,.04),(.17,.104,.057))
for j in range(11):trim('Gantry web',(-10+j*2,6.46,0),(.09,.74,.69),steel)
trim('Gantry cap',(0,6.83,0),(21,.10,.8),steel)
write_scene('Depot.scene',depot);write_scene('DepotDrill.scene',depot+training[len(base):])
for key,b in [('Terminal',base),('Depot',depot)]:
    im=Image.new('RGBA',(256,256));d=ImageDraw.Draw(im);d.ellipse((2,2,253,253),fill=(32,46,49,225),outline=(193,202,188,200),width=3)
    for s in b:
        co=re.search(r'Collider 1 ([^\n]+)',s)
        if not co:continue
        x,y,z=map(float,re.search(r'Position ([^\n]+)',s)[1].split());w,h,l=map(float,co[1].split());sx,sy,sz=map(float,re.search(r'Scale ([^\n]+)',s)[1].split());w*=sx;l*=sz
        if y<0 or h*sy<.2:continue
        d.rectangle((128+(x-w/2)*5,128+(z-l/2)*5,128+(x+w/2)*5,128+(z+l/2)*5),fill=(117,133,130,230))
    im.save(A/f'UI/Art/Radar{key}.png')

# Closed rings shape the limbs, boots, helmet and equipment around a rifle-ready pose.
palette={'cloth':(.095,.118,.085),'vest':(.155,.145,.093),'strap':(.045,.05,.043),'leather':(.067,.055,.041),'lens':(.028,.05,.058),'steel':(.082,.087,.084),'skin':(.22,.14,.09)}
(M/'Fieldgear.mtl').write_text('# VelcrynLinear\n'+''.join(f'newmtl {k}\nKd {r} {g} {b}\nPr .74\n'+('map_Kd Canvas.png\n' if k in ('cloth','strap','vest') else '') for k,(r,g,b) in palette.items()))
def start():return ['mtllib Fieldgear.mtl','s 1'],[]
rows,v=start()
def form(mat,rings,n=16):
    start=len(v);rows.append('usemtl '+mat)
    for x,y,z,rx,rz in rings:
        for i in range(n):
            t=i*2*math.pi/n;p=(x+rx*math.cos(t),y,z+rz*math.sin(t));v.append(p);rows.append('v %.5f %.5f %.5f'%p)
    for j in range(len(rings)-1):
        for i in range(n):
            a=start+j*n+i+1;b=start+j*n+(i+1)%n+1;rows.append(f'f {a} {a+n} {b+n} {b}')
    for j in (0,len(rings)-1):
        ids=list(range(start+j*n+1,start+(j+1)*n+1));rows.append('f '+' '.join(map(str,ids if j==0 else ids[::-1])))
def bulb(mat,x,y,z,w,h,d):
    form(mat,[(x,y+h*off,z,w*r,d*r) for off,r in [(-1,.32),(-.72,.78),(-.3,.96),(.25,1),(.72,.8),(1,.30)]])
for side in [-1,1]:
    x=side*.142;z=side*.085
    form('cloth',[(x,-.78,z,.093,.105),(x,-.62,z,.116,.115),(x,-.42,z-.025,.114,.119),(x,-.30,z,.14,.14),(x*.82,-.06,0,.145,.15)])
    bulb('leather',x,-.78,z-.045,.11,.12,.17);bulb('vest',x,-.43,z-.095,.10,.115,.047)
    leg=['mtllib Fieldgear.mtl','s 1'];indices=iter(v)
    for line in rows[2:]:
        if line.startswith('v '):lx,ly,lz=next(indices);leg.append(f'v {lx:.5f} {ly+.08:.5f} {lz:.5f}')
        else:leg.append(line)
    (M/('FieldLeg'+('L' if side<0 else 'R')+'.obj')).write_text('\n'.join(surface_uvs(leg))+'\n');rows,v=start()
form('cloth',[(0,-.15,0,.22,.15),(0,.07,0,.23,.145),(0,.38,0,.285,.17),(0,.49,0,.20,.125)],24)
form('vest',[(0,-.025,-.018,.24,.17),(0,.12,-.018,.254,.182),(0,.37,-.01,.278,.188),(0,.43,0,.22,.155)],20)
for x in [-.165,-.055,.055,.165]:bulb('vest',x,.09,-.175,.049,.105,.048)
bulb('strap',0,.50,0,.105,.09,.098);bulb('cloth',0,.63,-.012,.129,.16,.132);bulb('lens',0,.67,-.115,.119,.043,.042);bulb('vest',0,.744,0,.153,.133,.158)
for side in [-1,1]:
    x=side*.265;end=.055 if side==1 else -.07;depth=-.24 if side==1 else -.41
    form('cloth',[(end,.17,depth,.057,.057),(side*.19,.19,-.21,.083,.08),(x,.16,-.09,.089,.089),(x*1.1,.29,-.02,.10,.10),(x,.41,0,.115,.116),(x,.45,0,.079,.076)])
    bulb('leather',end,.185,depth-.035,.068,.069,.08)
(M/'FieldOperator.obj').write_text('\n'.join(surface_uvs(rows))+'\n');rows,v=start()
for x,z in [(.055,.06),(-.055,-.29)]:
    rows,v=start()
    form('cloth',[(x*3,-.26,z+.34,.075,.115),(x*2,-.17,z+.2,.068,.118),(x,-.14,z+.07,.048,.072)])
    bulb('leather',x,-.126,z,.056,.065,.09)
    for j in range(4):bulb('leather',x*.60,-.15+j*.018,z-.05,.045,.012,.027)
    (M/('TriggerHand.obj' if x>0 else 'SupportHand.obj')).write_text('\n'.join(surface_uvs(rows))+'\n')
remote=A/'Prefabs/RemotePawn.prefab';root,body=parts(remote.read_text())[:2];limbs=[]
body=re.sub(r'^Mesh .*$', 'Mesh 1 0 "Assets/Models/Tactical/FieldOperator.obj" 0',body,flags=re.M)
for i,(n,p,asset) in enumerate([('Left leg',(0,-.08,0),'FieldLegL'),('Right leg',(0,-.08,0),'FieldLegR'),('Carried carbine',(0,.31,-.14),'rifle')],3):
    part=pos(name(body,n),p).replace('Entity 2',f'Entity {i}').replace('Parent 1','Parent 2').replace('FieldOperator.obj',asset+'.obj')
    limbs.append(part.replace('Parent 2','Parent 6') if n=='Carried carbine' else part)
torso=name(body,'Upper body').replace('Entity 2','Entity 6').replace('Parent 1','Parent 2')
rig=re.sub(r'^Mesh .*$', 'Mesh 0',body,flags=re.M)
remote.write_text('MyEngineScene\nEntities 6\n'+root+rig+''.join(limbs)+torso+'HierarchyFolders 0\n')
for p in (A/'Prefabs').glob('*Viewmodel.prefab'):
    entities=parts(p.read_text())[:3]
    for i,n in enumerate(['TriggerHand','SupportHand'],4):
        hand=name(entities[1],n);hand=re.sub('"Assets/Models/[^"\n]+"',f'"Assets/Models/Tactical/{n}.obj"',hand).replace('Entity 2',f'Entity {i}')
        entities.append(hand)
    p.write_text('MyEngineScene\nEntities 5\n'+''.join(entities)+'HierarchyFolders 0\n')
