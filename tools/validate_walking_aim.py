"""Read independent live bone/pose samples during fixed-controller locomotion."""
import json, math
from pathlib import Path

def validate(folder):
    folder=Path(folder)
    rows=[json.loads(s) for s in (folder/'weapon-observations.jsonl').read_text().splitlines()]
    controls=[json.loads(s) for s in (folder/'controls.jsonl').read_text().splitlines()]
    def sub(a,b):return [x-y for x,y in zip(a,b)]
    def rotate(v,r):
        p,y,roll=[x*2*math.pi/65536 for x in r];cp,sp,cy,sy,cr,sr=math.cos(p),math.sin(p),math.cos(y),math.sin(y),math.cos(roll),math.sin(roll)
        axes=[(cp*cy,cp*sy,sp),(sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp),(-cr*sp*cy-sr*sy,-cr*sp*sy+sr*cy,cr*cp)]
        return [sum(v[i]*axes[i][j] for i in range(3)) for j in range(3)]
    phases={};checks={};all_errors=[]
    for phase in [2,3,4,5,6,9,10,11,12,13]:
        times=[c['uptime_ms'] for c in controls if c['phase']==phase and c['sent']]
        samples=[]
        for row in rows:
            if row.get('rig_sample_consistent') is not True:continue
            if not times or not min(times)+400<=row['uptime_ms']<=max(times)-100:continue
            objects={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
            rigs=[o['values'] for o in objects.values() if o['type']=='KFVRWeaponRig']
            pawns=[o['values'] for o in objects.values() if o['type']=='KFHumanPawn']
            if len(rigs)!=1 or len(pawns)!=1:continue
            rig=rigs[0];pawn=pawns[0]
            if not rig.get('bTracking') or rig.get('bHolstered'):continue
            visual=objects.get(rig['Visual'],{}).get('values');hand=next((o['values'] for o in objects.values() if o['type']=='KFVRHandVisual' and not o['values']['bLeftHand']),None)
            if not visual or not hand or visual.get('bHidden') or hand.get('bHidden'):continue
            gun=objects.get(rig['Gun'],{})
            tip=rotate([x*visual['DrawScale'] for x in visual['TipLocal']],visual['Rotation'])
            actual=sub(rig['MuzzleLocation'],visual['Location'])
            palm=rotate([x*visual['DrawScale'] for x in visual['PalmLocal']],visual['Rotation'])
            palm=[x+y for x,y in zip(palm,visual['Location'])]
            expected=rotate([.5*rig['Tracking.UnitsPerMetre'],.2*rig['Tracking.UnitsPerMetre'],-.25*rig['Tracking.UnitsPerMetre']],rig['Tracking.BodyRotation'])
            errors=dict(muzzle_bone=math.dist(tip,actual),gun_to_hand=math.dist(palm,hand['PalmWorld']),hand_to_head=math.dist(sub(hand['PalmWorld'],rig['Tracking.HeadPosition']),expected))
            all_errors.append(errors)
            samples.append(dict(weapon=gun.get('type'),pawn=pawn['Location'],speed=math.dist(pawn['Velocity'],[0,0,0]),anchor=sub(rig['Tracking.HeadPosition'],pawn['Location']),eye_height=pawn['EyeHeight'],walk_bob=pawn['WalkBob'],errors=errors,aim=rig['AimRotation'],body=rig['Tracking.BodyRotation']))
        phases[str(phase)]={'samples':len(samples),'weapons':sorted({s['weapon'] for s in samples}),'max_speed':max((s['speed'] for s in samples),default=0),'max_errors':{k:max((s['errors'][k] for s in samples),default=None) for k in ['muzzle_bone','gun_to_hand','hand_to_head']},'samples_detail':samples}
        checks[f'phase_{phase}_coverage']=len(samples)>=3
    for phase in [3,4,5,6,10,11,12,13]:
        data=phases[str(phase)];checks[f'phase_{phase}_pawn_moves']=data['max_speed']>10 and max((math.dist(s['pawn'],data['samples_detail'][0]['pawn']) for s in data['samples_detail']),default=0)>20
    checks['pistol_covered']='KFVRTrackedSingle' in phases['2']['weapons']
    checks['rifle_covered']='KFVRTrackedBullpup' in phases['9']['weapons']
    # Independent measured muzzle bone and hand palm, not only requested poses.
    for key in ['muzzle_bone','gun_to_hand','hand_to_head']:
        checks[key+'_stable']=bool(all_errors) and max(e[key] for e in all_errors)<.1
    rigs=[o['values'] for r in rows for o in r['objects'] if o['type']=='KFVRWeaponRig']
    checks['no_shots_or_recoil']=bool(rigs) and rigs[-1]['TraceCount']==rigs[0]['TraceCount'] and all(r['Recoil']==0 for r in rigs)
    trace_path=folder/'trace-summary.json';anchor={};checks['render_anchor_excludes_desktop_bob']=False
    if trace_path.exists():
        trace=json.loads(trace_path.read_text());body=json.loads((Path(trace['report']).parent/'native-body-camera.json').read_text())
        samples=body['samples'];removed=[math.dist(s['origin'],s['desktop_origin']) for s in samples]
        anchor=dict(samples=len(samples),max_removed_desktop_offset=max(removed,default=0))
        checks['render_anchor_excludes_desktop_bob']=trace['body_anchor_passed'] and sum(d>.5 for d in removed)>=10
    checks['controller_aim_survives_walking']=bool(all_errors) and all(max(abs((a-b+32768)%65536-32768) for a,b in zip(s['aim'],s['body']))<=4 for p in phases.values() for s in p['samples_detail'])
    final=rows[-1]['objects'] if rows else []
    checks['stop_releases_hands']=bool(final) and all(o['values']['bHidden'] for o in final if o['type']=='KFVRHandVisual') and bool(rigs) and not rigs[-1]['bTracking']
    guards=[r.get('rig_sample_consistent') for r in rows]
    report=dict(passed=all(checks.values()),checks=checks,phases=phases,anchor=anchor,crossed_tick_samples=sum(g is False for g in guards),unguarded_samples=sum(g is None for g in guards),synthetic_controllers=True,headset_presented=False)
    (folder/'walking-aim-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    summary={**report,'phases':{k:{a:b for a,b in v.items() if a!='samples_detail'} for k,v in phases.items()}}
    if not report['passed']:raise ValueError('Walking diagnostic failures: '+', '.join(k for k,v in checks.items() if not v))
    return summary

if __name__=='__main__':
    import sys
    print(json.dumps(validate(sys.argv[1]),indent=2))
