"""Bake camera-aligned weapon meshes and their original tactical material finishes.
Uses the existing project GunPack geometry; never downloads third-party assets.
"""
from pathlib import Path
import math,json,random,wave,struct
from PIL import Image, ImageDraw
from duel_finishes import finishes,surface_uvs
A=Path(__file__).resolve().parents[1]/'Projects/DuelFPS/Assets'
OUT=A/'Models/Tactical';OUT.mkdir(exist_ok=True)
finishes(OUT)
ART=A/'UI/Art';ART.mkdir(exist_ok=True)
WEAPONS=[('rifle','Rifle','AssaultRifle_1',.30,'KESTREL AR4',(.24,.19,.115)),('smg','SMG','Bullpup_1',.19,'VECTOR K9',(.055,.12,.11)),('shotgun','Shotgun','Shotgun_1',.22,'BREACH S8',(.23,.11,.055)),('pistol','Pistol','Pistol_1',.30,'MAKO P12',(.09,.12,.15)),('sniper','Sniper','SniperRifle_1',.18,'SENTINEL S98',(.19,.23,.12)),('marksman','Marksman','SniperRifle_3',.18,'REVENANT M14',(.27,.16,.085))]
WEAPONS += [('rifle2', 'Falcon', 'AssaultRifle_3', 0.3, 'FALCON AR7', (0.12, 0.2, 0.27)), ('smg2', 'Wasp', 'SubmachineGun_1', 0.25, 'WASP M9', (0.12, 0.2, 0.27)), ('shotgun2', 'Hammer', 'Shotgun_3', 0.22, 'HAMMER S12', (0.12, 0.2, 0.27)), ('pistol2', 'Viper', 'Revolver_1', 0.3, 'VIPER R6', (0.12, 0.2, 0.27)), ('sniper2', 'Longbow', 'SniperRifle_5', 0.18, 'LONGBOW X50', (0.12, 0.2, 0.27)), ('marksman2', 'Scout', 'SniperRifle_2', 0.18, 'SCOUT DMR', (0.12, 0.2, 0.27))]
# Borrow only the scene-format writer definitions, not the legacy asset build.
import ast
source=Path(__file__).with_name('build_emberwild.py').read_text()
node=ast.parse(source);env={'math':math}
exec(compile(ast.Module(body=[n for n in node.body if isinstance(n,ast.FunctionDef) and n.name in ('entity','write_scene')],type_ignores=[]),'<scene writers>','exec'),env)
entity=env['entity']
for id,prefab,src,scale,name,paint in WEAPONS:
    lines=(A/'Models/GunPack'/f'{src}.obj').read_text().splitlines()
    verts=[tuple(map(float,l.split()[1:4])) for l in lines if l.startswith('v ')]
    sight=max(v[1] for v in verts)*scale
    mats={l[7:] for l in lines if l.startswith('usemtl ')}
    colors={};out=[]
    for line in lines:
        if line.startswith('mtllib '):out.append(f'mtllib {id}.mtl')
        elif line.startswith('v '):
            x,y,z=map(float,line.split()[1:4]);out.append(f'v {z*scale:.6f} {y*scale-sight:.6f} {-x*scale:.6f}')
        elif line.startswith('vn '):
            x,y,z=map(float,line.split()[1:4]);out.append(f'vn {z:.6f} {y:.6f} {-x:.6f}')
        else:out.append(line)
    (OUT/f'{id}.obj').write_text('\n'.join(surface_uvs(out))+'\n')
    mtl=['# VelcrynLinear']
    for m in sorted(mats):
        low=m.lower();color=paint;metal=.15;rough=.64
        if 'black' in low:color=(.018,.022,.027);metal=.03;rough=.78
        elif 'darkmetal' in low:color=paint;metal=.35;rough=.48
        elif 'metal' in low:color=(.095,.11,.13);metal=.75;rough=.38
        elif 'wood' in low:color=paint;metal=0;rough=.8
        elif 'glass' in low:color=(.015,.08,.10);metal=.35;rough=.15
        colors[m]=color;mtl += [f'newmtl {m}','Kd '+' '.join(map(str,color)),f'Pm {metal}',f'Pr {rough}','d 1']
        if 'glass' not in low:mtl.append('map_Kd Parkerized.png')
    (OUT/f'{id}.mtl').write_text('\n'.join(mtl)+'\n')
    # Root and mesh are now aligned: no child yaw to corrupt parent pitch.
    root=entity(1,name+' Viewmodel')
    mesh=entity(2,name,mesh=id,parent=1)
    # The borrowed writer defaults to Emberwild's folder. Resolve the authored
    # mesh path explicitly instead of relying on that writer's project prefix.
    import re
    mesh=re.sub(r'"Assets/Models/[^"\n]+"',f'"Assets/Models/Tactical/{id}.obj"',mesh)
    assert (A/'Models/Tactical'/f'{id}.obj').is_file()
    muzzle=entity(3,'MuzzleSocket',(0,-sight*.25,-max(v[0] for v in verts)*scale),parent=2)
    (A/'Prefabs'/f'{prefab}Viewmodel.prefab').write_text('MyEngineScene\nEntities 3\n'+root+mesh+muzzle+'HierarchyFolders 0\n')
    # Slightly elevated three-quarter bench view. Face normals light the
    # receiver, rail and grip independently rather than filling a silhouette.
    image=Image.new('RGBA',(1400,500));draw=ImageDraw.Draw(image)
    def project(p):
        x,y,z=p;return (.966*x-.259*z,.975*y-.222*(.259*x+.966*z),.222*y+.975*(.259*x+.966*z))
    projected=list(map(project,verts))
    lo=min(v[0] for v in projected);hi=max(v[0] for v in projected);ymin=min(v[1] for v in projected);ymax=max(v[1] for v in projected)
    factor=min(1260/(hi-lo),350/(ymax-ymin));cx=(hi+lo)/2;cy=(ymax+ymin)/2;faces=[];mat=''
    for line in lines:
        if line.startswith('usemtl '):mat=line[7:]
        elif line.startswith('f '):
            ids=[int(v.split('/')[0])-1 for v in line.split()[1:]];points=[projected[i] for i in ids]
            faces.append((sum(v[2] for v in points)/len(points),points,mat))
    for depth,points,mat in sorted(faces,key=lambda f:f[0]):
        aa,bb,cc=points[:3];u=[bb[i]-aa[i] for i in range(3)];v=[cc[i]-aa[i] for i in range(3)]
        nx,ny,nz=u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0];n=max(1e-9,math.sqrt(nx*nx+ny*ny+nz*nz));nx,ny,nz=nx/n,ny/n,nz/n
        diffuse=max(0,-.32*nx+.74*ny+.59*nz);shine=max(0,-.2*nx+.5*ny+.84*nz)**14
        color=colors.get(mat,paint);shade=.62+.75*diffuse
        rgb=tuple(int(min(1,c**.46*shade+.19*shine)*255) for c in color)
        draw.polygon([(700+(x-cx)*factor,250-(y-cy)*factor) for x,y,z in points],fill=(*rgb,255))
    image.save(ART/f'{id}.png')
# Correct only the authored terminal material set, preserving legacy imports.
p=A/'Models/Breakbulk/Terminal.mtl';text=p.read_text();text=text if text.startswith('# VelcrynLinear') else '# VelcrynLinear\n'+text;p.write_text(text)
for scene in ['Arena','TerminalDrill']:
    p=A/'Scenes'/f'{scene}.scene';text=p.read_text().replace('Light 1 1 0.84 0.64 -0.48 -0.78 -0.38 2.3','Light 1 1 0.82 0.64 -0.48 -0.78 -0.38 1.35');p.write_text(text)
# Dedicated rifle reports, deterministic synthesized audio.
for name,freq,duration in [('sniper',62,.65),('marksman',95,.32)]:
    rng=random.Random(71);samples=[];rate=44100
    for i in range(int(rate*duration)):
        t=i/rate;v=.72*math.exp(-t*18)*rng.uniform(-1,1)+.4*math.sin(2*math.pi*freq*t)*math.exp(-t*10);samples.append(struct.pack('<h',int(max(-1,min(1,v))*30000)))
    with wave.open(str(A/'Audio/Breakbulk'/f'{name}.wav'),'wb') as w:w.setparams((1,2,rate,0,'NONE','not compressed'));w.writeframes(b''.join(samples))
print('Baked six aligned weapons, PBR finishes, thumbnails, reports and dusk lighting')
