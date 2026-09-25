"""Read-only input call-path audit using the installed binary identity gates."""
import argparse,pathlib
import audit_native
p=argparse.ArgumentParser();p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args()
audit_native.TARGETS=[
 ('Engine.dll','?ReadInput@UInput@@UAEXMAAVFOutputDevice@@@Z'),
 ('Engine.dll','?ReadInput@UViewport@@UAEXM@Z'),
 ('Engine.dll','?eventPlayerInput@UPlayerInput@@QAEXM@Z'),
 ('Core.dll','?CallFunction@UObject@@UAEXAAUFFrame@@QAXPAVUFunction@@@Z'),
 ('Core.dll','?ProcessInternal@UObject@@QAEXAAUFFrame@@QAX@Z')]
audit_native.EXTRA=[]
audit_native.audit(a.game,a.out)
