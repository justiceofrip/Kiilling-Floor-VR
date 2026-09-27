"""Read-only snapshots of our reflected script rig; never an input/write path."""
import argparse,ctypes,json,pathlib,struct,time
from observe_kf import Observer,unpack
OWNERS={'KFVRWeaponRig','KFVRHandVisual','KFVRWeaponVisual','KFVRTrackedSingle','KFVRTrackedDeagle','KFVRTrackedBullpup','KFVRManualReload','KFVRTrackedKnife','KFVRTrackedDualies','KFVRDualBeltReload','KFVRMeleeTarget','KFWeapon'}
def layout(o):
 fields=[]
 for row in o.rows:
  if row['class'].endswith('Property') and (row['outer'] in OWNERS or (row['outer']=='Actor' and row['name'] in ['Location','Rotation','bHidden'])):
   d=o.read(int(row['address'],16),116);off=unpack(d,76);size=unpack(d,60)
   if unpack(d,56)!=1 or size not in [4,12]:continue
   fields.append((row['outer'],row['name'],row['class'],off,size,unpack(d,112)))
 schema=[]
 for row in o.rows:
  if row['class'] not in ['KFVRWeaponRig','KFVRWeaponVisual','KFVRHandVisual','KFVRTrackedSingle','KFVRTrackedDeagle','KFVRTrackedBullpup','KFVRManualReload','KFVRTrackedKnife','KFVRTrackedDualies','KFVRDualBeltReload','KFVRMeleeTarget']:continue
  address=int(row['address'],16);chain=[];cls=unpack(o.read(address+40,4))
  while cls:
   chain.append(o.object_name(cls));cls=unpack(o.read(cls+44,4))
  selected=[f for f in fields if f[0] in chain]
  schema.append((row,selected,max((f[3]+f[4] for f in selected),default=0)))
 return schema

def sample(o):
 if not hasattr(o,'vr_schema'):o.vr_schema=layout(o)
 out=[]
 for row,fields,size_needed in o.vr_schema:
  address=int(row['address'],16);buffer=o.read(address,size_needed);values={}
  for owner,name,typ,off,size,mask in fields:
   data=buffer[off:off+size]
   if typ=='IntProperty':value=struct.unpack('<i',data)[0]
   elif typ=='FloatProperty':value=struct.unpack('<f',data)[0]
   elif typ=='BoolProperty':value=bool(unpack(data)&mask)
   elif typ=='ObjectProperty':value=hex(unpack(data))
   elif typ=='StructProperty' and size==12:value=struct.unpack('<iii' if name.endswith('Rotation') else '<fff',data)
   else:continue
   values[name]=value
  out.append(dict(address=row['address'],type=row['class'],values=values))
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--seconds',type=float,default=20);a=p.parse_args()
 tick=ctypes.WinDLL('kernel32').GetTickCount64;tick.restype=ctypes.c_ulonglong
 with a.output.open('w') as f:
  end=time.monotonic()+a.seconds
  stop=a.output.with_suffix('.stop')
  consecutive_errors=0
  while time.monotonic()<end and not stop.exists():
   o=None
   try:
    o=Observer(a.pid,a.game)
    o.snapshot()
    # Refresh object discovery once/second: visuals are created on first tracking.
    for i in range(10):
     if stop.exists():break
     f.write(json.dumps(dict(uptime_ms=tick(),objects=sample(o)))+'\n');f.flush();time.sleep(.1)
    consecutive_errors=0
   except (OSError,ValueError) as error:
    # Object/name arrays can grow while our presentation actors are first created.
    # Retry only transient read snapshots; never weaken the installed ABI checks.
    transient=isinstance(error,OSError) or 'Array changed during snapshot' in str(error) or 'FName index mismatch' in str(error)
    if not transient:raise
    consecutive_errors+=1
    print('Observer snapshot retry: '+str(error),flush=True)
    if consecutive_errors>=5:raise
    time.sleep(.05)
   finally:
    if o is not None:o.close()

  if stop.exists():
   # Native publishing has stopped; let the rig's existing 100 ms heartbeat
   # release its presentation, then independently record the final state.
   time.sleep(.15)
   o=Observer(a.pid,a.game)
   try:
    o.snapshot();f.write(json.dumps(dict(uptime_ms=tick(),objects=sample(o)))+'\n');f.flush()
   finally:o.close()
