"""Exercise real rejected magazine releases before a successful reload."""
import json
import math
import pathlib
from validate_grip_pose import rotate


def validate(folder, weapon="single"):
    folder = pathlib.Path(folder)
    controls = [json.loads(x) for x in (folder / "controls.jsonl").read_text().splitlines()]
    rows = [json.loads(x) for x in (folder / "weapon-observations.jsonl").read_text().splitlines()]
    gun_type = {"single": "KFVRTrackedSingle", "deagle": "KFVRTrackedDeagle", "mk23": "KFVRTrackedMK23"}[weapon]
    addresses = {o["address"] for r in rows for o in r["objects"] if o["type"] == gun_type}
    def objects(kind, selected=rows):
        return [o["values"] for r in selected for o in r["objects"]
                if o["type"] == kind and not o["values"].get("bDeleteMe")
                and (kind != "KFVRManualReload" or o["values"]["Gun"] in addresses)]
    def selected(phase):
        times = [c["uptime_ms"] for c in controls if c["phase"] == phase and c["sent"]]
        return [r for r in rows if times and min(times) + 200 <= r["uptime_ms"] <= max(times)]
    checks = {}
    def at(name, phase, predicate):
        data = objects("KFVRManualReload", selected(phase))
        checks[name] = bool(data) and all(predicate(m) for m in data)
    manuals = objects("KFVRManualReload")
    if not manuals:
        raise ValueError("No physical reload observed")
    guns = objects(gun_type)
    capacity = guns[0]["MagCapacity"]
    budget = sum(manuals[0][k] for k in ("OwnedRounds", "SpentRounds", "DiscardedRounds"))
    checks["every_sample_conserves_ammo"] = all(m["bValid"] and sum(m[k] for k in ("OwnedRounds", "SpentRounds", "DiscardedRounds")) == budget for m in manuals)
    at("initial_catch", 5, lambda m: m["HeldMagazine"] == 1 and m["bChambered"])
    at("retained_chamber_fires", 7, lambda m: m["SpentRounds"] == 1 and not m["bChambered"])
    at("partial_magazine_stored", 10, lambda m: m["StowCount"] == 1 and m["HeldMagazine"] == 0)
    at("upside_down_attempt_holds_new_magazine", 12, lambda m: m["HeldMagazine"] == 2 and m["InsertedMagazine"] == 0)
    at("upside_down_release_rejected", 13, lambda m: m["HeldMagazine"] == 0 and m["InsertedMagazine"] == 0 and m["SeatCount"] == 0 and m["DropCount"] == 1 and m["DiscardedRounds"] == capacity)
    at("sideways_attempt_refills_empty_slot", 15, lambda m: m["HeldMagazine"] == 2 and m["InsertedMagazine"] == 0 and m["GrabCount"] == 2 and m["DiscardedRounds"] == capacity)
    at("sideways_release_rejected", 16, lambda m: m["HeldMagazine"] == 0 and m["InsertedMagazine"] == 0 and m["SeatCount"] == 0 and m["DropCount"] == 2 and m["DiscardedRounds"] == 2 * capacity)
    at("aligned_attempt_refills_empty_slot", 18, lambda m: m["HeldMagazine"] == 2 and m["InsertedMagazine"] == 0 and m["GrabCount"] == 3 and m["DiscardedRounds"] == 2 * capacity)
    at("aligned_push_seats_once_without_feeding", 20, lambda m: m["HeldMagazine"] == 0 and m["InsertedMagazine"] == 2 and m["SeatCount"] == 1 and not m["bChambered"] and m["SpentRounds"] == 1)
    at("full_slide_pull_still_blocks_fire", 23, lambda m: m["SlideTravel"] > .9 and not m["bChambered"] and m["SpentRounds"] == 1)
    at("slide_return_chambers", 24, lambda m: m["RackCount"] == 1 and m["bChambered"] and m["LoadedRounds"] == capacity)
    at("stock_shot_consumes_and_feeds", 26, lambda m: m["SpentRounds"] == 2 and m["LoadedRounds"] == capacity - 1)
    checks["no_stock_timed_reload"] = not any(g["bIsReloading"] for g in guns)
    rigs = objects("KFVRWeaponRig")
    checks["two_real_stock_shots"] = rigs[-1]["TraceCount"] - rigs[0]["TraceCount"] == 2 and guns[-1]["VRFireAttempts"] - guns[0]["VRFireAttempts"] == 2
    checks["released_at_stop"] = not rigs[-1]["bTracking"] and not rigs[-1]["bFiring"]
    # Independently read actual magazine bone axes, not the gate's reported
    # cosine. Prove the negative cases were near enough to seat and preserved
    # forward aim, so distance/yaw cannot accidentally make this test pass.
    frames = {}
    for phase, expected_cosine in ((12, -1), (15, 0), (19, 1)):
        values = []
        for r in selected(phase):
            if not r.get("rig_sample_consistent"):
                continue
            objs = {o["address"]: o for o in r["objects"] if not o["values"].get("bDeleteMe")}
            rig = next((o["values"] for o in objs.values() if o["type"] == "KFVRWeaponRig"), None)
            if not rig or rig["Gun"] not in addresses or not rig["bTracking"]:
                continue
            v = objs[rig["Visual"]]["values"]
            expected = [rotate(v["Magazine" + axis], v["Rotation"]) for axis in ("X", "Y", "Z")]
            actual = [v["MeasuredMagazine" + axis] for axis in ("X", "Y", "Z")]
            dots = [sum(a*b for a,b in zip(expected_axis, actual_axis)) for expected_axis,actual_axis in zip(expected, actual)]
            rest = [a+b for a,b in zip(v["Location"], rotate([x*v["DrawScale"] for x in v["MagazineLocal"]], v["Rotation"]))]
            forward = [rotate([1,0,0], rig[k]) for k in ("Tracking.LeftRotation", "Tracking.RightRotation")]
            values.append(((sum(dots)-1)/2, math.dist(rest,v["MeasuredMagazinePosition"])/rig["Tracking.UnitsPerMetre"], sum(a*b for a,b in zip(*forward))))
        frames[str(phase)] = {"samples": len(values), "max_angle_cosine_error": max((abs(v[0]-expected_cosine) for v in values),default=None), "max_distance_metres": max((v[1] for v in values),default=None), "min_controller_forward_dot": min((v[2] for v in values),default=None)}
        checks["actual_magazine_frame_phase_" + str(phase)] = len(values) >= 3 and all(abs(c-expected_cosine)<.002 and distance<.12 and forward>.99 for c,distance,forward in values)
    report = dict(weapon=weapon, passed=all(checks.values()), checks=checks, actual_frames=frames, initial_rounds=budget, remaining_rounds=manuals[-1]["OwnedRounds"], discarded_rounds=manuals[-1]["DiscardedRounds"], synthetic_controllers=True, live_game=True, headset_presented=False)
    (folder / "reload-alignment-validation.json").write_text(json.dumps(report,indent=2)+"\n")
    if not report["passed"]:
        raise ValueError("Reload alignment checks failed: " + ", ".join(k for k,v in checks.items() if not v))
    return report
