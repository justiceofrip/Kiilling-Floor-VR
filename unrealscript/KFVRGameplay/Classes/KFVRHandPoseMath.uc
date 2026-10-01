// Runtime profile names, not copied mesh/animation data.
class KFVRHandPoseMath extends Object;
var name LeftBones[16],RightBones[16];
static function name SourceBone(bool Left,int Index)
{
    if (Index<0 || Index>=16) return '';
    if (Left) return default.LeftBones[Index];
    return default.RightBones[Index];
}
static function rotator BlendRotation(rotator A,rotator B,float Alpha)
{
    if (Alpha<=0) return A;
    if (Alpha>=1) return B;
    return QuatToRotator(QuatSlerp(QuatFromRotator(A),QuatFromRotator(B),Alpha));
}
defaultproperties
{
    LeftBones(0)=Hands_LArmPalm
    LeftBones(1)=Hands_LArmThumb1
    LeftBones(2)=Hands_LArmThumb2
    LeftBones(3)=Hands_LArmThumb3
    LeftBones(4)=Hands_LArmFinger11
    LeftBones(5)=Hands_LArmFinger12
    LeftBones(6)=Hands_LArmFinger13
    LeftBones(7)=Hands_LArmFinger21
    LeftBones(8)=Hands_LArmFinger22
    LeftBones(9)=Hands_LArmFinger23
    LeftBones(10)=Hands_LArmFinger32
    LeftBones(11)=Hands_LArmFinger33
    LeftBones(12)=Hands_LArmFinger34
    LeftBones(13)=Hands_LArmFinger42
    LeftBones(14)=Hands_LArmFinger43
    LeftBones(15)=Hands_LArmFinger44
    RightBones(0)=Hands_RArmPalm
    RightBones(1)=Hands_RArmThumb1
    RightBones(2)=Hands_RArmThumb2
    RightBones(3)=Hands_RArmThumb3
    RightBones(4)=Hands_RArmFinger11
    RightBones(5)=Hands_RArmFinger12
    RightBones(6)=Hands_RArmFinger13
    RightBones(7)=Hands_RArmFinger21
    RightBones(8)=Hands_RArmFinger22
    RightBones(9)=Hands_RArmFinger23
    RightBones(10)=Hands_RArmFinger32
    RightBones(11)=Hands_RArmFinger33
    RightBones(12)=Hands_RArmFinger34
    RightBones(13)=Hands_RArmFinger42
    RightBones(14)=Hands_RArmFinger43
    RightBones(15)=Hands_RArmFinger44
}
