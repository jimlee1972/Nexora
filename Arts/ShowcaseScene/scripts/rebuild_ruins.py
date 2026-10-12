"""Editable reference reconstruction of the ruined crystal sanctuary.
Run with Blender 5.1: blender -b --python rebuild_ruins.py -- --preview
The concept image is used only as a hidden reference, never as render geometry.
"""
import argparse
import bpy
import bmesh
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector, noise
from math import sin, cos, pi, radians

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--preview', action='store_true')
parser.add_argument('--output', default=str(ROOT/'ruins_scene.blend'))
parser.add_argument('--render', default=str(ROOT/'ruins_scene_preview.png'))
parser.add_argument('--samples', type=int, default=32)
parser.add_argument('--device', choices=['CPU', 'CUDA'], default='CPU')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
R = random.Random(10122026)
bpy.ops.wm.read_factory_settings(use_empty=True)
S = bpy.context.scene
COL = {}
for key, label in [('hero','01 Crystal and broken astrolabe'),('plinth','02 Carved sanctuary steps'),('arch','03 Ruined arcade masonry'),('floor','04 Weathered courtyard'),('plants','05 Ivy grasses and cypresses'),('props','06 Ceramics and cloth'),('distance','07 Mountains waterfalls and distant citadel'),('light','08 Golden hour and atmosphere'),('camera','09 Camera and reference')]:
    c=bpy.data.collections.new(label);S.collection.children.link(c);COL[key]=c

def mesh(name, vs, fs, mat=None, group='arch', indices=None):
    data=bpy.data.meshes.new(name);data.from_pydata(vs,[],fs);data.update()
    ob=bpy.data.objects.new(name,data);COL[group].objects.link(ob)
    if mat:
        for m in mat if isinstance(mat,list) else [mat]:data.materials.append(m)
    if indices:
        for poly,i in zip(data.polygons,indices):poly.material_index=i
    return ob

def bevel(o,w=.03,segments=2):
    mod=o.modifiers.new('Chipped rounded stone edges','BEVEL');mod.width=w;mod.segments=segments
    return o

def box(name,loc,size,mat,edge=.025,group='arch',angle=0,wear=0):
    x,y,z=[v*.5 for v in size]
    vs=[(-x,-y,-z),(x,-y,-z),(x,y,-z),(-x,y,-z),(-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z)]
    if wear:vs=[(a+R.uniform(-wear,wear),b+R.uniform(-wear,wear),c+R.uniform(-wear,wear)) for a,b,c in vs]
    o=mesh(name,vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],mat,group)
    o.location=loc;o.rotation_euler.z=angle
    if edge:bevel(o,edge)
    return o

def path(name,pts,mat,r=.012,group='hero',closed=False):
    d=bpy.data.curves.new(name,'CURVE');d.dimensions='3D';d.resolution_u=1;d.bevel_depth=r;d.bevel_resolution=1
    p=d.splines.new('POLY');p.points.add(len(pts)-1)
    for a,b in zip(p.points,pts):a.co=(*b,1)
    p.use_cyclic_u=closed
    o=bpy.data.objects.new(name,d);COL[group].objects.link(o);d.materials.append(mat)
    return o

def lathe(name,center,profile,mat,n=64,group='props',pattern=None):
    vs=[(r*cos(2*pi*i/n),r*sin(2*pi*i/n),z) for r,z in profile for i in range(n)]
    fs=[];mi=[]
    for j in range(len(profile)-1):
        for i in range(n):
            fs.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
            mi.append(pattern((i+.5)/n,(profile[j][1]+profile[j+1][1])/2) if pattern else 0)
    fs.append(tuple(reversed(range(n))));mi.append(0)
    o=mesh(name,vs,fs,mat,group,mi);o.location=center
    for p in o.data.polygons:p.use_smooth=True
    return o

def wedge(name,center,ri,ro,a0,a1,depth,mat,group='hero',edge=.018,steps=3):
    vs=[]
    for y in [-depth/2,depth/2]:
        for radius in [ri,ro]:
            for i in range(steps+1):
                a=a0+(a1-a0)*i/steps
                vs.append((radius*cos(a),y,radius*sin(a)))
    n=steps+1;fs=[]
    for i in range(steps):
        fs.extend([(i,i+1,n+i+1,n+i),(2*n+i,3*n+i,3*n+i+1,2*n+i+1),(i,2*n+i,2*n+i+1,i+1),(n+i,n+i+1,3*n+i+1,3*n+i)])
    fs.extend([(0,n,3*n,2*n),(steps,2*n+steps,3*n+steps,n+steps)])
    o=mesh(name,vs,fs,mat,group);o.location=center
    if edge:bevel(o,edge)
    return o

def disc(name,center,r,depth,mat,group='plinth',n=96):
    o=lathe(name,center,[(r,0),(r,depth)],mat,n,group)
    bm=bmesh.new();bm.from_mesh(o.data)
    bm.verts.ensure_lookup_table();bm.faces.new([bm.verts[n+i] for i in range(n)])
    bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(o.data);bm.free()
    return o

def material(name,color,rough=.5,metal=0,emission=0):
    m=bpy.data.materials.new(name);m.use_nodes=True;m.diffuse_color=(*color,1)
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
    p.inputs['Emission Color'].default_value=(*color,1);p.inputs['Emission Strength'].default_value=emission
    return m

def noise_color(m,c1,c2,scale=5,bump=.03):
    n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF')
    tc=n.new('ShaderNodeTexCoord');tex=n.new('ShaderNodeTexNoise');tex.inputs['Scale'].default_value=scale;tex.inputs['Detail'].default_value=4
    l.new(tc.outputs['Generated'],tex.inputs['Vector'])
    ramp=n.new('ShaderNodeValToRGB');ramp.color_ramp.elements[0].position=.15;ramp.color_ramp.elements[0].color=(*c1,1);ramp.color_ramp.elements[1].position=.82;ramp.color_ramp.elements[1].color=(*c2,1)
    l.new(tex.outputs['Fac'],ramp.inputs[0]);l.new(ramp.outputs['Color'],p.inputs['Base Color'])
    if bump:
        micro=n.new('ShaderNodeTexNoise');micro.inputs['Scale'].default_value=95;micro.inputs['Detail'].default_value=3;l.new(tc.outputs['Generated'],micro.inputs['Vector'])
        b=n.new('ShaderNodeBump');b.inputs['Strength'].default_value=.28;b.inputs['Distance'].default_value=bump;l.new(micro.outputs['Fac'],b.inputs['Height']);l.new(b.outputs[0],p.inputs['Normal'])
    return m

stone=material('Warm limestone - CC0 scanned sandstone',(.48,.37,.25),.82)
n=stone.node_tree.nodes;l=stone.node_tree.links;p=n.get('Principled BSDF')
coord=n.new('ShaderNodeTexCoord');anchor=bpy.data.objects.new('World-space two metre stone texture scale',None);COL['camera'].objects.link(anchor);coord.object=anchor
scale=n.new('ShaderNodeVectorMath');scale.operation='SCALE';scale.inputs[3].default_value=.53;l.new(coord.outputs['Object'],scale.inputs[0])
images={}
for role,fn in [('color','sandstone_cracks_diff_2k.jpg'),('normal','sandstone_cracks_nor_gl_2k.png'),('rough','sandstone_cracks_rough_2k.jpg'),('height','sandstone_cracks_disp_2k.png')]:
    im=bpy.data.images.load(str(ROOT/'textures'/fn));im.name='CC0 sandstone '+role
    if role!='color':im.colorspace_settings.name='Non-Color'
    im.pack();im.filepath='//textures/'+fn;images[role]=im
    t=n.new('ShaderNodeTexImage');t.image=im;t.projection='BOX';t.projection_blend=.3;l.new(scale.outputs[0],t.inputs['Vector'])
    if role=='color':
        mult=n.new('ShaderNodeMixRGB');mult.blend_type='MULTIPLY';mult.inputs[0].default_value=1;mult.inputs[2].default_value=(.86,.86,.82,1);l.new(t.outputs['Color'],mult.inputs[1]);l.new(mult.outputs[0],p.inputs['Base Color'])
    elif role=='rough':l.new(t.outputs['Color'],p.inputs['Roughness'])
    elif role=='height':
        b=n.new('ShaderNodeBump');b.inputs['Strength'].default_value=.55;b.inputs['Distance'].default_value=.065;l.new(t.outputs['Color'],b.inputs['Height']);l.new(b.outputs[0],p.inputs['Normal'])
stone_dark=stone.copy();stone_dark.name='Damp shadowed limestone'
for node in stone_dark.node_tree.nodes:
    if node.type=='MIX_RGB':node.inputs[2].default_value=(.36,.39,.30,1)
bronze=noise_color(material('Aged warm bronze with patina',(.24,.13,.035),.3,.8),(.055,.081,.06),(.39,.22,.065),7,.016)
gold=material('Polished worn bronze edges',(.54,.31,.095),.28,.78)
recess=material('Deep weathered incised grooves',(.045,.048,.034),.95)
rune=material('Turquoise recessed rune inlays',(.012,.28,.31),.34,.3,.55)
teal=material('Ancient blue green woven cloth',(.025,.105,.119),.94)
noise_color(teal,(.008,.035,.042),(.038,.12,.14),5,.003)
clay=noise_color(material('Ochre ceramic body',(.34,.14,.035),.5),(.12,.064,.025),(.46,.25,.075),5,.008)
clay_blue=material('Faded blue painted ceramic motifs',(.031,.07,.089),.6)
dirt=noise_color(material('Earth between flagstones',(.12,.097,.048),.95),(.043,.035,.018),(.16,.13,.066),18,.04)
water=material('Clear reflective rainwater',(.17,.25,.2),.06)
wp=water.node_tree.nodes.get('Principled BSDF');wp.inputs['Transmission Weight'].default_value=.85;wp.inputs['IOR'].default_value=1.333
nt=water.node_tree;n=nt.nodes;l=nt.links;tex=n.new('ShaderNodeTexNoise');tex.inputs['Scale'].default_value=45;tex.inputs['Detail'].default_value=2;b=n.new('ShaderNodeBump');b.inputs['Strength'].default_value=.13;b.inputs['Distance'].default_value=.012;l.new(tex.outputs['Fac'],b.inputs['Height']);l.new(b.outputs[0],wp.inputs['Normal'])
leafmats=[]
for i,c in enumerate([(.045,.073,.01),(.085,.115,.018),(.15,.17,.025),(.039,.062,.015),(.095,.135,.042)]):
    m=noise_color(material('Ivy leaf '+str(i),c,.42),tuple(x*.55 for x in c),tuple(x*1.25 for x in c),4,.002)
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Subsurface Weight'].default_value=.075;p.inputs['Subsurface Radius'].default_value=(.5,.7,.2)
    leafmats.append(m)
stemmat=material('Woody vine stems',(.07,.065,.022),.85)
grassmats=[material('Grass olive '+str(i),c,.7) for i,c in enumerate([(.13,.16,.025),(.23,.21,.055),(.08,.12,.02),(.28,.22,.07)])]

# World: HDR clouds and reflections, with a separate cinematic key light.
S.world=bpy.data.worlds.new('CC0 Kloppenheim golden cloud sky');S.world.use_nodes=True
nt=S.world.node_tree;n=nt.nodes;l=nt.links;n.clear();out=n.new('ShaderNodeOutputWorld');env=n.new('ShaderNodeTexEnvironment')
im=bpy.data.images.load(str(ROOT/'textures/kloppenheim_06_puresky_2k.hdr'));im.pack();im.filepath='//textures/kloppenheim_06_puresky_2k.hdr';env.image=im
coord=n.new('ShaderNodeTexCoord');mapping=n.new('ShaderNodeMapping');mapping.inputs['Rotation'].default_value=(radians(-12),0,radians(-152));l.new(coord.outputs['Normal'],mapping.inputs['Vector']);l.new(mapping.outputs['Vector'],env.inputs['Vector'])
bg=n.new('ShaderNodeBackground');bg.inputs['Strength'].default_value=.8;l.new(env.outputs[0],bg.inputs[0]);l.new(bg.outputs[0],out.inputs['Surface'])

# Individual irregular flagstones, open joints, shallow reflective puddles.
box('Courtyard earth bed',(0,-5,-.5),(55,27,1.0),dirt,0,'floor')
puddle_centers=[(-.55,-9.1,1.05,.95),(-3.9,-5.5,1.05,.38),(5.6,-1.4,.9,.4)]
for row in range(21):
    y=-14+row*1.05
    for col in range(23):
        x=-19+col*1.7+(row%2)*.85
        if x*x+(y-2)**2<4.0**2:continue
        wet=any(((x-px)/rx)**2+((y-py)/ry)**2<.65 for px,py,rx,ry in puddle_centers)
        h=R.uniform(.125,.14);z=-.014 if wet else .0
        o=box('Flagstone_%02d_%02d'%(row,col),(x+R.uniform(-.06,.06),y+R.uniform(-.04,.04),z),(R.uniform(1.65,1.69),R.uniform(1.005,1.035),h),stone_dark if wet else stone,R.uniform(.013,.025),'floor',R.uniform(-.006,.006),.015)
        if R.random()<.13 and not wet and -11<y<3:
            path('Hairline crack',[(x-.6,y-.23,.1),(x-.2,y+.04,.105),(x+.06,y+.12,.103),(x+.48,y+.41,.103)],recess,.005,'floor')
for j,(x,y,rx,ry) in enumerate(puddle_centers):
    vs=[(x,y,.075)]+[(x+rx*(1+R.uniform(-.13,.13))*cos(2*pi*i/60),y+ry*(1+R.uniform(-.13,.13))*sin(2*pi*i/60),.075) for i in range(60)]
    mesh('Shallow irregular reflecting puddle '+str(j),vs,[(0,i+1,(i+1)%60+1) for i in range(60)],water,'floor')
for x,y,r in [(3.1,-5.6,.68),(-2.4,-3.2,.56),(5.1,-3.0,.56)]:
    for dr in [0,.05,.14]:path('Inlaid pavement compass',[(x+(r-dr)*cos(t*2*pi/100),y+(r-dr)*sin(t*2*pi/100),.115) for t in range(100)],recess,.012,'floor',True)
    for angle in [0,pi/2,pi,3*pi/2]:path('Pavement engraving',[(x+r*.75*cos(angle),y+r*.75*sin(angle),.116),(x+r*.12*cos(angle+.4),y+r*.12*sin(angle+.4),.116),(x+r*.75*cos(angle+.85),y+r*.75*sin(angle+.85),.116)],recess,.009,'floor')

# Three courses of individually chipped circular sanctuary stones.
cy=2.0
for tier,(rad,z,h) in enumerate([(4.85,.06,.19),(4.33,.25,.23),(3.68,.48,.28)]):
    disc('Sanctuary tier foundation '+str(tier),(0,cy,z),rad-.07,h,stone_dark,'plinth',96)
    count=[54,48,44][tier]
    for i in range(count):
        a0=2*pi*i/count+.004;a1=2*pi*(i+1)/count-.004
        # Horizontal segment mesh uses the same wedge topology rotated onto XY.
        o=wedge('Hand laid sanctuary step %d %d'%(tier,i),(0,cy,z+h/2),rad-.5,rad,a0,a1,h-.012,stone,'plinth',.025,2)
        o.rotation_euler.x=pi/2
for i in range(32):
    a=2*pi*i/32;rr=R.uniform(3.8,4.6)
    if R.random()<.7:box('Collapsed altar fragments',(rr*cos(a),cy+rr*sin(a),.35),(R.uniform(.3,.65),R.uniform(.25,.55),R.uniform(.2,.4)),stone,.04,'plinth',a+.3,.04)
# Front ornamental relief plaque.
box('Front relief lintel',(-.1,-1.45,.63),(2.65,.38,.63),stone,.04,'plinth')
box('Front relief recessed field',(-.1,-1.656,.64),(2.32,.024,.41),stone_dark,.015,'plinth')
for cx in [-.85,-.1,.65]:
    path('Front relief lozenge',[(cx,-1.68,.48),(cx+.32,-1.68,.64),(cx,-1.68,.82),(cx-.32,-1.68,.64)],gold,.021,'plinth',True)
    path('Front relief scroll',[(cx-.28,-1.69,.55),(cx-.17,-1.69,.58),(cx-.1,-1.69,.74),(cx,-1.69,.78),(cx+.12,-1.69,.72),(cx+.2,-1.69,.59),(cx+.29,-1.69,.55)],bronze,.013,'plinth')

# Hero broken astrolabe: three masonry bands, recessed glowing track, bronze straps.
CENTER=(0,2.15,3.62)
start,end=radians(156),radians(471)
N=41
for row in range(3):
    ri=2.15+row*.245;ro=ri+.239
    for i in range(N):
        a=start+(end-start)*i/N+.0025;b=start+(end-start)*(i+1)/N-.0025
        d=.64+R.uniform(-.055,.055)
        o=wedge('Astrolabe weathered block %d %02d'%(row,i),CENTER,ri+R.uniform(-.013,.013),ro+R.uniform(-.012,.018),a,b,d,stone,'hero',.018,2)
# Continuous rear core gives depth without unnaturally uniform front joints.
wedge('Astrolabe dark inner track',(0,2.12,3.62),2.13,2.23,start+.025,end-.025,.70,bronze,'hero',.01,140)
wedge('Turquoise inner channel',(0,1.765,3.62),2.205,2.255,start+.07,end-.07,.012,rune,'hero',.004,140)
for rad in [2.28,2.82]:wedge('Bronze protective rim',(0,1.804,3.62),rad,rad+.027,start+.018,end-.018,.055,bronze,'hero',.004,140)
for deg in [162,226,299,391,444]:
    a=radians(deg)
    wedge('Bronze structural collar',CENTER,2.10,2.93,a-.064,a+.064,.79,bronze,'hero',.024,4)
    for side in [-.05,.05]:wedge('Collar gilded piping',(0,2.15,3.62),2.09,2.94,a+side-.009,a+side+.009,.82,gold,'hero',.008,2)
    for rad in [2.23,2.72,2.85]:
        pos=(rad*cos(a),1.716,3.62+rad*sin(a))
        o=lathe('Hammered collar rivet',pos,[(.044,0),(.04,.022),(.022,.045)],gold,12,'hero');o.rotation_euler.x=pi/2
for i in range(25):
    a=start+.15+i*(end-start-.3)/24;rr=2.57
    if any(abs(a-radians(d))<.10 for d in [162,226,299,391,444]):continue
    x=rr*cos(a);z=3.62+rr*sin(a)
    tangent=Vector((-sin(a),0,cos(a)));radial=Vector((cos(a),0,sin(a)));p=Vector((x,1.807,z))
    shape=[(-.12,0),(0,.09),(.12,0),(0,-.09),(-.12,0)] if i%3==0 else [(-.11,-.06),(-.11,.06),(.03,.06),(.03,-.045),(.12,-.045)]
    path('Engraved astrolabe glyph',[p+tangent*t+radial*r for t,r in shape],rune,.014)
# Additional radial chips on broken ends.
for a in [start,end]:
    for i in range(7):
        rr=R.uniform(2.25,2.8);p=(rr*cos(a),2.1+R.uniform(-.16,.16),3.62+rr*sin(a))
        box('Broken tip stone chip',p,(.22,.32,.19),stone,.025,'hero',R.random(),.06)

# Carved altar bowl and bronze ribs.
profile=[(.54,.75),(.56,.82),(.5,.91),(.37,1.06),(.31,1.35),(.39,1.55),(.62,1.8),(.80,1.93),(.82,2.02),(.68,2.08),(.6,1.98)]
lathe('Fluted stone crystal altar',(0,1.80,0),profile,stone,72,'hero')
for z,rad,h in [(.79,.58,.09),(1.91,.8,.08),(2.055,.73,.04)]:lathe('Altar bronze moulding',(0,1.8,z),[(rad,0),(rad,h)],bronze,72,'hero')
for i in range(12):
    a=2*pi*i/12
    path('Altar sculpted bronze rib',[(r*cos(a),1.8+r*sin(a),z) for r,z in [(.51,.85),(.38,1.04),(.34,1.32),(.40,1.55),(.62,1.78),(.8,1.96)]],bronze,.038)
    p=Vector((.71*cos(a),1.8+.71*sin(a),1.97));u=Vector((-sin(a),cos(a),0));v=Vector((0,0,1))
    path('Altar rim carved diamond',[p+u*-.10,p+v*.10,p+u*.10,p-v*.1,p+u*-.1],gold,.015)
for sign in [-1,1]:
    pts=[(sign*(.63+.57*t-.08*t*t),1.88+.07*sin(t*pi),1.95+.74*t+.12*t*t) for t in [i/22 for i in range(23)]]
    path('Bronze ascending crescent claw',pts,bronze,.075)
    path('Claw turquoise inset',[(x,y-.075,z+.015) for x,y,z in pts],rune,.018)
lathe('Luminous altar pool',(0,1.8,2.026),[(.59,0),(.59,.012)],material('Altar energy pool',(.008,.57,.48),.22,.0,2.6),64,'hero')

# Clear many-faceted floating quartz, retaining transparent dark facets.
crystalmat=material('Aquamarine refractive crystal',(.10,.55,.46),.055)
p=crystalmat.node_tree.nodes.get('Principled BSDF');p.inputs['Transmission Weight'].default_value=.96;p.inputs['IOR'].default_value=1.46;p.inputs['Coat Weight'].default_value=.28;p.inputs['Emission Color'].default_value=(.004,.3,.24,1);p.inputs['Emission Strength'].default_value=.13
absorb=crystalmat.node_tree.nodes.new('ShaderNodeVolumeAbsorption');absorb.inputs['Color'].default_value=(.04,.62,.49,1);absorb.inputs['Density'].default_value=.19;crystalmat.node_tree.links.new(absorb.outputs[0],crystalmat.node_tree.nodes.get('Material Output').inputs['Volume'])
crystaldark=crystalmat.copy();crystaldark.name='Deep aquamarine facet';crystaldark.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.025,.23,.24,1)
def crystal(name,loc,width,height,group='hero',rotation=0):
    vs=[(0,0,-height*.5)];nr=7
    for j,(z,scale,twist) in enumerate([(-.2,.82,0),(.12,1,.16),(.29,.62,.02)]):
        for k in range(nr):
            a=2*pi*k/nr+twist+rotation;rr=width*.5*scale*(1+R.uniform(-.08,.08));vs.append((rr*cos(a),rr*sin(a),height*z))
    vs.append((width*.025,-width*.02,height*.5));top=len(vs)-1;fs=[]
    for i in range(nr):fs.append((0,1+(i+1)%nr,1+i))
    for j in range(2):
        for i in range(nr):
            a=1+j*nr+i;b=1+j*nr+(i+1)%nr;c=1+(j+1)*nr+(i+1)%nr;d=1+(j+1)*nr+i
            fs.extend([(a,b,c),(a,c,d)])
    for i in range(nr):fs.append((1+2*nr+i,1+2*nr+(i+1)%nr,top))
    o=mesh(name,vs,fs,[crystalmat,crystaldark],group,[1 if R.random()<.20 else 0 for _ in fs]);o.location=loc
    bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.recalc_face_normals(bm,faces=bm.faces);bm.to_mesh(o.data);bm.free()
    return o
crystal('Crystal',(0,1.70,3.89),1.2,2.6,rotation=.18)
for x,y,z,w,h in [(-1.0,1.8,3.42,.16,.32),(1.1,2.0,3.45,.18,.35),(-.74,1.65,2.82,.10,.2),(.80,1.70,2.77,.12,.24)]:crystal('Orbiting crystal fragment',(x,y,z),w,h)
# Hairline internal fracture planes, not large opaque luminous cracks.
fracture=material('Subtle crystal internal fractures',(.045,.36,.3),.18,.15,.16)
for i in range(12):
    z=R.uniform(2.94,4.68);x=R.uniform(-.35,.35)
    path('Quartz fracture filament',[(x-.09,1.5,z-.1),(x+.045,1.42,z),(x+.15,1.44,z+.12)],fracture,.003)
mote=material('Tiny cyan energy motes',(.04,.8,.7),.32,0,3)
vs=[];fs=[]
for i in range(180):
    a=R.uniform(0,2*pi);r=R.uniform(.3,1.3);z=R.uniform(2.12,3.6);x=r*cos(a);y=1.7+r*.35*sin(a);size=R.uniform(.0025,.008);b=len(vs)
    vs.extend([(x-size,y,z),(x,y,z+size*2),(x+size,y,z),(x,y,z-size*1.1)]);fs.append((b,b+1,b+2,b+3))
mesh('Drifting energy sparkles',vs,fs,mote,'hero')

# Arcade pillars with coursed blocks, mouldings, recessed relief and voussoirs.
def pillar(name,x,y,h,width=1.05,depth=1.3,ornament=True):
    for z,w,d,hh in [(.12,width*1.5,depth*1.24,.24),(.35,width*1.33,depth*1.13,.19),(.54,width*1.15,depth*1.04,.15),(h-.18,width*1.23,depth*1.1,.15),(h+.02,width*1.42,depth*1.2,.18),(h+.18,width*1.5,depth*1.25,.15)]:box(name+' moulding',(x,y,z),(w,d,hh),stone,.035,'arch',wear=.012)
    courses=max(3,int((h-.7)/.46))
    for j in range(courses):
        z=.65+(h-.95)*(j+.5)/courses
        box(name+' masonry course '+str(j),(x,y,z),(width,depth,(h-.95)/courses-.018),stone,.026,'arch',wear=.023)
    if ornament:
        z=.65+(h-.8)/2;hh=h-1.2
        box(name+' relief inset',(x,y-depth/2-.014,z),(.65,.023,hh),stone_dark,.04,'arch')
        for dx in [-.39,.39]:box(name+' relief border',(x+dx,y-depth/2-.052,z),(.064,.074,hh+.13),stone,.019,'arch')
        pz=h-1.0
        for s in [1,.66]:path(name+' sunken diamond',[(x,y-depth/2-.075,pz+.45*s),(x+.28*s,y-depth/2-.075,pz),(x,y-depth/2-.075,pz-.45*s),(x-.28*s,y-depth/2-.075,pz)],gold if s<1 else stone,.025,'arch',True)
        for dx in [-.19,.0,.19]:path(name+' vertical flute',[(x+dx,y-depth/2-.065,.95),(x+dx*.6,y-depth/2-.085,pz-.65),(x+dx*.35,y-depth/2-.075,pz-.35)],stone,.018,'arch')

def arcade(name,cx,y,r,spring,depth=1.35):
    for sign in [-1,1]:pillar(name+(' west' if sign<0 else ' east'),cx+sign*(r+.3),y,spring,1.05,depth)
    for row in range(2):
        n=19
        for i in range(n):wedge(name+' voussoir',(cx,y,spring),r+row*.28,r+(row+1)*.28-.012,pi*i/n+.004,pi*(i+1)/n-.004,depth,stone,'arch',.028,2)
    wedge(name+' inner archivolt',(cx,y-depth/2-.04,spring),r-.065,r+.018,0,pi,.09,stone,'arch',.015,50)
    # Incomplete masonry crest creates a broken silhouette.
    for i in range(11):
        x=cx+(i-5)*.51;z=spring+math.sqrt(max(.01,(r+.59)**2-(x-cx)**2))+.16
        if R.random()<.70:box(name+' broken crest',(x,y+.08,z),(.53,depth*.84,R.uniform(.23,.48)),stone,.028,'arch',R.uniform(-.1,.1),.045)
arcade('Western foreground arcade',-8.98,1.1,2.48,4.97)
arcade('Western receding arcade',-4.26,6.2,2.12,4.63,1.12)
arcade('Eastern sanctuary arcade',6.35,5.0,2.44,4.7)
pillar('Eastern foreground pier',7.6,-.1,7.3,1.18,1.6)
# Back wall broken towers, set well behind the hero silhouette.
def tower(name,x,y,z,h,w):
    for j in range(int(h/.52)):
        box(name+' course',(x,y,z+(j+.5)*.52),(w,1.28,.5),stone,.025,'distance',wear=.022)
    for dx in [-w*.5,0,w*.5]:box(name+' crown',(x+dx,y,z+h+.13),(w*.34,1.4,.52),stone,.025,'distance',wear=.05)
    box(name+' crown ledge',(x,y,z+h-.13),(w*1.28,1.55,.2),stone,.03,'distance')
    box(name+' inset lancet',(x,y-.655,z+h-1.3),(w*.34,.032,1.44),recess,.06,'distance')
    for side in [-1,1]:box(name+' lancet edge',(x+side*w*.25,y-.7,z+h-1.3),(.09,.07,1.46),stone,.015,'distance')
for i,(x,y,z,h,w) in enumerate([(-2.5,12,1.5,5.8,.85),(1.2,15,1.8,7.2,1.0),(4.0,16,2.2,6.4,1.1),(8,17,2.0,7.6,1.2),(11.5,17,1.9,6.7,1.0)]):tower('Distant ruined spire '+str(i),x,y,z,h,w)
for x in [4.8,6.4,8.0,9.6,11.2,12.8]:
    box('Upper ruin parapet',(x,15,7.2),(1.62,1.0,.55),stone,.05,'distance',wear=.06)
    wedge('Upper ruin open arcade',(x,15,7.8),.5,.68,0,pi,.8,stone,'distance',.02,18)
    for sx in [-.60,.60]:box('Upper ruin pilaster',(x+sx,15,7.55),(.2,.8,.8),stone,.03,'distance')
# Broad stair flights rising to the arcades.
for label,x,y in [('West',-7.15,3.8),('East',5.25,4.1)]:
    for i in range(7):box(label+' stair '+str(i),(x,y+i*.43,.12+i*.19),(3.6,.48,.26),stone,.035,'arch',wear=.025)
    for side in [-1,1]:
        for i in range(4):box(label+' stair side ruin',(x+side*1.99,y+i*.63,.4+i*.25),(.6,.7,.65),stone,.03,'arch',wear=.06)

# Wind-shaped pointed banners and embroidered gold emblems.
def banner(name,x,y,top,width,height):
    vs=[];fs=[];nx=24;nz=40
    def pt(u,v):return (x+(u-.5)*width,y+.10*sin(2*pi*u+v*5)+.12*sin(v*4),top-v*height-(max(0,v-.81)/.19)*(.50-abs(u-.5))*.50)
    for j in range(nz+1):
        for i in range(nx+1):vs.append(pt(i/nx,j/nz))
    for j in range(nz):
        for i in range(nx):a=j*(nx+1)+i;fs.append((a,a+1,a+nx+2,a+nx+1))
    ob=mesh(name,vs,fs,teal,'props');solid=ob.modifiers.new('Woven cloth thickness','SOLIDIFY');solid.thickness=.006
    for side in [.045,.955]:path(name+' gold stitched edge',[tuple(Vector(pt(side,j/50))+Vector((0,-.012,0))) for j in range(51)],gold,.011,'props')
    outline=[(.5,.33),(.69,.51),(.5,.76),(.31,.51),(.5,.33)]
    for sc in [1,.6]:path(name+' embroidered lozenge',[tuple(Vector(pt(.5+(u-.5)*sc,.53+(v-.53)*sc))+Vector((0,-.022,0))) for u,v in outline],gold,.014,'props')
    path(name+' emblem axis',[tuple(Vector(pt(.5,v))+Vector((0,-.025,0))) for v in [.26,.80]],gold,.01,'props')
    path(name+' hanging rod',[(x-width*.57,y,top+.03),(x+width*.57,y,top+.03)],bronze,.036,'props')
banner('Western blue sanctuary banner',-2.9,5.43,5.0,.9,3.6)
banner('Eastern blue sanctuary banner',7.43,3.76,6.4,1.02,3.95)

# Painted handled vessels, with geometry-assigned glaze pattern.
def vase(name,x,y,h,rad):
    prof=[]
    for j in range(51):
        v=j/50
        rr=rad*(.43+.59*sin(pi*min(1,v*1.27)) if v<.74 else .40+.19*((v-.74)/.26))
        if v>.96:rr+=rad*.08
        prof.append((rr,h*v))
    def pattern(u,z):
        v=z/h
        if .08<v<.14 or .67<v<.71 or .89<v<.93:return 1
        if .2<v<.62:
            a=(u*10)%1-.5;b=((v-.2)/.42*2)%1-.5
            return int(abs(a)+abs(b)<.43 or (abs(a)<.045 and abs(b)<.47))
        return 0
    lathe(name,(x,y,.12),prof,[clay,clay_blue],100,'props',pattern)
    for sign in [-1,1]:
        path(name+' curved handle',[(x+sign*rad*(.46+.53*sin(t*pi)),y,h*(.84-.40*t)+.12) for t in [i/30 for i in range(31)]],clay,.052,'props')
    lathe(name+' mouth interior',(x,y,.12+h),[(rad*.59,0),(rad*.47,-.06)],clay_blue,80,'props')
for data in [('West foreground amphora',-8.5,-.6,1.55,.60),('West stairs vessel',-5.72,1.2,1.07,.45),('Small west vessel',-4.93,1.0,.6,.28),('East foreground amphora',6.12,-.55,1.55,.60),('East steps vessel',5.4,3.6,.85,.37)]:vase(*data)

# Dense but fine ivy leaves, each a folded nine-point silhouette.
LEAF_V=[];LEAF_F=[];LEAF_M=[];LEAF_UV=[]
def leaf(center,size,a,tilt=0,mi=None):
    c=Vector(center);u=Vector((cos(a),sin(a),.05))*size;v=Vector((-sin(a)*sin(tilt),cos(a)*sin(tilt),cos(tilt)))*size*1.35
    normal=u.cross(v).normalized();outline=[(0,-.72),(-.32,-.48),(-.54,-.18),(-.65,.10),(-.66,.32),(-.49,.44),(-.45,.70),(-.26,.75),(-.13,.60),(0,1),(.13,.60),(.26,.75),(.45,.70),(.49,.44),(.66,.32),(.65,.10),(.54,-.18),(.32,-.48)]
    b=len(LEAF_V);LEAF_V.append(tuple(c+normal*size*.12))
    LEAF_V.extend(tuple(c+u*x+v*y+normal*size*.06*sin(y*pi)) for x,y in outline)
    LEAF_UV.append((.5,.43));LEAF_UV.extend((x*.7+.5,(y+.72)/1.72) for x,y in outline)
    idx=R.randrange(len(leafmats)) if mi is None else mi
    for i in range(len(outline)):LEAF_F.append((b,b+1+i,b+1+(i+1)%len(outline)));LEAF_M.append(idx)

def ivy_strand(name,points,density=9,leafsize=.12):
    path(name+' stem',points,stemmat,.012,'plants')
    for a,b in zip(points,points[1:]):
        a=Vector(a);b=Vector(b);length=(b-a).length
        for j in range(max(1,int(length*density))):
            p=a+(b-a)*R.random();p+=Vector((R.uniform(-.18,.18),R.uniform(-.14,.14),R.uniform(-.12,.12)))
            leaf(p,R.uniform(.68,1.13)*leafsize,R.uniform(-.7,.7),R.uniform(-.6,.6))
            if R.random()<.8:leaf(p+Vector((R.uniform(-.18,.18),R.uniform(-.12,.12),-.08)),leafsize*R.uniform(.65,1),R.uniform(-1,1),R.uniform(-.7,.7))
for cx,y,r,h in [(-8.98,1.1,2.48,4.97),(-4.26,6.2,2.12,4.63),(6.35,5,2.44,4.7)]:
    for k in range(7):
        pts=[(cx+(r+.40+R.uniform(-.1,.1))*cos(a),y-.76+R.uniform(-.18,.03),h+(r+.40)*sin(a)) for a in [pi*i/42 for i in range(43)]]
        ivy_strand('Arch climbing ivy',pts,6,.13)
    for px in [cx-r-.3,cx+r+.3]:
        for k in range(4):
            pts=[(px+(-1 if k%2 else 1)*R.uniform(.37,.54),y-.73+R.uniform(-.13,.02),h+.2-i*.38) for i in range(14)]
            ivy_strand('Pillar edge ivy',pts,9,.13)
    for k in range(9):
        a=R.uniform(.1,3.02);x=cx+(r+.15)*cos(a);z=h+(r+.15)*sin(a)
        ivy_strand('Loose hanging ivy',[(x+.05*sin(j),y-.87,z-j*.25) for j in range(R.randint(3,10))],8,.11)
for k in range(10):
    pts=[(2.74*cos(a)+R.uniform(-.08,.08),1.65+R.uniform(-.16,.04),3.62+2.74*sin(a)) for a in [radians(90-i*6) for i in range(27)]]
    ivy_strand('Astrolabe right climbing ivy',pts,6,.10)
for k in range(12):
    x=R.uniform(1.6,2.9);z=R.uniform(1.2,4.2)
    ivy_strand('Astrolabe draping vine',[(x+.1*sin(j*.7),1.52,z-j*.22) for j in range(R.randint(4,11))],8,.085)
# Clumps on altar edges, stairs, wall tops and foreground frame.
for i in range(190):
    a=R.uniform(0,2*pi);r=R.uniform(3.25,4.65);x=r*cos(a);y=cy+r*sin(a)
    if y<-.5 and abs(x)<1.55:continue
    z=.65 if r<3.6 else .30
    for j in range(R.randint(6,13)):leaf((x+R.uniform(-.3,.3),y+R.uniform(-.22,.22),z+R.uniform(.01,.25)),R.uniform(.075,.14),R.uniform(-pi,pi),R.uniform(-1.2,1.2))
for px,py,z in [(-9,-4,.4),(-8.6,-8.4,.2),(-4.95,-10.1,.3),(3.5,-9.7,.2),(7.2,-4,.2),(-6.4,1.1,.2),(6.8,.5,.1)]:
    for j in range(150):leaf((px+R.gauss(0,.52),py+R.gauss(0,.36),z+R.random()*.8),R.uniform(.11,.21),R.uniform(-pi,pi),R.uniform(-1.3,1.3))
foliage=mesh('Folded ivy leaves - individually modeled',LEAF_V,LEAF_F,leafmats,'plants',LEAF_M)
uv=foliage.data.uv_layers.new(name='Leaf local coordinates')
for poly in foliage.data.polygons:
    poly.use_smooth=True
    for li in poly.loop_indices:uv.data[li].uv=LEAF_UV[foliage.data.loops[li].vertex_index]
for mat in leafmats:
    nt=mat.node_tree;n=nt.nodes;l=nt.links;p=n.get('Principled BSDF')
    uvnode=n.new('ShaderNodeTexCoord');sep=n.new('ShaderNodeSeparateXYZ');l.new(uvnode.outputs['UV'],sep.inputs[0])
    sub=n.new('ShaderNodeMath');sub.operation='SUBTRACT';sub.inputs[1].default_value=.5;l.new(sep.outputs['X'],sub.inputs[0])
    ab=n.new('ShaderNodeMath');ab.operation='ABSOLUTE';l.new(sub.outputs[0],ab.inputs[0])
    vein=n.new('ShaderNodeMath');vein.operation='LESS_THAN';vein.inputs[1].default_value=.009;l.new(ab.outputs[0],vein.inputs[0])
    bump=n.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.4;bump.inputs['Distance'].default_value=.003;l.new(vein.outputs[0],bump.inputs['Height']);l.new(bump.outputs[0],p.inputs['Normal'])
    trans=n.new('ShaderNodeBsdfTranslucent');trans.inputs['Color'].default_value=(.19,.25,.065,1)
    mix=n.new('ShaderNodeMixShader');mix.inputs[0].default_value=.20;l.new(p.outputs[0],mix.inputs[1]);l.new(trans.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],n.get('Material Output').inputs['Surface'])
# Thin bending grass blades with earthy variation.
gv=[];gf=[];gm=[]
for i in range(1500):
    x=R.uniform(-14,12);y=R.uniform(-13,12)
    if x*x+(y-cy)**2<3.3**2:continue
    if abs(x)<4 and -10<y<0 and R.random()<.64:continue
    for j in range(R.randint(9,16)):
        a=R.random()*2*pi;h=R.uniform(.10,.32);w=R.uniform(.014,.032);bend=R.uniform(.07,.25);xx=x+R.uniform(-.1,.1);yy=y+R.uniform(-.1,.1);b=len(gv)
        for t in [0,.4,.75,1]:
            cx=xx+bend*t*t*cos(a);cyy=yy+bend*t*t*sin(a)
            for sign in [-1,1]:gv.append((cx+sign*w*(1-t)*sin(a),cyy-sign*w*(1-t)*cos(a),.09+h*t))
        for k in range(3):gf.append((b+2*k,b+2*k+1,b+2*k+3,b+2*k+2));gm.append(R.randrange(4))
mesh('Courtyard fine grass blades',gv,gf,grassmats,'plants',gm)
# Foreground collapsed masonry produces a natural shallow depth-of-field frame.
for side in [-1,1]:
    for i in range(13):
        x=(-4.85 if side<0 else 2.9)+R.uniform(-.7,.8);y=R.uniform(-10.3,-7.8);z=R.uniform(.12,.37)
        box('Foreground tumbled limestone',(x,y,z),(R.uniform(.7,1.2),R.uniform(.5,.9),R.uniform(.3,.65)),stone,.07,'floor',R.uniform(-.35,.35),.12)

# Layered craggy landscape meshes, distant ruined aqueduct and translucent falls.
rockmat=noise_color(material('Distant weathered cliff',(.23,.22,.18),.95),(.12,.15,.12),(.45,.41,.29),9,.15)
mountmat=noise_color(material('Atmospheric distant ridge',(.28,.29,.27),1),(.20,.26,.26),(.42,.39,.30),6,.09)
def landscape(name,y,base,amp,mat,width=150,depth=26,nx=180,ny=18):
    vs=[];fs=[]
    for j in range(ny+1):
        yy=y+j*depth/ny
        for i in range(nx+1):
            x=-width/2+i*width/nx
            silhouette=.25+.48*abs(noise.fractal(Vector((x*.055,yy*.045,1.7)),1.1,2.1,4))+.25*sin(x*.12+.3)**2
            zz=base+amp*silhouette+noise.fractal(Vector((x*.3,yy*.22,.4)),1.1,2.2,3)*1.3
            vs.append((x,yy,zz))
    for j in range(ny):
        for i in range(nx):a=j*(nx+1)+i;fs.extend([(a,a+1,a+nx+2),(a,a+nx+2,a+nx+1)])
    return mesh(name,vs,fs,mat,'distance')
landscape('Far mountain ridges',78,-2,14,mountmat,190,40)
landscape('Clifftop valley',36,-3,10,rockmat,120,32)
# Sheer cliff faces are irregular sculpted columns, giving waterfalls a drop.
for x,y,h,w in []:
    for i in range(9):
        xx=x+(i-4)*w/8
        box('Eroded valley cliff',(xx,y+R.uniform(-.45,.45),h/2-3),(w/7,R.uniform(2,4),h),rockmat,.24,'distance',R.uniform(-.12,.12),.35)
fallmat=material('Silvery translucent waterfall',(.69,.82,.83),.23,0,.15)
fp=fallmat.node_tree.nodes.get('Principled BSDF');fp.inputs['Transmission Weight'].default_value=.24
for x,y,z,h,w in [(-11.0,24.5,5.8,12,1.65),(-7.5,28.5,5.2,12,1.25),(12.0,29.4,6.7,14,1.7)]:
    for strip in range(14):
        xx=x+(strip-6.5)*w/14;vs=[];fs=[]
        for j in range(36):
            t=j/35;zj=z-t*h;offset=.025*sin(t*12+strip)+.009*sin(t*41+strip)
            for sg in [-1,1]:vs.append((xx+offset+sg*w/35*(1+.4*sin(t*20)),y-.9*t*t,zj))
        for j in range(35):fs.append((2*j,2*j+1,2*j+3,2*j+2))
        mesh('Veiled waterfall stream',vs,fs,fallmat,'distance')
for i in range(7):
    x=-30+i*3.25;y=35
    box('Aqueduct pier',(x,y,0.3),(1.0,1.5,7.5),stone,.05,'distance')
    wedge('Aqueduct arch',(x+1.63,y,2.0),1.18,1.55,0,pi,1.3,stone,'distance',.04,18)
    box('Aqueduct parapet',(x+1.63,y,4.0),(3.4,1.65,.75),stone,.04,'distance')
for x,y,z,h,w in [(-19,40,6,4.6,2.4),(-21,42,6,6,1.7),(-16.5,42,7,4.8,1.4)]:tower('Valley citadel',x,y,z,h,w)
# Cypresses use detailed branch silhouettes instead of cone primitives.
cv=[];cf=[];cm=[]
def cypress(x,y,z,h,r):
    path('Cypress trunk',[(x,y,z),(x,y,z+h*.92)],stemmat,.065,'plants')
    for k in range(1500):
        t=R.random();ang=R.random()*2*pi;rr=r*(sin(pi*t)**.6)*R.random();c=Vector((x+rr*cos(ang),y+rr*sin(ang),z+h*t));size=R.uniform(.045,.085)
        b=len(cv);cv.extend([tuple(c+Vector((-size,0,-size))),tuple(c+Vector((size,0,-size))),tuple(c+Vector((0,-size*.7,0))),tuple(c+Vector((0,0,size*2.3)))])
        cf.extend([(b,b+1,b+3),(b+1,b+2,b+3),(b+2,b,b+3)]);cm.extend([R.randrange(4)]*3)
for p in [(-10.0,8,0,6.2,.64),(-12.8,12,-.2,5.4,.57),(-6.9,16,0,4,.45),(9,18,0,5.5,.5),(-16,27,6,3.8,.38),(-20,30,7,3.5,.4)]:cypress(*p)
mesh('Cypress clustered branch sprays',cv,cf,leafmats,'plants',cm)

# CC0 photogrammetry is appended locally and every texture is packed for relocation.
def load_cc0(asset,predicate):
    folder=ROOT/'assets'/asset
    with bpy.data.libraries.load(str(folder/(asset+'_2k.blend')),link=False) as (src,dst):
        dst.objects=[name for name in src.objects if predicate(name)]
    result=[o for o in dst.objects if o and o.type=='MESH']
    for im in bpy.data.images:
        if im.source!='FILE' or im.packed_file:continue
        file=folder/'textures'/Path(bpy.path.abspath(im.filepath)).name
        if file.exists():
            im.filepath=str(file);im.reload();im.pack();im.filepath='//assets/'+asset+'/textures/'+file.name
    for ob in result:
        ob['Asset source']='https://polyhaven.com/a/'+asset
        ob['License']='CC0'
    return result

def instance(source,name,loc,scale,rotation=0,group='distance'):
    obj=source.copy();obj.data=source.data;obj.name=name
    COL[group].objects.link(obj);obj.location=loc;obj.scale=scale;obj.rotation_euler.z=rotation
    return obj
cliff=load_cc0('mountainside',lambda n:n.endswith('LOD1'))[0]
for i,(x,y,z,sx,sy,sz,a) in enumerate([(-21,35,-5,1.4,1.3,1.5,-.22),(-13,28,-6,.9,.85,1.18,.1),(-6,32,-7,.8,.85,1.2,-.3),(10,34,-6,1.1,1,1.25,.35),(17,38,-6,1.3,1,1.6,-.1)]):
    instance(cliff,'Scanned valley escarpment '+str(i),(x,y,z),(sx,sy,sz),a)
rocks=load_cc0('rock_moss_set_01',lambda n:True)
for i in range(38):
    if i<14:
        x=(-5.1 if i%2 else 3.8)+R.uniform(-.75,.75);y=R.uniform(-9.7,-5.2);scale=R.uniform(.25,.45)
    else:
        a=R.uniform(0,2*pi);r=R.uniform(4.7,5.4);x=r*cos(a);y=2+r*sin(a);scale=R.uniform(.14,.26)
    instance(rocks[i%len(rocks)],'Scanned mossy stone '+str(i),(x,y,.025),(scale,)*3,R.uniform(-pi,pi),'floor')
weeds=load_cc0('weed_plant_02',lambda n:n.endswith('LOD1'))
for i in range(350):
    if i<120:
        a=R.uniform(0,2*pi);r=R.uniform(3.7,5.0);x=r*cos(a);y=2+r*sin(a);z=.27 if r<4.35 else .12
    else:
        x=R.uniform(-8,8);y=R.uniform(-10,6);z=.10
        if x*x+(y-2)**2<4.7**2:continue
    scale=R.uniform(1.1,3.1)
    instance(weeds[i%len(weeds)],'Scanned courtyard groundcover '+str(i),(x,y,z),(scale,)*3,R.uniform(-pi,pi),'plants')

# Incised stone arabesques give the shrine hand-carved, layered relief.
for i in range(12):
    a=2*pi*i/12
    for side in [-1,1]:
        pts=[]
        for j in range(22):
            t=j/21;r=.41+.24*t;z=1.05+.73*t;az=a+side*.16*sin(t*pi)
            pts.append((r*cos(az),1.8+r*sin(az),z))
        path('Acanthus altar relief',pts,gold,.016)
for cx in [-.85,-.1,.65]:
    for sign in [-1,1]:
        pts=[(cx+sign*(.12+.16*t)*cos(2*pi*t),-1.684,.64+.13*sin(2*pi*t)) for t in [i/35 for i in range(36)]]
        path('Plaque coiled foliage carving',pts,bronze,.009,'plinth')
for deg in range(171,462,14):
    a=radians(deg);rr=2.7;p=Vector((rr*cos(a),1.798,3.62+rr*sin(a)))
    tangent=Vector((-sin(a),0,cos(a)));radial=Vector((cos(a),0,sin(a)))
    for sign in [-1,1]:
        pts=[p+tangent*(sign*.12*sin(t*pi))+radial*(.08*cos(t*pi)) for t in [j/16 for j in range(17)]]
        path('Astrolabe palmette incision',pts,recess,.006)

# Eroded stone surfaces retain editable modifiers rather than a texture-only silhouette.
wear_texture=bpy.data.textures.new('Limestone shallow erosion','CLOUDS')
wear_texture.noise_scale=.16;wear_texture.noise_depth=2
for obj in list(S.objects):
    if obj.type!='MESH' or not any(m in (stone,stone_dark) for m in obj.data.materials):continue
    if obj.users_collection[0] == COL['distance']:continue
    subdiv=obj.modifiers.new('Surface tessellation for shallow erosion','SUBSURF')
    subdiv.subdivision_type='SIMPLE';subdiv.levels=2;subdiv.render_levels=2
    erode=obj.modifiers.new('Physical weathered stone surface','DISPLACE')
    erode.texture=wear_texture;erode.texture_coords='GLOBAL';erode.strength=.035;erode.mid_level=.5

# Camera and golden-hour lighting, matched to reference screen-space landmarks.
def light(name,kind,loc,energy,color,target=None,size=1):
    d=bpy.data.lights.new(name,kind);d.energy=energy;d.color=color
    if kind=='AREA':d.shape='DISK';d.size=size
    if kind=='POINT':d.shadow_soft_size=size
    if kind=='SUN':d.angle=radians(1.0)
    ob=bpy.data.objects.new(name,d);COL['light'].objects.link(ob);ob.location=loc
    if target is not None:ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler()
    return ob
light('Low golden sun through western arcade','SUN',(-24,36,21),5.3,(1,.82,.56),(0,0,0))
light('Warm courtyard bounce','AREA',(-5,-6,6),650,(1,.78,.54),(0,2,2),7)
light('Cool open sky fill','AREA',(3,3,10),750,(.56,.72,1),(0,1,1),9)
light('Cyan light from crystal base','POINT',(0,1.4,2.43),72,(.12,1,.76),size=.55)
light('Sunlit arch edge','AREA',(-9,6,7),1700,(1,.64,.28),(-2,2,3),5)
# A gentle local mist volume gives separation without obscuring the courtyard.
fog=bpy.data.materials.new('Valley atmospheric haze');fog.use_nodes=True;n=fog.node_tree.nodes;n.clear();out=n.new('ShaderNodeOutputMaterial');vol=n.new('ShaderNodeVolumePrincipled');vol.inputs['Color'].default_value=(.72,.76,.77,1);vol.inputs['Density'].default_value=.0045;vol.inputs['Anisotropy'].default_value=.30;fog.node_tree.links.new(vol.outputs['Volume'],out.inputs['Volume'])
box('Distant atmospheric haze volume',(0,46,12),(160,56,50),fog,0,'light')
camd=bpy.data.cameras.new('Reference camera');cam=bpy.data.objects.new('Camera',camd);COL['camera'].objects.link(cam);cam.location=(-1.8,-15,2.0);cam.rotation_euler=(Vector((-2.0,2.0,2.0))-cam.location).to_track_quat('-Z','Y').to_euler();camd.lens=29.5;camd.sensor_width=36;S.camera=cam
focus=bpy.data.objects.new('Focus - hovering crystal',None);focus.location=(0,1.7,3.5);COL['camera'].objects.link(focus);camd.dof.use_dof=True;camd.dof.focus_object=focus;camd.dof.aperture_fstop=3.5
ref=bpy.data.objects.new('Concept reference - never rendered',None);ref.empty_display_type='IMAGE';ref.data=bpy.data.images.load(str(ROOT/'V1-Visual-Identity-Concept.png'));ref.data.pack();ref.hide_render=True;ref.hide_viewport=True;COL['camera'].objects.link(ref)
S.render.engine='CYCLES';S.cycles.samples=args.samples;S.cycles.use_denoising=True;S.cycles.adaptive_threshold=.035;S.cycles.max_bounces=10;S.cycles.transmission_bounces=8;S.cycles.volume_bounces=1
S.render.resolution_x=1672;S.render.resolution_y=941;S.render.resolution_percentage=50 if args.preview else 100
S.view_settings.view_transform='AgX'
try:S.view_settings.look='AgX - Medium High Contrast'
except Exception:pass
S.view_settings.exposure=.75
S.cycles.device='CPU'
if args.device == 'CUDA':
    prefs=bpy.context.preferences.addons['cycles'].preferences
    prefs.compute_device_type='CUDA';prefs.get_devices()
    if not any(device.type == 'CUDA' for device in prefs.devices):
        raise RuntimeError('No CUDA device is available; use --device CPU')
    for device in prefs.devices:device.use=device.type=='CUDA'
    S.cycles.device='GPU'
# Persist the same masonry-normal correction used by the reviewed draft preview.
for obj in S.objects:
    if obj.type=='MESH' and any(label in obj.name for label in ('Astrolabe','voussoir','archivolt','arch','sanctuary step')):
        bm=bmesh.new();bm.from_mesh(obj.data)
        bmesh.ops.recalc_face_normals(bm,faces=bm.faces)
        bm.to_mesh(obj.data);bm.free()
ref.data.filepath_raw='//V1-Visual-Identity-Concept.png'
# Subtle HDR bloom through the compositor.
tree=bpy.data.node_groups.new('Sanctuary compositing','CompositorNodeTree')
S.compositing_node_group=tree
n=tree.nodes;l=tree.links
rl=n.new('CompositorNodeRLayers')
gl=n.new('CompositorNodeGlare')
gl.inputs['Type'].default_value='Fog Glow'
gl.inputs['Quality'].default_value='High'
gl.inputs['Threshold'].default_value=1.3
gl.inputs['Strength'].default_value=.07
gl.inputs['Size'].default_value=.28
tree.interface.new_socket(name='Image',in_out='OUTPUT',socket_type='NodeSocketColor')
out=n.new('NodeGroupOutput')
l.new(rl.outputs['Image'],gl.inputs['Image']);l.new(gl.outputs['Image'],out.inputs['Image'])
S.render.image_settings.file_format='PNG';S.render.filepath=args.render
S['Reference']='V1-Visual-Identity-Concept.png; editable 3D reconstruction, visual parity requires render review.'
S['Asset license']='Poly Haven CC0 sandstone_cracks kloppenheim_06_puresky, mountainside, rock_moss_set_01 and weed_plant_02; see ASSET_SOURCES.md.'
S['Rebuild seed']=10122026
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA'
Path(args.output).parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=args.output,compress=True)
print('SCENE_SAVED '+json.dumps({'objects':len(S.objects),'vertices':sum(len(o.data.vertices) for o in S.objects if o.type=='MESH'),'packed_images':sum(bool(i.packed_file) for i in bpy.data.images)}),flush=True)
bpy.ops.render.render(write_still=True)
print('RENDER_COMPLETE '+args.render,flush=True)