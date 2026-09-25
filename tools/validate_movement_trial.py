"""Validate the bounded synthetic movement sequence; no game writes here."""
import json,pathlib

def validate(native_path,controls_path,result_path):
 native=json.loads(pathlib.Path(native_path).read_text())
 controls=[json.loads(line) for line in pathlib.Path(controls_path).read_text().splitlines()]
 checks={};details={}
 for phase in range(12):
  samples=[s for s in controls if s['phase']==phase and (s['sent'] or phase==8)]
  if not samples:checks[str(phase)]=False;continue
  start=samples[0]['uptime_ms']+180;end=samples[-1]['uptime_ms']
  rows=[r for r in native['movement'] if start<=r['uptime_ms']<=end]
  expected={1:(.25,0),3:(0,.25),7:(-.25,0)}.get(phase,(0,0))
  consumed=[(r['consumed_forward']/6000,r['consumed_strafe']/6000) for r in rows]
  matching=sum(abs(f-expected[0])<.01 and abs(s-expected[1])<.01 for f,s in consumed)
  # Nonzero phases must reach KF input; interruption phases must stay neutral.
  checks[str(phase)]=len(rows)>=3 and (matching>=3 if phase in (1,3,7) else matching==len(rows))
  details[str(phase)]={'samples':len(rows),'matching_consumed_axes':matching,'expected':expected}
 checks['restoration']=native['movement_writes']>0 and native['movement_writes']==native['movement_restores'] and native['restore_failures']==0 and all(r['finite'] and r['restored'] for r in native['movement'])
 result={'passed':all(checks.values()),'checks':checks,'phases':details,'avatar_motion_accepted':False,'headset_accepted':False}
 pathlib.Path(result_path).write_text(json.dumps(result,indent=2))
 if not result['passed']:raise RuntimeError('Movement consumption/release trial failed: '+str(result_path))
 return result
