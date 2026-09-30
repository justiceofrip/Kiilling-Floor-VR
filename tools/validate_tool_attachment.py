"""Independent working-axis/grip checks from the real rendered tool and rig."""
import math


def attachment_checks(rows, controls, weapon_type, phases, forward_phase):
    def axes(rotation):
        p,y,r=[v*2*math.pi/65536 for v in rotation]
        cp,sp,cy,sy,cr,sr=math.cos(p),math.sin(p),math.cos(y),math.sin(y),math.cos(r),math.sin(r)
        return ((cp*cy,cp*sy,sp),(sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp),
                (-cr*sp*cy-sr*sy,sr*cy-cr*sp*sy,cr*cp))
    def transform(v,basis):
        return tuple(sum(v[j]*basis[j][i] for j in range(3)) for i in range(3))
    def direction(rotation):return axes(rotation)[0]
    checks={};counts={};errors={k:[] for k in ['grip_metres','trace_tip_metres','model_axis','bone_axis','trace_axis','forward_axis']}
    for phase in phases:
        times=[c['uptime_ms'] for c in controls if c['phase']==phase and c['sent']]
        selected=[r for r in rows if times and min(times)+600<=r['uptime_ms']<=max(times)-75]
        count=0
        for row in selected:
            objects={int(o['address'],16):o for o in row['objects'] if not o['values'].get('bDeleteMe')}
            for item in list(objects.values()):
                if item['type']!='KFVRWeaponRig':continue
                rig=item['values'];gun=objects.get(int(rig['Gun'],16));visual=objects.get(int(rig['Visual'],16))
                if not gun or gun['type']!=weapon_type or not visual:continue
                v=visual['values']
                if not rig['bTracking'] or rig['bHolstered'] or v['bHidden'] or not v.get('bToolAligned'):continue
                count+=1;basis=axes(v['Rotation']);units=rig['Tracking.UnitsPerMetre']
                palm=transform(v['PalmLocal'],basis);tip=transform(v['TipLocal'],basis)
                palm=tuple(a+b*v['DrawScale'] for a,b in zip(v['Location'],palm))
                tip=tuple(a+b*v['DrawScale'] for a,b in zip(v['Location'],tip))
                aim=direction(rig['Tracking.RightRotation'])
                errors['grip_metres'].append(math.dist(palm,rig['Tracking.RightPosition'])/units)
                errors['trace_tip_metres'].append(math.dist(tip,rig['MuzzleLocation'])/units)
                errors['model_axis'].append(math.dist(transform(v['ToolAxis'],basis),aim))
                errors['bone_axis'].append(math.dist(v['MeasuredToolAxis'],aim))
                errors['trace_axis'].append(math.dist(direction(rig['AimRotation']),aim))
                if phase==forward_phase:errors['forward_axis'].append(math.dist(aim,direction(rig['Tracking.BodyRotation'])))
        counts[str(phase)]=count
    checks['tool_attachment_all_selected_poses_sampled']=all(counts.values())
    for name,values in errors.items():
        checks['tool_attachment_'+name]=bool(values) and max(values)<(.001 if name.endswith('metres') else .005)
    return checks,dict(samples_by_phase=counts,max_errors={k:max(v,default=None) for k,v in errors.items()},headset_presented=False)
