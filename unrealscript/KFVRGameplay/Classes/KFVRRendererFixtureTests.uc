// Exercise the real installed GameInfo URL parser at the production boundary.
class KFVRRendererFixtureTests extends Commandlet;
var int Checks,Failures;
function Check(bool Condition,string Message)
{
    Checks++;
    if (!Condition) { Failures++; Log("KFVR_RENDERER_FIXTURE_TEST_FAIL:" @ Message); }
}
event int Main(string Params)
{
    local string URL,Options;
    Options="?Game=KFMod.KFGameType?Mutator=KFVRGameplay.KFVRDevMutator?Difficulty=1?KFVRUnattended=1?KFVRRendererTest=1?KFVRNativeHands=1?KFVRPhysicalReloads=1";
    URL="KF-Offices.rom" $ Options;
    Check(class'GameInfo'.static.ParseOption(URL,"KFVRRendererTest")=="", "Installed parser requires an options suffix, not a full map URL");
    Check(class'KFVRDevMutator'.static.LocalURLOption(URL,"KFVRRendererTest")=="1", "Actual full launcher URL enables the renderer flag");
    Check(class'KFVRDevMutator'.static.LocalURLOption(Options,"KFVRRendererTest")=="1", "Already separated options remain supported");
    Check(class'KFVRDevMutator'.static.LocalURLOption("KF-Offices.rom","KFVRRendererTest")=="", "Map without options has no renderer flag");
    Check(class'KFVRDevMutator'.static.LocalURLOption("KF-Offices.rom?Name=KFVRRendererTest=1","KFVRRendererTest")=="", "Flag text inside another option is not an enabling key");
    Check(class'KFVRDevMutator'.static.RendererFixtureAllowed(URL,True), "Full URL admits its own standalone Offices map");
    Check(class'KFVRDevMutator'.static.RendererFixtureAllowed("KF-Offices" $ Options,True), "Exact map without extension is supported");
    Check(class'KFVRDevMutator'.static.RendererFixtureAllowed("kf-offices.rom?kfvrrenderertest=1?kfvrunattended=1",True), "Engine option and map case rules are preserved");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed("KF-BioticsLab.rom" $ Options,True), "Other local map URLs reject the fixed-coordinate fixture");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed("KF-Offices-Other.rom" $ Options,True), "Local map prefix is not sufficient");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed(URL,False), "Network play rejects the fixture");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed("KF-Offices.rom?KFVRRendererTest=1",True), "Unattended flag is required");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed("KF-Offices.rom?KFVRUnattended=1",True), "Renderer flag is required");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed("KF-Offices.rom?KFVRRendererTest=10?KFVRUnattended=1",True), "Renderer value must be exactly one");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed("KF-Offices.rom?KFVRRendererTest=1?KFVRUnattended=10",True), "Unattended value must be exactly one");
    Check(class'KFVRDevMutator'.static.LocalURLMap(URL)=="KF-OFFICES.ROM", "Map identity comes from the same actual level URL as its options");
    Check(class'KFVRDevMutator'.static.LocalURLMap("KF-Offices")=="KF-OFFICES", "Optionless map prefix can still be identified");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed("Entry" $ Options,True), "An Entry level URL does not enable the benchmark");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed(Options,True), "Options alone cannot supply the required map identity");
    Check(!class'KFVRDevMutator'.static.RendererFixtureAllowed("KF-BioticsLab.rom?Name=KF-Offices.rom" $ Options,True), "Map text in an option cannot override the actual level prefix");
    Log("KFVR_RENDERER_FIXTURE_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    return Failures;
}
defaultproperties
{
    IsClient=False
    IsServer=False
    IsEditor=True
    LogToStdout=True
}
