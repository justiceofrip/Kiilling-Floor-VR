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
var bool bWristRequested;
var float WristRevealTime,PalmFacing,HeadDistanceMetres,HeightFromHeadMetres;
var float DisplayWidthMetres;
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
simulated function HideDisplay()
{
    bHidden=True; bWristRequested=False; WristRevealTime=-1.0;
}
simulated function UpdateDisplay()
{
    local vector X,Y,Z,Front,Right,Up,PalmX,PalmY,PalmZ,ToHead;
    local bool Allowed;
    local KFVRTrackedDualies D;
    local string Combined;
    if (Rig==None || Surface==None || !Rig.Active() || (Rig.Tracking.Flags & 1)==0)
    { HideDisplay(); return; }
    GetAxes(Rig.Tracking.BodyRotation,X,Y,Z);
    ExpectedPosition=Rig.Tracking.LeftPosition+(Z*0.09-X*0.03)*Rig.Tracking.UnitsPerMetre;
    SetLocation(ExpectedPosition); SetDrawScale(Rig.Tracking.UnitsPerMetre*DisplayWidthMetres/100.0);
    Front=Normal(Rig.Tracking.HeadPosition-ExpectedPosition);
    Right=Normal(Z cross Front); Up=Normal(Front cross Right);
    if (VSize(Right)>0.1) SetRotation(OrthoRotation(Front,Right,Up));
    AttachmentError=VSize(Location-ExpectedPosition);
    // Deliberately turn the free left palm toward the eyes to read the card.
    // A smaller card stays hidden during ordinary aiming, gripping or reaching.
    GetAxes(Rig.Tracking.LeftRotation,PalmX,PalmY,PalmZ);
    ToHead=Rig.Tracking.HeadPosition-Rig.Tracking.LeftPosition;
    HeadDistanceMetres=VSize(ToHead)/Rig.Tracking.UnitsPerMetre;
    HeightFromHeadMetres=(-ToHead dot Z)/Rig.Tracking.UnitsPerMetre;
    PalmFacing=PalmY dot Normal(ToHead);
    Allowed=Rig.Tracking.LeftGrip<=0.2 && Rig.Tracking.LeftTrigger<=0.2
        && Rig.HandGripRole(True)==0 && HeadDistanceMetres>=0.25 && HeadDistanceMetres<=0.70
        && HeightFromHeadMetres>=-0.42 && HeightFromHeadMetres<=0.28
        && (-ToHead dot X)>0.12*Rig.Tracking.UnitsPerMetre;
    // Separate enter/exit thresholds prevent flicker near the wrist angle.
    // The short deliberate hold prevents a passing gesture flashing the panel.
    if (!Allowed || PalmFacing<0.45) HideDisplay();
    else if (!bWristRequested)
    {
        if (PalmFacing>=0.65)
        {
            if (WristRevealTime<0) WristRevealTime=Level.TimeSeconds;
            if (Level.TimeSeconds-WristRevealTime>=0.15) bWristRequested=True;
        }
        else WristRevealTime=-1.0;
    }
    bHidden=!bWristRequested;
    DisplayedGun=Rig.Gun;
    if (DisplayedGun==None || Rig.PC.Pawn==None) { HideDisplay(); return; }
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
    else if (KFVRTrackedSyringe(DisplayedGun)!=None || KFVRTrackedWelder(DisplayedGun)!=None) AmmoLine="CHARGE " $ int(100*DisplayedGun.ChargeBar()) $ "%";
    else if (DisplayedGun.Class==class'Frag') AmmoLine="GRENADES " $ DisplayedGun.AmmoAmount(0);
    else if (bMelee) AmmoLine="MELEE";
    else AmmoLine="LOADED " $ DisplayedLoaded $ "    SPARE " $ DisplayedSpare;
    ActionLine="READY";
    if (Rig.bHolstered) ActionLine="HOLSTERED";
    else if (DisplayedGun.Class==class'Frag')
    { if (Rig.GrenadeControl!=None && Rig.GrenadeControl.bPinRemoved) ActionLine="RELEASE GRIP TO THROW"; else ActionLine="HOLD GRIP + TRIGGER TO ARM"; }
    else if (KFVRTrackedSyringe(DisplayedGun)!=None)
    { if (KFVRTrackedSyringe(DisplayedGun).bPendingHeal) ActionLine="HEALING"; else ActionLine="TOUCH BODY AND SQUEEZE"; }
    else if (KFVRTrackedWelder(DisplayedGun)!=None)
    {
        if (KFVRTrackedWelder(DisplayedGun).ContactDoor==None) ActionLine="POINT TIP AT CLOSED DOOR";
        else ActionLine="DOOR " $ int(KFVRTrackedWelder(DisplayedGun).DoorPercent) $ "%  L: UNWELD";
    }
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
    DisplayWidthMetres=0.20
    WristRevealTime=-1.0
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
