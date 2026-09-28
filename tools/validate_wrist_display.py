"""Check wrist-HUD visibility and placement against a real KF actor snapshot."""
import json
import math
from pathlib import Path

def validate(folder):
    folder=Path(folder)
    rows=[json.loads(line) for line in (folder/'weapon-observations.jsonl').read_text().splitlines()]
    controls=[json.loads(line) for line in (folder/'controls.jsonl').read_text().splitlines()]
    def objects(kind,selected=rows):
        return [o['values'] for row in selected for o in row['objects'] if o['type']==kind and not o['values'].get('bDeleteMe')]
    def phase(n):
        times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
        return objects('KFVRStatusDisplay',[r for r in rows if times and min(times)+500<=r['uptime_ms']<=max(times)-50])
    checks={}
    hidden={0:'ordinary_pose',2:'palm_turned_away',3:'hand_lowered',4:'too_close',5:'too_far',6:'gripping',7:'trigger_pressed',9:'focus_lost',11:'tracking_lost',14:'below_exit_angle',15:'weak_angle_cannot_reopen',17:'returned_to_ordinary_pose'}
    visible={1:'deliberate_wrist_turn',8:'released_grip',10:'focus_recovered',12:'tracking_recovered',13:'hysteresis_holds_reading',16:'full_angle_reopens'}
    for n,name in hidden.items():
        data=phase(n);checks[name+'_hidden']=bool(data) and all(p['bHidden'] and not p['bWristRequested'] for p in data)
    for n,name in visible.items():
        data=phase(n);checks[name+'_visible']=bool(data) and all(not p['bHidden'] and p['bWristRequested'] for p in data)
    panels=objects('KFVRStatusDisplay');rigs=objects('KFVRWeaponRig')
    checks['native_text_was_rendered']=bool(panels) and panels[-1]['RenderCalls']>0
    checks['no_per_eye_text_revision']=bool(panels) and panels[-1]['RenderCalls']<=panels[-1]['TextureUpdates']
    errors=[];sizes=[];axes=[];vitals=[]
    for row in rows:
        rr=objects('KFVRWeaponRig',[row]);pp=objects('KFVRStatusDisplay',[row]);pawns=objects('KFHumanPawn',[row])
        if len(rr)!=1:continue
        r=rr[0];units=r.get('Tracking.UnitsPerMetre',0)
        if units<=0:continue
        for p in pp:
            if p['bHidden']:continue
            yaw=r['Tracking.BodyRotation'][1]*2*math.pi/65536
            x=(math.cos(yaw),math.sin(yaw),0)
            expected=[r['Tracking.LeftPosition'][i]-.03*units*x[i]+(.09*units if i==2 else 0) for i in range(3)]
            errors.append(math.dist(expected,p['Location'])/units)
            sizes.append(p['DrawScale']*100/units)
            pitch,turn,_=p['Rotation'];pitch*=2*math.pi/65536;turn*=2*math.pi/65536
            normal=(math.cos(pitch)*math.cos(turn),math.cos(pitch)*math.sin(turn),math.sin(pitch))
            delta=[h-a for h,a in zip(r['Tracking.HeadPosition'],p['Location'])];length=math.sqrt(sum(v*v for v in delta))
            axes.append(sum(a*b for a,b in zip(normal,delta))/length)
            if pawns:vitals.append(p['DisplayedHealth']==pawns[0]['Health'] and p['DisplayedArmor']==int(pawns[0]['ShieldStrength']))
    checks['actual_card_is_20_cm_wide']=bool(sizes) and all(abs(size-.20)<.001 for size in sizes)
    checks['actual_card_follows_wrist_offset']=bool(errors) and max(errors)<.001
    checks['actual_card_faces_head']=bool(axes) and min(axes)>.999
    checks['visible_vitals_match_pawn']=bool(vitals) and all(vitals)
    checks['display_releases_at_stop']=bool(panels) and panels[-1]['bHidden'] and not panels[-1]['bWristRequested']
    checks['no_gameplay_from_hud_gestures']=bool(rigs) and rigs[-1]['TraceCount']==rigs[0]['TraceCount'] and rigs[-1]['UseCount']==rigs[0]['UseCount']
    report=dict(passed=all(checks.values()),checks=checks,render_calls=panels[-1]['RenderCalls'] if panels else 0,max_position_error_metres=max(errors,default=None),minimum_facing_dot=min(axes,default=None),live_game=True,synthetic_controllers=True,headset_presented=False,visual_inspection_separate=True)
    (folder/'wrist-display-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    if not report['passed']:raise ValueError('Wrist display checks failed: '+', '.join(k for k,v in checks.items() if not v))
    return report

if __name__=='__main__':
    import sys
    print(json.dumps(validate(sys.argv[1]),indent=2))
