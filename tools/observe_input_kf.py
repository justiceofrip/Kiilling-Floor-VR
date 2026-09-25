"""Read-only input object/property evidence. Never authorizes unsynchronized writes."""
import argparse,json,pathlib,struct
from observe_kf import Observer,unpack
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
o=Observer(a.pid,a.game)
try:
 names={'PlayerInput','aUp','aBaseX','aBaseY','aBaseZ','aMouseX','aMouseY','aForward','aStrafe','aTurn','aLookUp','bFire','bAltFire'}
 snapshot=o.snapshot({'PlayerController':names-{'bFire','bAltFire'},'Controller':{'bFire','bAltFire'}})
 candidates=[v for v in snapshot['observations'] if v['class']=='KFPlayerController' and v['values'].get('PlayerController.Player')]
 if len(candidates)!=1:raise ValueError('Need one local player controller')
 pc=candidates[0];address=int(pc['address'],16);viewport=int(pc['values']['PlayerController.Player']['address'],16)
 level=next(v for v in snapshot['observations'] if v['address']==pc['values']['Actor.Level']['address'])
 if level['values']['LevelInfo.NetMode']!=0:raise ValueError('Input audit requires standalone world')
 props={k:v for k,v in snapshot['properties'].items() if k.split('.')[0] in ['PlayerController','Controller'] and k.split('.')[1] in names}
 if len(props)!=len(names):raise ValueError('Missing input properties: '+str(names-{k.split('.')[1] for k in props}))
 for key,v in props.items():
  if key.split('.')[1].startswith('a') and (v['type']!='FloatProperty' or v['size']!=4):raise ValueError('Unexpected axis property layout')
 native_input=unpack(o.read(viewport+0x78,4));back=unpack(o.read(native_input+0xea4,4));script_input=int(pc['values']['PlayerController.PlayerInput']['address'],16)
 if back!=viewport or unpack(o.read(script_input+0x1c,4))!=address or unpack(o.read(viewport+0x34,4))!=address:raise ValueError('Input ownership chain mismatch')
 functions=[dict(address=row['address'],index=row['index'],owner=row['outer'],name=row['name'],path=o.path(int(row['address'],16))) for row in o.rows if row['class']=='Function' and row['name'] in ['PlayerInput','PlayerTick','PlayerMove','UpdateRotation','StartFire','StopFire'] and row['outer'] in ['KFPlayerInput','PlayerInput','KFPlayerController','PlayerController','PlayerWalking']]
 report=dict(read_only=True,synchronized=False,pid=a.pid,controller=pc,properties=props,native_input=hex(native_input),script_input=hex(script_input),viewport=hex(viewport),ownership_verified=True,functions=functions,axis_scale_evidence='UInput.ReadInput multiplies float input properties by 20/DeltaTime after evaluating key holds; default forward/strafe aliases use Speed 300. Native input timing must still be measured before any writes.')
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2));print(json.dumps(dict(pid=a.pid,ownership_verified=True,properties=props,functions=functions),indent=2))
finally:o.close()
