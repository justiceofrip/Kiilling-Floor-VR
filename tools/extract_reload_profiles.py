"""Read-only extraction of asset references for authoring physical reload profiles.
No actor calls, animation sampling, installed-file writes or profile activation.
"""
import argparse,hashlib,json,math,pathlib,struct
from observe_kf import Observer,unpack
from native_profile import AUDITED_SHA256

CLASSES=['Single','Dualies','Deagle','DualDeagle','MK23Pistol','DualMK23Pistol']
FIELDS={'Mesh','MeshRef','DrawScale','DrawScale3D','PlayerViewOffset','PlayerViewPivot','DisplayFOV',
 'MagCapacity','ReloadRate','ReloadAnimRate','ReloadAnim','ShortReloadAnim','SelectAnim',
 'PutDownAnim','IdleAnim','FireModeClass','FlashBoneName','AttachmentClass'}

def extract(pid,game):
 for module in ['Core.dll','Engine.dll']:
  if hashlib.sha256((game/'System'/module).read_bytes()).hexdigest()!=AUDITED_SHA256[module]:
   raise ValueError('Unaudited module: '+module)
 o=Observer(pid,game)
 try:
  snapshot=o.snapshot()
  local=[r for r in snapshot['observations'] if r['class']=='KFPlayerController'
         and r['values'].get('PlayerController.Player')]
  if len(local)!=1:raise ValueError('One local viewport is required')
  level=next(r for r in snapshot['observations'] if r['address']==local[0]['values']['Actor.Level']['address'])
  if level['values']['LevelInfo.NetMode']!=0:raise ValueError('Profile audit requires the local standalone world')
  rva=o.exports['?GetDefaultObject@UClass@@QBEPBVUObject@@XZ'];o.pe.relocate_image(o.base)
  accessor=o.pe.get_data(rva,64)
  if o.read(o.base+rva,64)!=accessor or bytes.fromhex('8bbef0040000') not in accessor or bytes.fromhex('8b86ec040000') not in accessor:
   raise ValueError('Default-object accessor changed')
  classes={o.path(int(r['address'],16)).casefold():int(r['address'],16) for r in o.rows if r['class']=='Class'}
  descriptors=[]
  for row in o.rows:
   if row['name'] not in FIELDS or not row['class'].endswith('Property'):continue
   data=o.read(int(row['address'],16),116)
   descriptors.append(dict(name=row['name'],owner=unpack(data,28),type=row['class'],
       dimension=unpack(data,56),size=unpack(data,60),offset=unpack(data,76)))
  profiles=[]
  for name in CLASSES:
   cls=classes.get(('KFMod.'+name).casefold())
   if not cls:profiles.append(dict(weapon='KFMod.'+name,loaded=False));continue
   ancestry=[];parent=cls
   for _ in range(32):
    if not parent:break
    ancestry.append(parent);parent=unpack(o.read(parent+44,4))
   address,count,capacity=struct.unpack('<III',o.read(cls+0x4ec,12));size=unpack(o.read(cls+68,4))
   if not 0<size==count<=capacity<=65536:raise ValueError('Invalid class default buffer')
   blob=o.read(address,size);values={};metadata={}
   for owner in reversed(ancestry):
    for field in descriptors:
     if field['owner']!=owner:continue
     offset=field['offset'];stride=field['size'];dimension=field['dimension'];typ=field['type']
     if not 0<stride<=16 or not 0<dimension<=2 or offset+stride*dimension>size:raise ValueError('Invalid reflected field '+field['name'])
     result=[]
     for i in range(dimension):
      raw=blob[offset+i*stride:offset+(i+1)*stride]
      if typ=='IntProperty' and stride==4:value=struct.unpack('<i',raw)[0]
      elif typ=='FloatProperty' and stride==4:
       value=struct.unpack('<f',raw)[0]
       if not math.isfinite(value):raise ValueError('Nonfinite default')
      elif typ=='NameProperty' and stride==4:value=o.name(unpack(raw))
      elif typ in ['ObjectProperty','ClassProperty'] and stride==4:value=o.path(unpack(raw)) if unpack(raw) else None
      elif typ=='StrProperty' and stride==12:
       pointer,length,limit=struct.unpack('<III',raw)
       if not 0<=length<=limit<=4096:raise ValueError('Invalid string bounds')
       data=o.read(pointer,length*2) if length else b''
       if length and data[-2:]!=b'\x00\x00':raise ValueError('Unterminated string')
       value=data[:-2].decode('utf-16le') if length else ''
      elif typ=='StructProperty' and stride==12:value=struct.unpack('<iii' if field['name']=='PlayerViewPivot' else '<fff',raw)
      else:continue
      result.append(value)
     if result:values[field['name']]=result if dimension>1 else result[0];metadata[field['name']]={k:v for k,v in field.items() if k not in ['owner','name']}
   profiles.append(dict(weapon='KFMod.'+name,loaded=True,defaults=values,reflection=metadata,
       enabled=False,calibration_required=['grip_to_model','muzzle','magwell','magazine_tip',
       'action_grip_and_travel','four_animation_ranges']))
  return dict(pid=pid,read_only=True,synchronized=False,source='verified loaded class defaults; not perk-modified instances',
      game_files_written=False,profiles_enabled=False,profiles=profiles)
 finally:o.close()

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--pid',type=int,required=True);parser.add_argument('--game',type=pathlib.Path,required=True);parser.add_argument('--output',type=pathlib.Path,required=True);args=parser.parse_args()
 result=extract(args.pid,args.game);args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2))
 for profile in result['profiles']:
  d=profile.get('defaults',{});print(profile['weapon'],json.dumps({k:d.get(k) for k in ['Mesh','MeshRef','ReloadAnim','MagCapacity']}))
 print('Read-only profile inputs saved to '+str(args.output))
