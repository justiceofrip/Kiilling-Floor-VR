"""Bounded, READ-ONLY KF process/object inspector. No injection or native calls.
Offsets are admitted by installed exported accessor instructions, not guesses.
This is an unsynchronised external snapshot, never authority for memory writes.
"""
import argparse,ctypes as c,hashlib,json,pathlib,struct,sys
from ctypes import wintypes as w
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'local/pydeps'))
import pefile
K=c.WinDLL('kernel32',use_last_error=True);P=c.WinDLL('psapi',use_last_error=True)
K.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];K.OpenProcess.restype=w.HANDLE
K.CloseHandle.argtypes=[w.HANDLE];K.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)];K.ReadProcessMemory.restype=w.BOOL
P.EnumProcessModulesEx.argtypes=[w.HANDLE,c.POINTER(w.HMODULE),w.DWORD,c.POINTER(w.DWORD),w.DWORD];P.EnumProcessModulesEx.restype=w.BOOL
P.GetModuleFileNameExW.argtypes=[w.HANDLE,w.HMODULE,w.LPWSTR,w.DWORD]
K.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)]

def unpack(data,at=0):return struct.unpack_from('<I',data,at)[0]
class Observer:
 def __init__(self,pid,game):
  self.pid=pid;self.handle=K.OpenProcess(0x410,False,pid)
  if not self.handle:raise OSError(c.get_last_error(),'Read-only OpenProcess failed')
  buf=c.create_unicode_buffer(32768);length=w.DWORD(len(buf))
  if not K.QueryFullProcessImageNameW(self.handle,0,buf,c.byref(length)) or pathlib.Path(buf.value).resolve()!= (game/'System/KillingFloor.exe').resolve():raise ValueError('Process does not match selected installation')
  mods=(w.HMODULE*1024)();needed=w.DWORD()
  if not P.EnumProcessModulesEx(self.handle,mods,c.sizeof(mods),c.byref(needed),3) or needed.value>c.sizeof(mods):raise ValueError('Invalid module snapshot')
  self.modules={}
  for m in mods[:needed.value//c.sizeof(w.HMODULE)]:
   name=c.create_unicode_buffer(32768);P.GetModuleFileNameExW(self.handle,m,name,len(name));self.modules[pathlib.Path(name.value).name.lower()]=(int(m),name.value)
  self.base,self.core_path=self.modules['core.dll'];self.pe=pefile.PE(self.core_path)
  self.exports={s.name.decode():s.address for s in self.pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name}
  # Direct accessor ABI/signature checks on both disk and loaded code.
  for name,signature in [('?GetIndex@UObject@@QBEKXZ','8b4104c3'),('?GetClass@UObject@@QBEPAVUClass@@XZ','8b4128c3'),('?GetOuter@UObject@@QBEPAV1@XZ','8b411cc3'),('?GetSuperClass@UClass@@QBEPAV1@XZ','8b412cc3'),('?GetPropertiesSize@UStruct@@UBEHXZ','8b4144c3')]:
   rva=self.exports[name];sig=bytes.fromhex(signature)
   if self.pe.get_data(rva,len(sig))!=sig or self.read(self.base+rva,len(sig))!=sig:raise ValueError('Unverified accessor '+name)
  rva=self.exports['?GetName@UObject@@QBEPBGXZ'];body=self.read(self.base+rva,16)
  if body[:5]!=bytes.fromhex('8b41248b0d') or body[9:]!=bytes.fromhex('8b048183c00cc3'):raise ValueError('Unverified FName accessor')
  names_rva=self.exports['?Names@FName@@0V?$TArray@PAUFNameEntry@@@@A']
  if unpack(body,5)!=self.base+names_rva:raise ValueError('FName global mismatch')
  self.names=self.array(self.base+names_rva,200000)
  self.objects=self.array(self.base+self.exports['?GObjObjects@UObject@@0V?$TArray@PAVUObject@@@@A'],500000)
  self.name_cache={};self.headers={};self.by_pointer={};self.rows=[]
 def close(self):
  if self.handle:K.CloseHandle(self.handle);self.handle=None
 def read(self,addr,size):
  if not 0x10000<=addr<0x100000000 or not 0<=size<=64000000:raise ValueError('Invalid read range')
  buf=c.create_string_buffer(size);got=c.c_size_t()
  if not K.ReadProcessMemory(self.handle,addr,buf,size,c.byref(got)) or got.value!=size:raise OSError(c.get_last_error(),'Memory no longer readable')
  return buf.raw
 def array(self,addr,maximum):
  before=self.read(addr,12);ptr,count,cap=struct.unpack('<III',before)
  if not 0<count<=cap<=maximum:raise ValueError('Invalid array bounds')
  data=self.read(ptr,count*4)
  if self.read(addr,12)!=before:raise ValueError('Array changed during snapshot; retry')
  return list(struct.unpack('<'+'I'*count,data))
 def name(self,i):
  if i in self.name_cache:return self.name_cache[i]
  if not 0<=i<len(self.names) or not self.names[i]:return '<invalid>'
  data=self.read(self.names[i],524)
  if unpack(data)!=i:raise ValueError('FName index mismatch')
  end=next((i for i in range(12,len(data),2) if data[i:i+2]==b'\x00\x00'),len(data));name=data[12:end].decode('utf-16le',errors='strict');self.name_cache[i]=name;return name
 def header(self,ptr):
  if ptr not in self.headers:self.headers[ptr]=self.read(ptr,44)
  return self.headers[ptr]
 def object_name(self,ptr):return self.name(unpack(self.header(ptr),36)) if ptr else 'None'
 def path(self,ptr):
  parts=[];seen=set()
  while ptr and ptr not in seen and len(parts)<32:
   seen.add(ptr);h=self.header(ptr);parts.append(self.name(unpack(h,36)));ptr=unpack(h,28)
  return '.'.join(reversed(parts))
 def snapshot(self):
  skipped=0
  for index,ptr in enumerate(self.objects):
   if not ptr:continue
   try:
    h=self.header(ptr)
    if unpack(h,4)!=index:skipped+=1;continue
    row={'address':hex(ptr),'index':index,'name':self.name(unpack(h,36)),'class':self.object_name(unpack(h,40)),'outer':self.object_name(unpack(h,28))}
    self.rows.append(row);self.by_pointer[ptr]=row
   except (ValueError,OSError,UnicodeError):skipped+=1
  classes={row['name']:int(row['address'],16) for row in self.rows if row['class']=='Class'}
  wanted={'LevelInfo':{'NetMode','TimeSeconds','Pauser','Game'},'Actor':{'Location','Rotation','Level','Owner'},'Controller':{'Pawn'},'PlayerController':{'Player','ViewTarget','FOVAngle','bBehindView'},'Pawn':{'Health','Weapon','PlayerReplicationInfo','CollisionRadius','CollisionHeight'},'Weapon':{'FireMode','Ammo'},'KFWeapon':{'MagAmmoRemaining','MagCapacity','ReloadRate'},'Ammunition':{'AmmoAmount','MaxAmmo'}}
  properties={}
  for row in self.rows:
   if row['outer'] in wanted and row['name'] in wanted[row['outer']] and row['class'].endswith('Property'):
    ptr=int(row['address'],16);data=self.read(ptr,116);off=unpack(data,76);dim=unpack(data,56);size=unpack(data,60)
    owner=unpack(data,28);owner_size=unpack(self.read(owner+68,4))
    if not 0<size<=64 or dim!=1 or not off+size<=owner_size:continue
    properties[row['outer']+'.'+row['name']]={'offset':off,'size':size,'type':row['class'],'mask':unpack(data,112) if row['class']=='BoolProperty' else None,'owner_size':owner_size}
  wanted_classes=['GameEngine','WindowsClient','WindowsViewport','D3D9RenderDevice','Level','LevelInfo','KFPlayerController','KFHumanPawn','KFGameType','KFGameReplicationInfo']
  observations=[]
  for row in self.rows:
   if row['class'] not in wanted_classes:continue
   ptr=int(row['address'],16);h=self.header(ptr);class_ptr=unpack(h,40);chain=[]
   for _ in range(32):
    if not class_ptr:break
    chain.append(self.object_name(class_ptr));class_ptr=unpack(self.read(class_ptr+44,4))
   values={}
   for key,prop in properties.items():
    if key.split('.')[0] not in chain:continue
    data=self.read(ptr+prop['offset'],prop['size']);typ=prop['type']
    if typ=='ByteProperty':value=data[0]
    elif typ=='IntProperty':value=struct.unpack('<i',data)[0]
    elif typ=='FloatProperty':value=struct.unpack('<f',data)[0]
    elif typ=='ObjectProperty':
     target=unpack(data);value={'address':hex(target),'name':self.object_name(target)} if target else None
    elif typ=='BoolProperty':value=bool(unpack(data)&prop['mask'])
    elif typ=='StructProperty' and len(data)==12:value=struct.unpack('<iii' if key.endswith('Rotation') else '<fff',data)
    else:continue
    values[key]=value
   observations.append({**row,'path':self.path(ptr),'values':values})
  return {'pid':self.pid,'read_only':True,'synchronized':False,'core_sha256':hashlib.sha256(pathlib.Path(self.core_path).read_bytes()).hexdigest(),'modules':{name:{'base':hex(v[0]),'path':v[1]} for name,v in self.modules.items() if name in ['engine.dll','core.dll','d3d9drv.dll','d3d9.dll','d3d8.dll','roengine.dll']},'object_count':len(self.objects),'valid_objects':len(self.rows),'skipped':skipped,'properties':properties,'observations':observations}
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--pid',type=int,required=True);parser.add_argument('--game',type=pathlib.Path,required=True);parser.add_argument('--output',type=pathlib.Path,required=True);args=parser.parse_args();o=None
 try:
  o=Observer(args.pid,args.game);report=o.snapshot();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k not in ['properties','modules']},indent=2))
 finally:
  if o:o.close()

