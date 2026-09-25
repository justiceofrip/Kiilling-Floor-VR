"""Read-only, bounded x86 control-flow listing from the owned installed binaries.
Reports are private. An export boundary is an audit bound, not proof of function size.
"""
import argparse,hashlib,json,pathlib,sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'local/pydeps'))
import pefile,capstone
from capstone.x86 import X86_OP_IMM,X86_OP_MEM
from native_profile import TARGETS,AUDITED_SHA256
EXTRA=[('Engine.dll','?Render@USkeletalMeshInstance@@UAEXPAVFDynamicActor@@PAVFLevelSceneNode@@PAV?$TList@PAVFDynamicLight@@@@PAV?$TList@PAUFProjectorRenderInfo@@@@PAVFRenderInterface@@@Z'),('Engine.dll','?Render@AEmitter@@QAEXPAVFDynamicActor@@PAVFLevelSceneNode@@PAV?$TList@PAVFDynamicLight@@@@PAVFRenderInterface@@@Z'),('Engine.dll','?ProcessEvent@AActor@@UAEXPAVUFunction@@PAX1@Z')]
def audit(game,out):
 out.mkdir(parents=True,exist_ok=True);reports=[];decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);decoder.detail=True
 for file,name in TARGETS+EXTRA:
  path=game/'System'/file;sha=hashlib.sha256(path.read_bytes()).hexdigest()
  if sha!=AUDITED_SHA256[file]:raise ValueError('Unsupported binary '+file)
  pe=pefile.PE(str(path));base=pe.OPTIONAL_HEADER.ImageBase
  exports={base+x.address:x.name.decode() for x in pe.DIRECTORY_ENTRY_EXPORT.symbols if x.name};start=next(at for at,n in exports.items() if n==name);bound=min([at for at in exports if at>start]+[start+65536]);bound=min(bound,start+65536)
  imports={imp.address:entry.dll.decode()+'!'+(imp.name.decode() if imp.name else '#'+str(imp.ordinal)) for entry in pe.DIRECTORY_ENTRY_IMPORT for imp in entry.imports}
  pending=[start];seen={};calls=[];escapes=[]
  while pending:
   at=pending.pop()
   while start<=at<bound and at not in seen and len(seen)<18000:
    instruction=next(decoder.disasm(pe.get_data(at-base,min(15,bound-at)),at,count=1),None)
    if not instruction:escapes.append(dict(at=hex(at-base),reason='undecodable'));break
    ins=instruction;seen[at]=ins;after=at+ins.size
    if ins.group(capstone.CS_GRP_CALL):
     target=ins.operands[0];label='indirect '+ins.op_str
     if target.type==X86_OP_IMM:label=exports.get(target.imm,'sub_'+hex(target.imm-base))
     elif target.type==X86_OP_MEM and not target.mem.base and not target.mem.index:label=imports.get(target.mem.disp,label)
     calls.append(dict(at=hex(at-base),target=label))
    if ins.group(capstone.CS_GRP_JUMP):
     op=ins.operands[0]
     if op.type==X86_OP_IMM:
      if start<=op.imm<bound:pending.append(op.imm)
      else:escapes.append(dict(at=hex(at-base),reason='branch outside export bound',target=exports.get(op.imm,hex(op.imm-base))))
     else:escapes.append(dict(at=hex(at-base),reason='indirect jump',target=ins.op_str))
     if ins.mnemonic=='jmp':break
    if ins.group(capstone.CS_GRP_RET):break
    at=after
  labels={int(c['at'],16)+base:c['target'] for c in calls}
  listing='\n'.join(f'{at-base:08x} {ins.mnemonic} {ins.op_str}'+(' ; '+labels[at] if at in labels else '') for at,ins in sorted(seen.items()))
  (out/(file+'-'+hex(start-base)+'.asm')).write_text(listing+'\n')
  reports.append(dict(file=file,sha256=sha,symbol=name,rva=hex(start-base),next_export_bound=hex(bound-base),instructions=len(seen),calls=calls,unresolved_branches=escapes))
 (out/'calls.json').write_text(json.dumps(reports,indent=2));print(json.dumps([dict(symbol=r['symbol'],instructions=r['instructions'],calls=len(r['calls']),unresolved_branches=len(r['unresolved_branches'])) for r in reports],indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args();audit(a.game,a.out)
