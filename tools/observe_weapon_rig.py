"""Read-only snapshots of our reflected script rig; never an input/write path."""
import argparse,ctypes,json,pathlib,struct,time
from observe_kf import Observer,unpack
OWNERS={'KFVRMagazineGrip','KFVRWeaponRig','KFVRHandVisual','KFVRWeaponVisual','KFVRTrackedSingle','KFVRTrackedSyringe','KFVRTrackedWelder','KFVRTraderPanel','KFVRGrenadeControl','KFVRTraderTest','KFVRDoorTest','KFVRTrackedDeagle','KFVRTrackedMK23','KFVRTrackedBullpup','KFVRManualReload','KFVRTrackedKnife','KFVRTrackedDualies','KFVRDualBeltReload','KFVRStatusDisplay','KFVRMeleeTarget','Projectile','Grenade','Nade','Ammunition','KFWeapon'}
def layout(o):
 fields=[]
 for row in o.rows:
  if row['class'].endswith('Property') and (row['outer'] in OWNERS or (row['outer']=='KFPawn' and row['name']=='healthToGive') or (row['outer']=='Pawn' and row['name'] in ['Health','HealthMax','ShieldStrength','Weapon','EyeHeight','BaseEyeHeight','WalkBob']) or (row['outer']=='Actor' and row['name'] in ['Location','Rotation','Velocity','DrawScale','bHidden','bDeleteMe','LifeSpan'])):
   d=o.read(int(row['address'],16),116);off=unpack(d,76);size=unpack(d,60)
   if unpack(d,56)!=1 or size not in [4,12]:continue
   fields.append((row['outer'],row['name'],row['class'],off,size,unpack(d,112)))
 # Expand the reflected TrackingPacket members instead of trusting a copied
 # byte layout. This lets independent checks compare actual bones to both
 # the delivered head pose and body heading in the same object snapshot.
 tracking=[r for r in o.rows if r['outer']=='KFVRWeaponRig' and r['name']=='Tracking' and r['class']=='StructProperty']
 if len(tracking)==1:
  d=o.read(int(tracking[0]['address'],16),116);base=unpack(d,76);packet_size=unpack(d,60)
  for row in o.rows:
   if row['outer']!='TrackingPacket' or not row['class'].endswith('Property'):continue
   d=o.read(int(row['address'],16),116);off=unpack(d,76);size=unpack(d,60)
   if unpack(d,56)==1 and size in [4,12] and 0<=off and off+size<=packet_size:
    fields.append(('KFVRWeaponRig','Tracking.'+row['name'],row['class'],base+off,size,unpack(d,112)))
 # Expand measured native grip/finger frames through the reflected Coords
 # members. These are read-only snapshots, independent of script error fields.
 coords=[]
 for row in o.rows:
  if row['outer'].lower()=='coords' and row['class']=='StructProperty':
   d=o.read(int(row['address'],16),116)
   if unpack(d,56)==1 and unpack(d,60)==12:coords.append((row['name'],unpack(d,76)))
 for row in o.rows:
  if (row['outer'],row['name']) not in [('KFVRWeaponVisual','GripPose'),('KFVRHandVisual','FingerWorld'),('KFVRHandVisual','NativeBind')]:continue
  d=o.read(int(row['address'],16),116);base=unpack(d,76);count=unpack(d,56);size=unpack(d,60)
  if row['class']!='StructProperty' or size!=48 or count not in [15,32] or sorted(o for _,o in coords)!=[0,12,24,36]:raise ValueError('Unexpected measured grip frame layout')
  for i in range(count):
   for name,offset in coords:fields.append((row['outer'],f"{row['name']}.{i}.{name}",'StructProperty',base+i*size+offset,12,0))
 o.vr_fields=fields
 schema=[]
 for row in o.rows:
  if row['class'] not in ['KFHumanPawn','KFVRMagazineGrip','KFVRWeaponRig','KFVRWeaponVisual','KFVRHandVisual','KFVRTrackedSingle','KFVRTrackedSyringe','KFVRTrackedWelder','KFVRTraderPanel','KFVRGrenadeControl','KFVRTraderTest','KFVRDoorTest','KFVRTrackedDeagle','KFVRTrackedMK23','KFVRTrackedBullpup','KFVRManualReload','KFVRTrackedKnife','KFVRTrackedDualies','KFVRDualBeltReload','KFVRStatusDisplay','KFVRMeleeTarget','Nade','MedicNade','FlameNade','FragAmmo']:continue
  schema.append(object_layout(o,row))
 return schema

def object_layout(o,row):
 cls=unpack(o.read(int(row['address'],16)+40,4));chain=[]
 for _ in range(32):
  if not cls:break
  chain.append(o.object_name(cls));cls=unpack(o.read(cls+44,4))
 selected=[f for f in o.vr_fields if f[0] in chain]
 return (row,selected,max((f[3]+f[4] for f in selected),default=0))

def sample(o):
 if not hasattr(o,'vr_schema'):o.vr_schema=layout(o)
 # Bracket multi-object reads with the rig's packet and completion counters.
 # A walking pawn may advance several UU between reads; crossed-Tick samples
 # are useful logs but cannot establish relative hand/weapon attachment.
 guards=[];stamps={}
 for row,fields,size_needed in o.vr_schema:
  if row['class']=='KFVRWeaponRig':
   for owner,name,typ,off,size,mask in fields:
    if name in ['Tracking.Serial','LastSerial','AppliedFrames','PresentationSerial']:
     at=int(row['address'],16)+off;before=o.read(at,size);guards.append((at,before));stamps[name]=unpack(before)
 out=[]
 for row,fields,size_needed in o.vr_schema:
  address=int(row['address'],16);buffer=o.read(address,size_needed);values={}
  for owner,name,typ,off,size,mask in fields:
   data=buffer[off:off+size]
   if typ=='StrProperty' and owner in ['KFVRStatusDisplay','KFVRTraderPanel']:
    pointer,count,capacity=struct.unpack('<III',data)
    if count==0:value=''
    elif 0<count<=capacity<=1024:value=o.read(pointer,count*2).decode('utf-16le').rstrip('\x00')
    else:continue
   elif typ=='IntProperty':value=struct.unpack('<i',data)[0]
   elif typ=='FloatProperty':value=struct.unpack('<f',data)[0]
   elif typ=='BoolProperty':value=bool(unpack(data)&mask)
   elif typ=='ObjectProperty':value=hex(unpack(data))
   elif typ=='StructProperty' and size==12:value=struct.unpack('<iii' if name.endswith('Rotation') else '<fff',data)
   else:continue
   values[name]=value
  out.append(dict(address=row['address'],type=row['class'],values=values))
  if row['class']=='KFVRGrenadeControl':
   # Follow the reflected live projectile pointer immediately: a full object
   # discovery can take longer than the flight of a low hand throw. Validate
   # its index against the current installed UObject array before reading.
   ptr=int(values.get('LastProjectile','0x0'),16)
   if ptr and not any(int(item[0]['address'],16)==ptr for item in o.vr_schema):
    h=o.read(ptr,44);index=unpack(h,4)
    array=o.read(o.base+o.exports['?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A'],12)
    data,count,capacity=struct.unpack('<III',array)
    if not 0<=index<count<=capacity<=500000 or unpack(o.read(data+index*4,4))!=ptr:continue
    cls=o.object_name(unpack(h,40))
    if cls in ['Nade','MedicNade','FlameNade']:
     o.vr_schema.append(object_layout(o,dict(address=hex(ptr),index=index,**{'class':cls})))
 o.rig_sample_consistent=bool(guards) and ('PresentationSerial' not in stamps or stamps['PresentationSerial']==stamps.get('Tracking.Serial')) and all(o.read(at,len(before))==before for at,before in guards)
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--seconds',type=float,default=20);a=p.parse_args()
 tick=ctypes.WinDLL('kernel32').GetTickCount64;tick.restype=ctypes.c_ulonglong
 # Build full object inventories on a separate reader so discovery cannot
 # leave one-second holes in the live pose/contact samples. Each reader owns
 # its own process handle and caches; only completed snapshots are transferred.
 from concurrent.futures import ThreadPoolExecutor
 def discover():
  fresh=Observer(a.pid,a.game)
  try:
   fresh.snapshot();fresh.vr_schema=layout(fresh);return fresh
  except BaseException:
   fresh.close();raise
 def transient(error):
  return isinstance(error,OSError) or 'Array changed during snapshot' in str(error) or 'FName index mismatch' in str(error)
 with a.output.open('w') as f, ThreadPoolExecutor(max_workers=1) as discovery:
  end=time.monotonic()+a.seconds;stop=a.output.with_suffix('.stop')
  o=None;pending=discovery.submit(discover);next_discovery=0;consecutive_errors=0
  try:
   while time.monotonic()<end and not stop.exists():
    try:
     if pending is not None and pending.done():
      completed=pending;pending=None;fresh=completed.result()
      if o is not None:o.close()
      o=fresh;next_discovery=time.monotonic()+1;consecutive_errors=0
     if pending is None and time.monotonic()>=next_discovery:pending=discovery.submit(discover)
     if o is not None:
      f.write(json.dumps(dict(uptime_ms=tick(),objects=sample(o),rig_sample_consistent=o.rig_sample_consistent))+'\n');f.flush()
    except (OSError,ValueError) as error:
     if not transient(error):raise
     consecutive_errors+=1
     print('Observer snapshot retry: '+str(error),flush=True)
     if consecutive_errors>=5:raise
     if o is not None:o.close();o=None
     next_discovery=0
    time.sleep(.1)
  finally:
   if o is not None:o.close()
   if pending is not None:
    try:pending.result().close()
    except (OSError,ValueError):pass

  if stop.exists():
   # Native publishing has stopped; let the rig's existing 100 ms heartbeat
   # release its presentation, then independently record the final state.
   time.sleep(.15)
   final=discover()
   try:
    f.write(json.dumps(dict(uptime_ms=tick(),objects=sample(final)))+'\n');f.flush()
   finally:final.close()
