"""Measure sleeve-free glove attachment/orientation in the actual KF renderer."""
import json
import math
from pathlib import Path

def validate(folder):
    folder=Path(folder)
    rows=[json.loads(s) for s in (folder/'weapon-observations.jsonl').read_text().splitlines()]
    controls=[json.loads(s) for s in (folder/'controls.jsonl').read_text().splitlines()]
    def objects(kind,selected=rows):
        return [o['values'] for r in selected for o in r['objects'] if o['type']==kind and not o['values'].get('bDeleteMe')]
    def phase(n):
        times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
        return objects('KFVRHandVisual',[r for r in rows if times and min(times)+700<=r['uptime_ms']<=max(times)-50])
    def dot(a,b):return sum(x*y for x,y in zip(a,b))
    counts=json.loads((folder/'hand-eye-pairs.json').read_text())
    checks={'fresh_stereo_when_looking_straight_down':counts[4]>=10,'fresh_stereo_when_looking_straight_up':counts[6]>=10};settled=[]
    for n in [0,3,4,5,6,7,8,9,10,12,15,17]:
        data=phase(n);settled.extend(data)
        checks[f'pose_{n}_both_original_gloves']=bool(data) and {h['bLeftHand'] for h in data}=={False,True} and all(h['bStandaloneGlove'] and h['bCalibrated'] and h['PoseFrames']>0 and not h['bHidden'] for h in data)
        checks[f'pose_{n}_palm_attachment']=bool(data) and all(h['PalmError']<.05 for h in data)
        checks[f'pose_{n}_palm_orientation']=bool(data) and all(h['PalmAxisError']<.005 for h in data)
        checks[f'pose_{n}_finger_orientation']=bool(data) and all(h['IndexAxisError']<.005 for h in data)
    # Independent OpenXR-to-UE basis and head-relative position comparison.
    # Aim uses these orientations; grip is independently tilted 60 degrees.
    # Position expectations use grip, not the deliberately offset aim origin.
    # These expectations do not use the script's target/error fields.
    def rotate(q,v):
        x,y,z,w=q;vx,vy,vz=v
        tx,ty,tz=2*(y*vz-z*vy),2*(z*vx-x*vz),2*(x*vy-y*vx)
        return (vx+w*tx+y*tz-z*ty,vy+w*ty+z*tx-x*tz,vz+w*tz+x*ty-y*tx)
    def world(v,yaw):
        x,y,z=-v[2],v[0],v[1];a=yaw*2*math.pi/65536;c=math.cos(a);s=math.sin(a)
        return (c*x-s*y,s*x+c*y,z)
    axis_errors=[];relative_errors=[]
    for n in [3,4,5,6,7,8,9,10,12,15,17]:
        times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
        selected=[r for r in rows if times and min(times)+700<=r['uptime_ms']<=max(times)-50]
        for row in selected:
            rigs=objects('KFVRWeaponRig',[row]);hh=objects('KFVRHandVisual',[row])
            if len(rigs)!=1:continue
            rig=rigs[0];head=rig['Tracking.HeadPosition'];yaw=rig['Tracking.BodyRotation'][1];units=rig['Tracking.UnitsPerMetre']
            for h in hh:
                side=-1 if h['bLeftHand'] else 1
                q={6:(0,side*.70710678,0,.70710678),7:(side*.70710678,0,0,.70710678),8:(0,0,side*.70710678,.70710678),9:(side*.3535534,.3535534,side*.1464466,.8535534)}.get(n,(0,0,0,1))
                for key,v in [('PalmForward',(0,0,-1)),('PalmRight',(1,0,0)),('PalmUp',(0,1,0))]:
                    axis_errors.append(math.dist(h[key],world(rotate(q,v),yaw)))
                head_local=(.1,.2,.05) if n==10 else (0,0,0)
                expected=world(tuple(a-b for a,b in zip((side*.18,-.18,-.38),head_local)),yaw)
                actual=tuple((a-b)/units for a,b in zip(h['PalmWorld'],head))
                relative_errors.append(math.dist(expected,actual))
    checks['actual_palm_axes_follow_aim_despite_grip_tilt']=bool(axis_errors) and max(axis_errors)<.005
    checks['palms_keep_grip_position_despite_offset_aim_origin']=bool(relative_errors) and max(relative_errors)<.001
    opened=phase(3);closed=phase(5)
    checks['free_hands_can_fully_open']=bool(opened) and all(h['GripRole']==0 and h['IndexCurl']==0 and h['GripCurl']==0 and dot(h['IndexDirection'],h['PalmForward'])>.995 for h in opened)
    checks['fists_curl_in_opposite_palm_directions']=bool(closed) and all(h['GripRole']==0 and h['IndexCurl']>.99 and h['GripCurl']>.99 and dot(h['IndexDirection'],h['PalmRight'])*(-1 if h['bLeftHand'] else 1)>.5 and dot(h['IndexDirection'],h['PalmForward'])<-.8 for h in closed)
    # Anatomical landmarks are independent of the script's target/error fields.
    # Native KF's pistol index points along actor-forward, with the thumb side up.
    def delta(a,b):return tuple(x-y for x,y in zip(a,b))
    checks['open_index_extends_forward_from_knuckle']=bool(opened) and all(dot(delta(h['IndexTipWorld'],h['IndexBaseWorld']),h['PalmForward'])>1 for h in opened)
    checks['knuckles_are_forward_of_palm']=bool(settled) and all(dot(delta(h['IndexBaseWorld'],h['PalmWorld']),h['PalmForward'])>1 for h in settled)
    # Native KF geometry has a measured 1.71 raw-unit thumb/little height
    # difference (1.426 UU at 50/60 scale); the authored gloves are broader.
    def landmark_height(h):return 1.0 if h.get('bNativeHandMesh') else 2.0
    checks['thumb_and_index_side_is_above_little_finger']=bool(settled) and all(dot(delta(h['ThumbBaseWorld'],h['LittleBaseWorld']),h['PalmUp'])>landmark_height(h) and dot(delta(h['IndexBaseWorld'],h['LittleBaseWorld']),h['PalmUp'])>landmark_height(h) for h in settled)
    native_requested=any(r.get('bNativeHandTest') for r in objects('KFVRWeaponRig'))
    checks['requested_hand_asset_loaded']=bool(settled) and all(bool(h.get('bNativeHandMesh'))==native_requested for h in settled)
    checks['finger_positions_remain_hand_sized']=bool(settled) and all(.2<math.dist(h['PalmWorld'],h['IndexTipWorld'])<10 for h in settled)
    for n,side in [(13,True),(14,False)]:
        data=phase(n)
        checks[f'phase_{n}_only_lost_hand_hidden']=bool(data) and all(h['bHidden']==(h['bLeftHand']==side) for h in data)
    # Capacitive thumb touch has its own articulated native rest pose. Release
    # lifts each joint instead of flattening all three axes into one direction.
    from validate_grip_pose import rotate
    touched=phase(4);released=phase(3)
    native_touched=[h for h in touched if h.get('bNativeHandMesh')]
    if native_touched:
        thumb_errors=[math.dist(h[f'FingerWorld.{j}.{axis}'],rotate(h[f'NativeBind.{j}.{axis}'],h['Rotation'])) for h in native_touched for j in range(3) for axis in ['XAxis','YAxis','ZAxis']]
        checks['touched_thumb_matches_native_joint_frames']=max(thumb_errors)<.005
        checks['touch_settles_without_forcing_grip_curl']=all(h['InputThumbCurl']==1 and h['ThumbPoseBlend']>.999 and h['GripCurl']==0 for h in native_touched)
        checks['released_thumb_is_raised_and_articulated']=bool(released) and all(h['ThumbPoseBlend']<.001 and dot(h['FingerWorld.2.XAxis'],h['PalmUp'])>.4 and dot(h['FingerWorld.0.XAxis'],h['FingerWorld.2.XAxis'])<.94 for h in released)
    checks['focus_loss_hides_both']=bool(phase(11)) and all(h['bHidden'] for h in phase(11))
    final=objects('KFVRHandVisual',rows[-1:]);rigs=objects('KFVRWeaponRig')
    checks['stop_releases_presentation']=len(final)==2 and all(h['bHidden'] for h in final) and bool(rigs) and not rigs[-1]['bTracking']
    checks['no_firing_from_free_hand_gestures']=bool(rigs) and rigs[-1]['TraceCount']==rigs[0]['TraceCount']
    report=dict(passed=all(checks.values()),checks=checks,native_hands_requested=native_requested,max_errors={k:max((h[k] for h in settled),default=None) for k in ['PalmError','PalmAxisError','IndexAxisError']},max_external_axis_error=max(axis_errors,default=None),max_external_head_relative_error_metres=max(relative_errors,default=None),live_game=True,synthetic_controllers=True,headset_presented=False,visual_inspection_separate=True)
    (folder/'hand-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    if not report['passed']:raise ValueError('Hand checks failed: '+', '.join(k for k,v in checks.items() if not v))
    return report

if __name__=='__main__':
    import sys
    print(json.dumps(validate(sys.argv[1]),indent=2))
