// Native-world status card. Rendering reads a Tick snapshot; it never advances gameplay.
class KFVRStatusDisplay extends Actor;
#exec STATICMESH IMPORT NAME=KFVRStatusPanel FILE=Models\KFVRStatusPanel.lwo
var KFVRWeaponRig Rig;
var ScriptedTexture Surface;
var ConstantColor White;
var FinalBlend PanelMaterial;
var Font TextFont;
var KFWeapon DisplayedGun;
var int DisplayedHealth,DisplayedArmor,DisplayedLoaded,DisplayedSpare,DisplayedLeft,DisplayedRight;
var int TextureUpdates,RenderCalls;
var bool bDual,bMelee;
var float HealthFraction,AttachmentError;
var vector ExpectedPosition;
var string WeaponLine,VitalLine,AmmoLine,ActionLine,PreviousText;
simulated function bool Initialize(KFVRWeaponRig R)
{
    Rig=R;
    TextFont=class'HUDKillingFloor'.static.LoadSmallFontStatic(0);
    if (TextFont==None) return False;
    Surface=ScriptedTexture(Level.ObjectPool.AllocateObject(class'ScriptedTexture'));
    if (Surface==None) return False;
    White=new(None) class'ConstantColor'; White.Color=MakeColor(255,255,255);
    Surface.SetSize(512,256); Surface.Client=self;
    PanelMaterial=new(None) class'FinalBlend'; PanelMaterial.Material=Surface;
    PanelMaterial.FrameBufferBlending=FB_Overwrite; PanelMaterial.ZTest=True;
    PanelMaterial.ZWrite=True; PanelMaterial.TwoSided=True; Skins[0]=PanelMaterial;
    return True;
}
simulated function color MakeColor(byte R,byte G,byte B)
{
    local color C;
    C.R=R; C.G=G; C.B=B; C.A=255; return C;
}
simulated function UpdateDisplay()
{
    local vector X,Y,Z,Front,Right,Up;
    local KFVRTrackedDualies D;
    local string Combined;
    if (Rig==None || Surface==None || !Rig.Active() || (Rig.Tracking.Flags & 1)==0)
    { bHidden=True; return; }
    GetAxes(Rig.Tracking.BodyRotation,X,Y,Z);
    ExpectedPosition=Rig.Tracking.LeftPosition+(Z*0.12-X*0.03)*Rig.Tracking.UnitsPerMetre;
    SetLocation(ExpectedPosition); SetDrawScale(Rig.Tracking.UnitsPerMetre*0.28/100.0);
    Front=Normal(Rig.Tracking.HeadPosition-ExpectedPosition);
    Right=Normal(Z cross Front); Up=Normal(Front cross Right);
    if (VSize(Right)>0.1) SetRotation(OrthoRotation(Front,Right,Up));
    AttachmentError=VSize(Location-ExpectedPosition);
    // A free, raised offhand exposes the card; grabbing/firing or lowering it hides it.
    bHidden=Rig.Tracking.LeftGrip>0.2 || Rig.Tracking.LeftTrigger>0.2
        || VSize(Rig.Tracking.LeftPosition-Rig.Tracking.HeadPosition)>0.75*Rig.Tracking.UnitsPerMetre;
    DisplayedGun=Rig.Gun;
    if (DisplayedGun==None || Rig.PC.Pawn==None) { bHidden=True; return; }
    DisplayedHealth=Max(0,Rig.PC.Pawn.Health); DisplayedArmor=Max(0,int(Rig.PC.Pawn.ShieldStrength));
    HealthFraction=FClamp(float(DisplayedHealth)/FMax(1,Rig.PC.Pawn.HealthMax),0,1);
    DisplayedLoaded=Max(0,DisplayedGun.MagAmmoRemaining);
    DisplayedSpare=Max(0,DisplayedGun.AmmoAmount(0)-DisplayedLoaded);
    WeaponLine=DisplayedGun.ItemName;
    VitalLine="HEALTH " $ DisplayedHealth $ "    ARMOR " $ DisplayedArmor;
    D=KFVRTrackedDualies(DisplayedGun); bDual=D!=None; bMelee=KFVRTrackedKnife(DisplayedGun)!=None;
    DisplayedLeft=0; DisplayedRight=DisplayedLoaded;
    if (D!=None)
    {
        DisplayedLeft=D.LeftLoaded; DisplayedRight=D.RightLoaded;
        AmmoLine="L " $ DisplayedLeft $ "   R " $ DisplayedRight $ "    SPARE " $ DisplayedSpare;
    }
    else if (bMelee) AmmoLine="MELEE";
    else AmmoLine="LOADED " $ DisplayedLoaded $ "    SPARE " $ DisplayedSpare;
    ActionLine="READY";
    if (Rig.bHolstered) ActionLine="HOLSTERED";
    else if (DisplayedGun.bIsReloading) ActionLine="RELOADING";
    else if (D!=None && D.Belt!=None)
    {
        if (D.Belt.bRightLatched || D.Belt.bLeftLatched) ActionLine="LIFT PISTOL FROM BELT";
        else if (D.Belt.RightPresented>0 || D.Belt.LeftPresented>0) ActionLine="MAGAZINES AT BELT";
        else ActionLine="BELT RELOAD";
    }
    else if (Rig.ManualReload!=None)
    {
        if (!Rig.ManualReload.bChambered) ActionLine="CHAMBER EMPTY";
        else ActionLine="MANUAL RELOAD";
    }
    Combined=WeaponLine $ "|" $ VitalLine $ "|" $ AmmoLine $ "|" $ ActionLine;
    if (Combined!=PreviousText)
    { PreviousText=Combined; Surface.Revision++; TextureUpdates++; }
}
simulated event RenderTexture(ScriptedTexture Tex)
{
    local color Ink,Accent;
    if (Tex!=Surface || White==None || TextFont==None) return;
    RenderCalls++; Ink=MakeColor(236,241,245); Accent=MakeColor(58,212,160);
    if (DisplayedHealth<=30) Accent=MakeColor(246,85,75);
    Tex.DrawTile(0,0,512,256,0,0,1,1,White,MakeColor(12,19,26));
    Tex.DrawTile(0,0,7,256,0,0,1,1,White,Accent);
    Tex.DrawText(20,14,Left(WeaponLine,28),TextFont,Ink);
    Tex.DrawText(20,63,VitalLine,TextFont,Ink);
    Tex.DrawTile(20,104,472,8,0,0,1,1,White,MakeColor(43,55,63));
    Tex.DrawTile(20,104,472*HealthFraction,8,0,0,1,1,White,Accent);
    Tex.DrawText(20,134,AmmoLine,TextFont,Ink);
    Tex.DrawText(20,198,ActionLine,TextFont,Accent);
}
simulated function Destroyed()
{
    Skins[0]=None;
    if (Surface!=None) { Surface.Client=None; Level.ObjectPool.FreeObject(Surface); Surface=None; }
    if (PanelMaterial!=None) PanelMaterial.Material=None;
    PanelMaterial=None; White=None; Super.Destroyed();
}
defaultproperties
{
    DrawType=DT_StaticMesh
    StaticMesh=StaticMesh'KFVRStatusPanel'
    Style=STY_Normal
    Physics=PHYS_None
    RemoteRole=ROLE_None
    bUnlit=True
    bHidden=True
    bCollideActors=False
    bCollideWorld=False
    bBlockActors=False
    bBlockPlayers=False
}
