// Standalone world-space trader. Transactions use the installed pawn functions.
class KFVRTraderPanel extends KFVRStatusDisplay;
var ShopVolume Shop;
var KFLevelRules Rules;
var KFHumanPawn Customer;
var KFWeapon PreviousGun;
var int DualAmmoTransfers,GrenadeFills,TransferredTotal,TransferredLoaded;
var bool bOpen,bSelectArmed,bSelectPrevious,bCloseArmed,bClosePrevious,bWasHolstered;
var int HoverRow,OpenCount,CloseCount,ActionCount,BuyCount,SellCount,AmmoCount,ArmorCount,HealCount,RejectedCount;
var float CursorU,CursorV,DisplayedCash,DisplayedWeight,DisplayedMaxWeight;
struct RowState { var bool Enabled; };
var RowState StateRows[9];
var string Rows[9],HeaderLine;
var class<KFWeapon> Catalog[3];
var int LastAction;
var float CashBefore,CashAfter,LastPrice;
var int AmmoBefore,AmmoAfter;
var float LastSellValue;
simulated function bool Initialize(KFVRWeaponRig R)
{
    if (!Super.Initialize(R)) return False;
    Surface.SetSize(1024,1024); SetDrawScale3D(vect(1,1,2));
    Catalog[0]=class'Deagle'; Catalog[1]=class'Bullpup'; Catalog[2]=class'Dualies';
    HoverRow=-1; return True;
}
function bool ContainsPickup(array<class<Pickup> > Items, class<Pickup> Kind)
{
    local int I;
    for (I=0;I<Items.Length;I++) if (Items[I]==Kind) return True;
    return False;
}
function bool Listed(class<Pickup> Kind)
{
    if (Rules==None) foreach DynamicActors(class'KFLevelRules',Rules) break;
    if (Rules==None) return False;
    return ContainsPickup(Rules.MediItemForSale,Kind) || ContainsPickup(Rules.SuppItemForSale,Kind)
        || ContainsPickup(Rules.ShrpItemForSale,Kind) || ContainsPickup(Rules.CommItemForSale,Kind)
        || ContainsPickup(Rules.BersItemForSale,Kind) || ContainsPickup(Rules.FireItemForSale,Kind)
        || ContainsPickup(Rules.DemoItemForSale,Kind) || ContainsPickup(Rules.NeutItemForSale,Kind);
}
function bool Eligible()
{
    local ShopVolume S;
    if (Rig==None || Rig.PC==None || Customer==None || Rig.PC.Pawn!=Customer || Customer.Health<=0 || Shop==None
        || !Shop.bCurrentlyOpen || Shop.bAlwaysClosed || Level.NetMode!=NM_Standalone || !Customer.CanBuyNow()) return False;
    foreach Customer.TouchingActors(class'ShopVolume',S) if (S==Shop) return True;
    return False;
}
function bool OpenShop(ShopVolume S)
{
    local vector X,Y,Z;
    if (Rig==None || Rig.PC==None) return False;
    Customer=KFHumanPawn(Rig.PC.Pawn); Shop=S;
    if (!Eligible()) return False;
    PreviousGun=Rig.Gun; bWasHolstered=Rig.bHolstered; Rig.ParkForTrader();
    GetAxes(Rig.Tracking.BodyRotation,X,Y,Z);
    SetLocation(Rig.Tracking.HeadPosition+(X-Z*0.08)*Rig.Tracking.UnitsPerMetre);
    SetRotation(OrthoRotation(-X,-Y,Z)); SetDrawScale(Rig.Tracking.UnitsPerMetre*0.9/100);
    bOpen=True; bHidden=False; bSelectArmed=False; bCloseArmed=False; bSelectPrevious=False; bClosePrevious=False;
    OpenCount++; HoverRow=-1; PreviousText=""; Rig.PulseRight(0.15,15); return True;
}
function CloseShop()
{
    if (!bOpen) return;
    bOpen=False; bHidden=True; CloseCount++; HoverRow=-1;
    bSelectArmed=False; bCloseArmed=False;
    if (Rig!=None) { Rig.RestoreTraderWeapon(PreviousGun); Rig.bHolstered=bWasHolstered; Rig.bTriggerArmed=False; Rig.bUseArmed=False; Rig.bUsePrevious=True; }
}
function KFWeapon Owned(class<KFWeapon> Kind)
{
    local Inventory I;
    if (Customer==None || Kind==None) return None;
    for (I=Customer.Inventory;I!=None;I=I.Inventory)
        if (KFWeapon(I)!=None && I.PickupClass==Kind.default.PickupClass) return KFWeapon(I);
    return None;
}
function class<KFVeterancyTypes> Veteran() { return Customer.GetVeteran(); }
function float WeaponPrice(class<KFWeapon> Kind)
{
    return class<KFWeaponPickup>(Kind.default.PickupClass).default.Cost
        * Veteran().static.GetCostScaling(KFPlayerReplicationInfo(Customer.PlayerReplicationInfo),Kind.default.PickupClass);
}
function float SalePrice(KFWeapon W)
{
    if (W.SellValue!=-1) return W.SellValue;
    return int(class<KFWeaponPickup>(W.PickupClass).default.Cost*0.75)
        * Veteran().static.GetCostScaling(KFPlayerReplicationInfo(Customer.PlayerReplicationInfo),W.PickupClass);
}
function KFWeapon AmmoWeapon(int Row)
{
    if (Row==8) return Owned(class'Frag');
    if (Row==3) { if (Owned(class'Dualies')!=None) return Owned(class'Dualies'); return Owned(class'Single'); }
    return Owned(Catalog[Row-4]);
}
function float ArmorPrice()
{
    return class'Vest'.default.ItemCost*(100-Customer.ShieldStrength)/100
        *Veteran().static.GetCostScaling(KFPlayerReplicationInfo(Customer.PlayerReplicationInfo),class'Vest');
}
function float FillPrice(KFWeapon W)
{
    local float Amount,Cost;
    if (W==None || W.AmmoClass[0]==None) return 0;
    Amount=float(W.AmmoClass[0].default.MaxAmmo)*Veteran().static.AddExtraAmmoFor(KFPlayerReplicationInfo(Customer.PlayerReplicationInfo),W.AmmoClass[0]);
    Cost=class<KFWeaponPickup>(W.PickupClass).default.AmmoCost*Veteran().static.GetAmmoCostScaling(KFPlayerReplicationInfo(Customer.PlayerReplicationInfo),W.PickupClass);
    return int(FMax(0,Amount-W.AmmoAmount(0))/W.default.MagCapacity*Cost);
}
function Snapshot()
{
    local int I;
    local KFWeapon W;
    local float Price;
    local string Combined;
    DisplayedCash=Customer.PlayerReplicationInfo.Score; DisplayedWeight=Customer.CurrentWeight; DisplayedMaxWeight=Customer.MaxCarryWeight;
    HeaderLine="CASH " $ int(DisplayedCash) $ "    LOAD " $ int(DisplayedWeight) $ "/" $ int(DisplayedMaxWeight);
    for (I=0;I<3;I++)
    {
        W=Owned(Catalog[I]);
        if (W!=None)
        { Rows[I]="SELL " $ W.ItemName $ "   +" $ int(SalePrice(W)); StateRows[I].Enabled=!W.bKFNeverThrow; }
        else
        {
            Price=WeaponPrice(Catalog[I]); Rows[I]="BUY " $ Catalog[I].default.ItemName $ "   " $ int(Price);
            StateRows[I].Enabled=Listed(Catalog[I].default.PickupClass) && DisplayedCash>=Price
                && Customer.CanCarry(class<KFWeaponPickup>(Catalog[I].default.PickupClass).default.Weight);
        }
    }
    for (I=3;I<9;I++)
    {
        if (I==6 || I==7) continue;
        W=AmmoWeapon(I); Rows[I]="AMMO - WEAPON NOT OWNED"; StateRows[I].Enabled=False;
        if (W!=None)
        { Price=FillPrice(W); Rows[I]="FILL " $ W.ItemName $ " AMMO   " $ int(Price); StateRows[I].Enabled=Price>0 && DisplayedCash>0; }
    }
    Price=ArmorPrice();
    Rows[6]="ARMOR " $ int(Customer.ShieldStrength) $ "%   " $ int(Price); StateRows[6].Enabled=Customer.ShieldStrength<100 && DisplayedCash>0;
    Rows[7]="HEAL " $ Customer.Health $ "%   " $ class'FirstAidKit'.default.ItemCost;
    StateRows[7].Enabled=Customer.Health+Customer.healthToGive<100 && DisplayedCash>=class'FirstAidKit'.default.ItemCost;
    if (Customer.healthToGive>0) Rows[7]="HEALING   " $ Customer.Health $ "%";
    Combined=HeaderLine $ "|" $ HoverRow $ "|" $ int(CursorU*1024) $ "|" $ int(CursorV*1024);
    for (I=0;I<9;I++) Combined=Combined $ "|" $ Rows[I] $ StateRows[I].Enabled;
    if (Combined!=PreviousText) { PreviousText=Combined; Surface.Revision++; TextureUpdates++; }
}
// KF creates Single before destroying Dualies, so the shared ammo actor is
// destroyed underneath its replacement. Rebind through stock GiveAmmo and
// restore the measured total/right feed only after a successful owned sale.
function SellTracked(KFWeapon W)
{
    local KFVRTrackedDualies D;
    local KFWeapon Remaining;
    local int Total,Loaded;
    D=KFVRTrackedDualies(W); Total=-1;
    if (D!=None) { D.EnsureFeed(); Total=D.AmmoAmount(0); Loaded=D.RightLoaded; }
    Customer.ServerSellWeapon(W.Class);
    if (Total<0 || Owned(class'Dualies')!=None) return;
    Remaining=Owned(class'Single');
    if (Remaining==None || Remaining.Owner!=Customer) return;
    Remaining.GiveAmmo(0,None,False);
    Remaining.ClientForceAmmoUpdate(0,Min(Total,Remaining.MaxAmmo(0)));
    Remaining.MagAmmoRemaining=Min(Loaded,Min(Remaining.MagCapacity,Remaining.AmmoAmount(0)));
    TransferredTotal=Remaining.AmmoAmount(0); TransferredLoaded=Remaining.MagAmmoRemaining; DualAmmoTransfers++;
}
function ExecuteRow(int Row)
{
    local KFWeapon W;
    local int I;
    if (!Eligible() || Row<0 || Row>8) { RejectedCount++; return; }
    Snapshot();
    if (!StateRows[Row].Enabled) { RejectedCount++; return; }
    LastAction=Row; CashBefore=Customer.PlayerReplicationInfo.Score; AmmoBefore=0; AmmoAfter=0;
    if (Row<3)
    {
        W=Owned(Catalog[Row]);
        if (W!=None)
        { LastSellValue=W.SellValue; LastPrice=SalePrice(W); SellTracked(W); SellCount++; }
        else
        { LastPrice=WeaponPrice(Catalog[Row]); Customer.ServerBuyWeapon(Catalog[Row],class<KFWeaponPickup>(Catalog[Row].default.PickupClass).default.Weight); BuyCount++; }
    }
    else if (Row<6 || Row==8)
    {
        W=AmmoWeapon(Row);
        if (W!=None)
        {
            AmmoBefore=W.AmmoAmount(0); LastPrice=FillPrice(W);
            Customer.ServerBuyAmmo(W.AmmoClass[0],False); AmmoAfter=W.AmmoAmount(0); AmmoCount++;
            if (Row==8 && AmmoAfter>AmmoBefore) GrenadeFills++;
            for (I=0;I<Rig.InventoryCount;I++) if (Rig.InventoryManuals[I]!=None) Rig.InventoryManuals[I].Reconcile();
            if (KFVRTrackedDualies(W)!=None) KFVRTrackedDualies(W).EnsureFeed();
        }
    }
    else if (Row==6) { LastPrice=ArmorPrice(); Customer.ServerBuyKevlar(); ArmorCount++; }
    else if (Row==7) { LastPrice=class'FirstAidKit'.default.ItemCost; Customer.ServerBuyFirstAid(); HealCount++; }
    else CloseShop();
    CashAfter=Customer.PlayerReplicationInfo.Score; ActionCount++; Rig.PulseRight(0.22,18);
}
simulated function UpdateDisplay()
{
    local vector Front,Right,Up,Ray,Hit,NormalHit,Origin;
    local float Den,T,U,V;
    local bool Down,CloseDown;
    if (!bOpen) { bHidden=True; return; }
    if (!Eligible() || !Rig.bTracking || Level.TimeSeconds-Rig.LastInputTime>0.1) { CloseShop(); return; }
    HoverRow=-1; CursorU=-1; CursorV=-1;
    if ((Rig.Tracking.Flags & 2)==0) { bSelectArmed=False; bCloseArmed=False; Snapshot(); return; }
    GetAxes(Rotation,Front,Right,Up); Origin=Rig.Tracking.RightPosition; Ray=vector(Rig.Tracking.RightRotation);
    Den=Ray dot Front;
    if (Den<-0.01 && Customer.Trace(Hit,NormalHit,Origin,Rig.Tracking.HeadPosition,True)==None)
    {
        T=((Location-Origin) dot Front)/Den;
        if (T>0 && T<2.5*Rig.Tracking.UnitsPerMetre)
        {
            Hit=Origin+Ray*T;
            U=0.5-((Hit-Location) dot Right)/(0.9*Rig.Tracking.UnitsPerMetre);
            V=0.5-((Hit-Location) dot Up)/(0.9*Rig.Tracking.UnitsPerMetre);
            if (U>=0 && U<=1 && V>=0 && V<=1 && Customer.Trace(Origin,NormalHit,Hit,Rig.Tracking.RightPosition,True)==None)
            {
                CursorU=U; CursorV=V;
                if (U>=0.04 && U<=0.96 && V*1024>=210 && V*1024<930 && int(V*1024-210)%80<68)
                    HoverRow=int((V*1024-210)/80);
            }
        }
    }
    Snapshot();
    Down=Rig.Tracking.RightTrigger>0.75; CloseDown=(Rig.Tracking.Flags & 8)!=0 || (Rig.Tracking.Flags & 32)!=0;
    if (Rig.Tracking.RightTrigger<0.15) bSelectArmed=True;
    if (!CloseDown) bCloseArmed=True;
    if (bCloseArmed && CloseDown && !bClosePrevious) CloseShop();
    else if (bSelectArmed && Down && !bSelectPrevious && HoverRow>=0) { ExecuteRow(HoverRow); bSelectArmed=False; }
    bSelectPrevious=Down; bClosePrevious=CloseDown;
}
simulated event RenderTexture(ScriptedTexture Tex)
{
    local int I;
    local color Ink,Fill;
    if (Tex!=Surface || White==None || TextFont==None) return;
    RenderCalls++; Ink=MakeColor(235,241,246);
    Tex.DrawTile(0,0,1024,1024,0,0,1,1,White,MakeColor(12,19,26));
    Tex.DrawText(40,30,"TRADER",TextFont,MakeColor(58,212,160));
    Tex.DrawText(40,86,HeaderLine,TextFont,Ink);
    Tex.DrawText(40,146,"POINT + TRIGGER    A / B: CLOSE",TextFont,Ink);
    for (I=0;I<9;I++)
    {
        Fill=MakeColor(31,44,54); Ink=MakeColor(235,241,246);
        if (!StateRows[I].Enabled) Ink=MakeColor(118,130,140);
        if (HoverRow==I) Fill=MakeColor(28,89,75);
        Tex.DrawTile(40,210+I*80,944,68,0,0,1,1,White,Fill);
        Tex.DrawText(58,226+I*80,Rows[I],TextFont,Ink);
    }
    if (CursorU>=0) Tex.DrawTile(CursorU*1024-5,CursorV*1024-5,10,10,0,0,1,1,White,MakeColor(255,235,120));
}
function Destroyed() { CloseShop(); Super.Destroyed(); }
