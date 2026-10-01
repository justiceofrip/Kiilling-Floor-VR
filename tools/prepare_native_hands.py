"""Build optional hand-only meshes from the owner's installed KF assets.

Generated game geometry/package stays in ignored local storage. Only this
converter is distributable. PSK structure and mirror convention reference:
https://github.com/gildor2/UEViewer/blob/master/Exporters/ExportPsk.cpp
No exporter source or game geometry is embedded here.
"""
import argparse, collections, hashlib, json, math, pathlib, struct, subprocess
from generate_hand_mesh import chunk, pack
ROOT=pathlib.Path(__file__).resolve().parents[1]


def quaternion_product(a,b):
    x,y,z,w=a;X,Y,Z,W=b
    return (w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z)


def inverse(q):return (-q[0],-q[1],-q[2],q[3])


def rotate(q,v):return quaternion_product(quaternion_product(q,(*v,0)),inverse(q))[:3]
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))


def read_mesh(path):
    data=path.read_bytes();at=0;chunks={}
    while at<len(data):
        if at+32>len(data):raise ValueError('Truncated PSK header')
        tag,version,size,count=struct.unpack_from('<20siii',data,at);at+=32
        if size<0 or count<0 or at+size*count>len(data):raise ValueError('Invalid PSK chunk')
        chunks[tag.rstrip(b'\0').decode()]=[data[at+i*size:at+(i+1)*size] for i in range(count)];at+=size*count
    return chunks


def extract_hand(chunks,left):
    source_bones=[struct.unpack('<64sIii11f',r) for r in chunks['REFSKELT']]
    names=[r[0].split(b'\0')[0].decode() for r in source_bones]
    points=[struct.unpack('<3f',r) for r in chunks['PNTS0000']]
    wedges=[struct.unpack('<IffBBH',r) for r in chunks['VTXW0000']]
    faces=[struct.unpack('<HHHBBI',r) for r in chunks['FACE0000']]
    weights=collections.defaultdict(list)
    for r in chunks['RAWWEIGHTS']:
        w,p,b=struct.unpack('<fii',r)
        if not math.isfinite(w) or w<0 or not 0<=p<len(points) or not 0<=b<len(source_bones):raise ValueError('Invalid mesh weights')
        weights[p].append((w,b))
    side='L' if left else 'R';prefix='Hands_'+side+'Arm';palm=names.index(prefix+'Palm')
    selected=[palm]+[names.index(prefix+suffix) for suffix in ['Thumb1','Thumb2','Thumb3','Finger11','Finger12','Finger13','Finger21','Finger22','Finger23','Finger32','Finger33','Finger34','Finger42','Finger43','Finger44']]
    world=[]
    for i,b in enumerate(source_bones):
        q=tuple(b[4:8]);length=math.sqrt(sum(x*x for x in q))
        if not math.isfinite(length) or length<.001:raise ValueError('Invalid bind rotation')
        q=tuple(x/length for x in q)
        # ActorX stores conjugated child rotations. Root rotation is direct.
        if i:q=inverse(q)
        pos=tuple(b[8:11])
        if i:
            if not 0<=b[3]<i:raise ValueError('Non-topological skeleton')
            pq,pp=world[b[3]];pos=add(pp,rotate(pq,pos));q=quaternion_product(pq,q)
        world.append((q,pos))
    pq,pp=world[palm]
    new_names=['Palm']+[f+str(j) for f in ['Thumb','Index','Middle','Ring','Little'] for j in [1,2,3]]
    # Rebase the installed bind mesh without baking a second skin deformation.
    # Preserve native bind geometry and let the runtime drive its finger bones.
    root_turn=(0,0,0,1) if left else (1,0,0,0)
    world_turn=quaternion_product(root_turn,inverse(pq))
    targets=[(0,0,0)];rotations=[(0,0,0,1)];parents=[0];offsets=[(0,0,0)];local_rotations=[(0,0,0,1)]
    for i,old in enumerate(selected[1:],1):
        parent=0 if (i-1)%3==0 else i-1
        position=rotate(world_turn,sub(world[old][1],pp))
        orientation=quaternion_product(world_turn,world[old][0])
        offset=rotate(inverse(rotations[parent]),sub(position,targets[parent]))
        relative=quaternion_product(inverse(rotations[parent]),orientation)
        parents.append(parent);offsets.append(offset);targets.append(position);rotations.append(orientation);local_rotations.append(inverse(relative))
    selected_map={old:i for i,old in enumerate(selected)}
    lower_arm=names.index(prefix+'2');eligible={lower_arm}
    for i,b in enumerate(source_bones):
        if i!=lower_arm and b[3] in eligible:eligible.add(i)
    side_weight=lambda p:sum(w for w,b in weights[p] if b in eligible)
    new_points=[];new_weights=[];new_wedges=[];new_faces=[];rim={};point_map={}
    def vertex(j):
        p,u,v,_,_,_=wedges[j];influences=collections.defaultdict(float);total=sum(w for w,b in weights[p])
        if not .99<total<1.01:raise ValueError('Non-unit skin weights')
        for w,b in weights[p]:influences[selected_map.get(b,0)]+=w/total
        return (rotate(world_turn,sub(points[p],pp)),(u,v),dict(influences))
    def crossing(a,b):
        t=-a[0][0]/(b[0][0]-a[0][0]);pos=(0.,*(a[0][i]+t*(b[0][i]-a[0][i]) for i in [1,2]))
        uv=tuple(x+t*(y-x) for x,y in zip(a[1],b[1]))
        rim[tuple(round(x,5) for x in pos)]=pos
        return pos,uv,{0:1.}  # Keep the cut rim rigid with its palm-owned cap.
    def triangle(vertices,material,smoothing=1):
        first=len(new_wedges)
        for pos,uv,weights in vertices:
            # Keep shared skin points and descending influences, as ActorX does.
            influences=sorted(((bone,w) for bone,w in weights.items() if w>0),key=lambda item:(-item[1],item[0]))
            key=pack('3f',*pos)+b''.join(pack('if',bone,w) for bone,w in influences)
            index=point_map.get(key)
            if index is None:
                index=len(new_points);point_map[key]=index;new_points.append(pos)
                for bone,weight in influences:new_weights.append(pack('fii',weight,index,bone))
            new_wedges.append(pack('IffBBH',index,*uv,material,0,0))
        new_faces.append(pack('HHHBBI',first,first+1,first+2,material,0,smoothing))
    for face in faces:
        if face[3]!=1 or not all(side_weight(wedges[j][0])>=.5 for j in face[:3]):continue
        polygon=[vertex(j) for j in face[:3]];clipped=[]
        # A geometric wrist cut avoids the ragged palm-weight threshold edge.
        for a,b in zip(polygon,polygon[1:]+polygon[:1]):
            if a[0][0]>=0:clipped.append(a)
            if (a[0][0]>=0)!=(b[0][0]>=0):clipped.append(crossing(a,b))
        for i in range(1,len(clipped)-1):triangle([clipped[0],clipped[i],clipped[i+1]],0,face[5])
    if len(rim)<6:raise ValueError('No closed wrist intersection')
    centre=(0.,sum(p[1] for p in rim.values())/len(rim),sum(p[2] for p in rim.values())/len(rim))
    ring=sorted(rim.values(),key=lambda p:math.atan2(p[2]-centre[2],p[1]-centre[1]))
    # ActorX/UE winding is opposite the conventional right-handed cross product.
    for a,b in zip(ring,ring[1:]+ring[:1]):triangle([(centre,(.5,.5),{0:1.}),(a,(.5,.5),{0:1.}),(b,(.5,.5),{0:1.})],1)
    bones=[]
    for i,(name,parent,v) in enumerate(zip(new_names,parents,offsets)):
        children=sum(j!=i and p==i for j,p in enumerate(parents))
        bones.append(pack('64sIii11f',name.encode(),0,children,parent,*local_rotations[i],*v,0,0,0,0))
    mesh=chunk('ACTRHEAD')+chunk('PNTS0000',12,(pack('3f',*v) for v in new_points))
    mesh+=chunk('VTXW0000',16,new_wedges)+chunk('FACE0000',12,new_faces)
    mesh+=chunk('MATT0000',88,[pack('64siIiIii',name,i,0,0,0,0,0) for i,name in enumerate([b'InstalledKFHands',b'WristCap'])])
    mesh+=chunk('REFSKELT',120,bones)+chunk('RAWWEIGHTS',12,new_weights)
    anim=chunk('ANIMHEAD')+chunk('BONENAMES',120,bones)
    anim+=chunk('ANIMINFO',168,[pack('64s64s4i3f3i',b'Idle',b'None',16,1,0,16,0,1,30,0,0,1)])
    anim+=chunk('ANIMKEYS',32,(pack('8f',*v,*q,1) for v,q in zip(offsets,local_rotations)))
    if not 1000<len(new_faces)<4000 or len(new_wedges)>65535 or max(abs(x) for v in new_points for x in v)>20:raise ValueError('Unexpected isolated hand geometry')
    return mesh,anim,dict(vertices=len(new_points),triangles=len(new_faces),wrist_cap_triangles=len(ring),bones=16,bounds=[[min(v[a] for v in new_points),max(v[a] for v in new_points)] for a in range(3)],original_selected_bones=[names[b] for b in selected],units_per_metre=60,skin_source_slot=1)


def build(game,umodel):
    work=ROOT/'local/native-hands';source=work/'export';source.mkdir(parents=True,exist_ok=True)
    # Refresh this one generated export; never reuse stale geometry after a game update.
    psk=source/'KF_Weapons_Trip/SkeletalMesh/9mm_Trip.psk'
    psk.unlink(missing_ok=True)
    command=[str(umodel),'-export','-noanim','-notex','-path='+str(game),'-out='+str(source),'KF_Weapons_Trip','9mm_Trip']
    result=subprocess.run(command,capture_output=True,text=True,timeout=60,creationflags=subprocess.CREATE_NO_WINDOW)
    (work/'export.log').write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError('Installed hand export failed')
    chunks=read_mesh(psk)
    compiler=ROOT/'local/compiler';package=compiler/'KFVRLocalHands';models=package/'Models';classes=package/'Classes';models.mkdir(parents=True,exist_ok=True);classes.mkdir(exist_ok=True)
    lines=['class KFVRLocalHandAssets extends Object;'];stats={}
    for left in [False,True]:
        name='NativeHand'+('Left' if left else 'Right');mesh,anim,stats[name]=extract_hand(chunks,left)
        (models/(name+'.psk')).write_bytes(mesh);(models/(name+'.psa')).write_bytes(anim)
        lines.extend([f'#exec MESH MODELIMPORT MESH={name} MODELFILE=Models\\{name}.psk',f'#exec MESH ORIGIN MESH={name} X=0 Y=0 Z=0 YAW=0 PITCH=0 ROLL=0',f'#exec MESH SCALE MESH={name} X=1 Y=1 Z=1',f'#exec ANIM IMPORT ANIM={name}Anim ANIMFILE=Models\\{name}.psa IMPORTSEQS=1',f'#exec ANIM DIGEST ANIM={name}Anim VERBOSE USERAWINFO',f'#exec MESH DEFAULTANIM MESH={name} ANIM={name}Anim'])
    (classes/'KFVRLocalHandAssets.uc').write_text('\n'.join(lines)+'\n')
    system=compiler/'System';config=(system/'KFVRMake.ini').read_text();needle='EditPackages=KFVRGameplay';assert config.count(needle)==1
    (system/'KFVRHandsMake.ini').write_text(config.replace(needle,needle+'\nEditPackages=KFVRLocalHands'))
    output=system/'KFVRLocalHands.u';output.unlink(missing_ok=True)
    result=subprocess.run([str(system/'ucc.exe'),'make','ini=KFVRHandsMake.ini','-unattended'],cwd=system,capture_output=True,timeout=90)
    (work/'compile.log').write_bytes(result.stdout+result.stderr)
    if result.returncode or not output.exists():raise RuntimeError('Local hand package build failed; inspect local/native-hands/compile.log')
    data=output.read_bytes();(work/'KFVRLocalHands.u').write_bytes(data)
    manifest=dict(source_package=str(game/'Animations/KF_Weapons_Trip.ukx'),source_sha256=hashlib.sha256((game/'Animations/KF_Weapons_Trip.ukx').read_bytes()).hexdigest(),exporter_sha256=hashlib.sha256(umodel.read_bytes()).hexdigest(),source_psk_sha256=hashlib.sha256(psk.read_bytes()).hexdigest(),package_sha256=hashlib.sha256(data).hexdigest(),hands=stats,redistributable=False,headset_accepted=False)
    (work/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--umodel',type=pathlib.Path,required=True);a=p.parse_args();build(a.game.resolve(),a.umodel.resolve())
