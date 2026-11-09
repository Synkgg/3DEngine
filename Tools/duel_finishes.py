"""Author deterministic surface detail and project missing GunPack UVs per face."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFilter
import math,random

def finishes(directory):
    rng=random.Random(836)
    for name,mid in [('Parkerized',217),('Canvas',210),('Pavement',170),('Sheet',183)]:
        n=512
        coarse=Image.new('L',(32,32));coarse.putdata([rng.randrange(125,220) for _ in range(1024)]);coarse=coarse.resize((n,n),Image.Resampling.BICUBIC)
        vals=[]
        for i,c in enumerate(coarse.getdata()):
            x=i%n;y=i//n
            weave=((x%4==0)*8+(y%4==0)*8) if name=='Canvas' else 0
            vals.append(max(0,min(255,round(mid+(c-172)*.30+rng.gauss(0,7)-weave))))
        img=Image.new('L',(n,n));img.putdata(vals);d=ImageDraw.Draw(img)
        if name=='Parkerized':
            for j in range(100):
                x=rng.randrange(n);y=rng.randrange(n);d.line((x,y,x+rng.randrange(2,17),y+1),fill=rng.randrange(210,244))
        elif name=='Pavement':
            d.line((0,1,n,1),fill=92,width=2);d.line((1,0,1,n),fill=100,width=2)
            for j in range(12):
                x=rng.randrange(n);y=rng.randrange(n);d.line((x,y,x+4,y+3,x+7,y+4),fill=127)
        img.convert('RGB').save(directory/(name+'.png'))

def surface_uvs(lines):
    if any(s.startswith('vt ') for s in lines):return lines
    verts=[tuple(map(float,s.split()[1:4])) for s in lines if s.startswith('v ')];out=[];idx=0
    for s in lines:
        if not s.startswith('f '):out.append(s);continue
        refs=[v.split('/') for v in s.split()[1:]];p=[verts[int(v[0])-1] for v in refs];u=[p[1][k]-p[0][k] for k in range(3)];v=[p[2][k]-p[0][k] for k in range(3)]
        normal=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]];axis=max(range(3),key=lambda k:abs(normal[k]));a,b=[k for k in range(3) if k!=axis];face=[]
        for ref,pt in zip(refs,p):
            idx+=1;out.append(f'vt {pt[a]*2.5:.5f} {pt[b]*2.5:.5f}');face.append(f'{ref[0]}/{idx}'+('/'+ref[2] if len(ref)>2 else ''))
        out.append('f '+' '.join(face))
    return out
