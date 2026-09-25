"""Generate a weapon coverage ledger from installed package inheritance and live defaults."""
import argparse,json,pathlib,re,struct
from observe_kf import Observer,unpack

def main():
 p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[1]
 packages=json.loads((root/'reports/packages.json').read_text());classes={p['file'][:-2]+'.'+c['name']:c for p in packages for c in p['classes']}
 def chain(key):
  result=[]
  for _ in range(64):
   c=classes.get(key)
   if not c:return result
   result.append(key);parent=c['parent']
   if not parent:return result
   key=parent if '.' in parent else key.split('.')[0]+'.'+parent
  raise ValueError('Cyclic class inheritance')
 o=Observer(a.pid,a.game)
 try:
  snapshot=o.snapshot();live_classes={row['name']:int(row['address'],16) for row in o.rows if row['class']=='Class'}
  accessor=o.pe.get_data(o.exports['?GetDefaultObject@UClass@@QBEPBVUObject@@XZ'],64)
  if bytes.fromhex('8bbe f0040000'.replace(' ','')) not in accessor or bytes.fromhex('8b86ec040000') not in accessor:raise ValueError('Default object accessor changed')
  properties=[]
  wanted={'MagCapacity','ReloadRate','ReloadAnimRate','Weight','bHoldToReload','bMeleeWeapon','bConsumesPhysicalAmmo','FireModeClass','InventoryGroup','PickupClass','FireRate','AmmoPerFire','DamageMin','DamageMax','MeleeDamage','weaponRange','AmmoClass'}
  for row in o.rows:
   if row['name'] not in wanted or not row['class'].endswith('Property'):continue
   at=int(row['address'],16);data=o.read(at,116);properties.append(dict(name=row['name'],owner=row['outer'],type=row['class'],dim=unpack(data,56),size=unpack(data,60),offset=unpack(data,76),mask=unpack(data,112)))
  def defaults(class_name,ancestors):
   cls=live_classes.get(class_name)
   if not cls:return None
   address,count,cap=struct.unpack('<III',o.read(cls+0x4ec,12));size=unpack(o.read(cls+0x44,4))
   if not 0<size==count<=cap<=65536:return None
   data=o.read(address,size);result={}
   for ancestor in reversed(ancestors):
    for field in properties:
     if field['owner']!=ancestor:continue
     offset=field['offset'];stride=field['size'];dim=field['dim'];typ=field['type']
     if not 0<dim<=2 or not 0<stride<=16 or offset+dim*stride>len(data):continue
     values=[]
     for i in range(dim):
      raw=data[offset+i*stride:offset+(i+1)*stride]
      if typ=='IntProperty' and stride==4:value=struct.unpack('<i',raw)[0]
      elif typ=='FloatProperty' and stride==4:value=struct.unpack('<f',raw)[0]
      elif typ=='BoolProperty' and stride==4:value=bool(unpack(raw)&field['mask'])
      elif typ=='ByteProperty' and stride==1:value=raw[0]
      elif typ in ['ClassProperty','ObjectProperty'] and stride==4:value=o.object_name(unpack(raw))
      else:value=None
      values.append(value)
     result[field['name']]=values if dim>1 else values[0]
   return result
  families={
  'Syringe':'Tool','Welder':'Tool','Frag':'Throwable','StunNade':'Throwable','PipeBombExplosive':'Deployable',
  'Chainsaw':'PoweredMelee','KFWeapon':'Unclassified','KFMeleeGun':'Melee','KFMedicGun':'MagazineFed',
  'KFWeaponShotgun':'Unclassified','BoomStick':'BreakAction','M79GrenadeLauncher':'BreakActionLauncher',
  'Magnum44Pistol':'Revolver','FlareRevolver':'Revolver','Dual44Magnum':'DualRevolver','DualFlareRevolver':'DualRevolver',
  'Dualies':'DualMagazineFed','Shotgun':'PumpShotgun','TrenchGun':'PumpShotgun','KSGShotgun':'DualTubePumpShotgun',
  'BenelliShotgun':'TubeFedShotgun','Winchester':'LeverAction','SPSniperRifle':'LeverAction',
  'Crossbow':'SingleRoundBow','M99SniperRifle':'SingleRoundBoltAction','LAW':'SingleRoundLauncher','M32GrenadeLauncher':'CylinderLauncher',
  'SeekerSixRocketLauncher':'MultiTubeLauncher','SealSquealHarpoonBomber':'HarpoonLauncher','Crossbuzzsaw':'SawLauncher',
  'ZEDGun':'EnergyWeapon','ZEDMKIIWeapon':'EnergyWeapon','Huskgun':'ChargeWeapon','FlameThrower':'FuelWeapon','BlowerThrower':'SpecialWeapon',
  'NailGun':'MagazineFedSpecial','Dummy_JoggingWeapon':'StoryTool'}
  magazines=['Single','Deagle','MK23Pistol','M7A3MMedicGun','MP7MMedicGun','M14EBRBattleRifle','ThompsonSMG','AK47AssaultRifle','M4AssaultRifle','AA12AutoShotgun','SCARMK17AssaultRifle','MKb42AssaultRifle','FNFAL_ACOG_AssaultRifle','MAC10MP','Bullpup','MachinePistol']
  families.update({n:'MagazineFed' for n in magazines});families['M4203AssaultRifle']='MagazineFed+UnderbarrelLauncher'
  result=[]
  for key in classes:
   lineage=chain(key)
   if 'KFMod.KFWeapon' not in lineage:continue
   names=[x.split('.')[-1] for x in lineage];family=next((families[n] for n in names if n in families),'Unclassified');d=defaults(names[0],names)
   source=root/'local/sdk'/key.split('.')[0]/'Classes'/(names[0]+'.uc');text=source.read_text() if source.exists() else '';decl=re.search(r'\bclass\s+\w+\s+extends\s+\w+[^;]*;',text,re.I)
   abstract=bool(decl and re.search(r'\babstract\b',decl.group(),re.I));entry=dict(class_name=key,ancestors=lineage[1:],abstract=abstract,candidate_reload_family=family,mechanical_profile_verified=False,defaults=d)
   result.append(entry)
  (root/'reports/weapon-defaults.json').write_text(json.dumps(result,indent=2))
  lines=['# Installed weapon coverage','',f'{len(result)} KFWeapon-derived classes, including abstract bases, skins, story tools and legacy variants. This is not a count of trader weapons.','', 'Class names and inheritance come from the installed packages. Numerical defaults are read from loaded class-default buffers; they are not current perk-modified statistics. Candidate mechanical families need model/animation review before activation. No weapon profile is enabled by this ledger.','', '| Class | Candidate family | Capacity | Reload seconds | Notes |','| --- | --- | ---: | ---: | --- |']
  for e in result:
   d=e['defaults'] or {};lines.append(f"| {e['class_name']} | {e['candidate_reload_family']} | {d.get('MagCapacity','?')} | {d.get('ReloadRate','?')} | {'Abstract base' if e['abstract'] else 'Pending interaction/asset validation'} |")
  (root/'docs/WEAPONS.md').write_text('\n'.join(lines)+'\n');print('Catalogued',len(result),'classes;',sum(e['defaults'] is not None for e in result),'live default buffers verified')
 finally:o.close()
if __name__=='__main__':main()
