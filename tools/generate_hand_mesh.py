"""Author original, sleeve-free glove meshes in classic ActorX PSK/PSA.

Centimetres; UE grip axes: X forward through the grasp, Y right, Z up.
The palm is the origin. Right palm faces -Y, left palm faces +Y. Finger
chains extend +Z in bind pose; runtime curl rotates them about local X.
Format reference (layout only, no exporter code/assets copied):
https://github.com/gildor2/UEViewer/blob/master/Exporters/Psk.h
PSK reverses Y and triangle winding when imported into UE2.
"""
import math
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
DIGITS = ('Thumb', 'Index', 'Middle', 'Ring', 'Little')
LENGTHS = ((2.8, 2.3, 1.9), (3.5, 2.4, 1.9), (3.9, 2.6, 2.0),
           (3.6, 2.4, 1.9), (2.9, 1.9, 1.6))
BASES = ((4.1, -0.1, -1.4), (3.0, 0.0, 4.1), (1.0, 0.0, 4.7),
         (-1.1, 0.0, 4.4), (-2.9, 0.0, 3.5))

def pack(fmt, *values):
    return struct.pack('<' + fmt, *values)

def chunk(tag, size=0, rows=()):
    rows = list(rows)
    assert all(len(row) == size for row in rows)
    return pack('20siii', tag.encode(), 2003321, size, len(rows)) + b''.join(rows)

def author(left):
    sign = -1 if left else 1
    points, weights, faces, bones = [], [], [], [('Palm', 0, (0, 0, 0))]
    # Elliptical ring lofts, capped and independently weighted: joint ends
    # overlap instead of borrowing skin weights from a remote shoulder.
    def loft(rings, bone, material, sides=12):
        start = len(points)
        for z, cx, cy, rx, ry in rings:
            for j in range(sides):
                angle = 2 * math.pi * j / sides
                points.append((cx + rx * math.cos(angle), sign * (cy + ry * math.sin(angle)), z))
                weights.append(bone)
        for ring in range(len(rings)-1):
            for j in range(sides):
                a = start + ring*sides+j
                b = start + ring*sides+(j+1)%sides
                c, d = b+sides, a+sides
                faces.extend(((a,b,c,material),(a,c,d,material)))
        for ring, reverse in ((0,True),(len(rings)-1,False)):
            z,cx,cy,_,_ = rings[ring]
            centre=len(points);points.append((cx,sign*cy,z));weights.append(bone)
            for j in range(sides):
                a=start+ring*sides+j;b=start+ring*sides+(j+1)%sides
                faces.append((centre,b,a,material) if reverse else (centre,a,b,material))
    # Palm, closed wrist cuff, dorsal padding. All geometry stays with Palm.
    loft([(-5.8,0,0,2.5,.95),(-4.8,0,0,2.7,1.2),(-2,0,0,3.7,1.45),
          (1.8,0,0,4.05,1.45),(3.8,0,0,3.75,1.0),(4.2,0,0,3.3,.7)],0,0)
    loft([(-6.2,0,0,2.6,1.05),(-6.0,0,0,2.85,1.3),(-4.9,0,0,2.9,1.4),
          (-4.7,0,0,2.65,1.18)],0,1)
    loft([(-2.3,0,1.1,2.7,.3),(-1.6,0,1.25,3,.5),(2.1,0,1.25,3,.5),
          (2.7,0,1.05,2.7,.3)],0,1)
    for finger,(name,lengths,base) in enumerate(zip(DIGITS,LENGTHS,BASES)):
        x,y,z=base
        radius=(.91,.88,.94,.86,.76)[finger]
        parent=0
        for joint,length in enumerate(lengths):
            bone=len(bones)
            offset=(x,sign*y,z) if joint==0 else (0,0,lengths[joint-1])
            bones.append((name+str(joint+1),parent,offset))
            r=radius*(1-.12*joint)
            loft([(z-.24,x,y,.60*r,.57*r),(z+.10,x,y,r,.90*r),
                  (z+length-.30,x,y,.86*r,.78*r),(z+length+.10,x,y,.51*r,.48*r)],bone,0,10)
            # Dorsal knuckle pad provides a readable bend direction.
            loft([(z+.15,x,y+.64*r,.62*r,.16*r),(z+.4,x,y+.74*r,.68*r,.27*r),
                  (z+length*.65,x,y+.69*r,.57*r,.20*r)],bone,1,8)
            parent=bone;z+=length
    # UE import mirrors PSK Y; cancel it here. Also reverse triangles for
    # left-hand authoring and for the format conversion, independently.
    ps_points=[(x,-y,z) for x,y,z in points]
    wedges=[];triangles=[]
    for a,b,c,material in faces:
        if left:a,b=b,a
        a,b=b,a
        first=len(wedges)
        for index in (a,b,c):
            wedges.append(pack('IffBBH',index,0.5,0.5,material,0,0))
        triangles.append(pack('HHHBBI',first,first+1,first+2,material,0,1))
    bone_rows=[]
    for index,(name,parent,(x,y,z)) in enumerate(bones):
        children=sum(i!=index and b[1]==index for i,b in enumerate(bones))
        bone_rows.append(pack('64sIii11f',name.encode(),0,children,parent,0,0,0,-1,x,-y,z,0,0,0,0))
    mesh=chunk('ACTRHEAD')+chunk('PNTS0000',12,(pack('3f',*v) for v in ps_points))
    mesh+=chunk('VTXW0000',16,wedges)+chunk('FACE0000',12,triangles)
    mesh+=chunk('MATT0000',88,(pack('64siIiIii',name.encode(),i,0,0,0,0,0) for i,name in enumerate(('Glove','Padding'))))
    mesh+=chunk('REFSKELT',120,bone_rows)
    mesh+=chunk('RAWWEIGHTS',12,(pack('fii',1,index,bone) for index,bone in enumerate(weights)))
    anim=chunk('ANIMHEAD')+chunk('BONENAMES',120,bone_rows)
    info=pack('64s64s4i3f3i',b'Idle',b'None',len(bones),1,0,len(bones),0,1,30,0,0,1)
    anim+=chunk('ANIMINFO',168,[info])
    anim+=chunk('ANIMKEYS',32,(pack('8f',x,-y,z,0,0,0,-1,1) for _,_,(x,y,z) in bones))
    assert len(points)<65536 and len(wedges)<65536 and len(bones)==16
    assert max(abs(v) for p in points for v in p)<20
    return mesh,anim,dict(vertices=len(points),triangles=len(faces),bones=len(bones))

def main():
    folder=ROOT/'unrealscript/KFVRGameplay/Models';folder.mkdir(parents=True,exist_ok=True)
    for left in (False,True):
        name='KFVRGlove'+('Left' if left else 'Right')
        mesh,anim,stats=author(left)
        (folder/(name+'.psk')).write_bytes(mesh)
        (folder/(name+'.psa')).write_bytes(anim)
        print(name,stats)

if __name__=='__main__':main()
