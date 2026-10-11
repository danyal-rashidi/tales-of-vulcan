#include "PlayerAnimInstance.h"
#include "PlayerEquipComponent.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"

namespace
{
	void SampleAt(const UAnimSequence* Sequence, float Time, FPoseContext& Into, bool bLoop)
	{
		if (!Sequence)
		{
			Into.ResetToRefPose();
			return;
		}
		const float Length = FMath::Max(Sequence->GetPlayLength(), 0.01f);
		const float T = bLoop ? FMath::Fmod(FMath::Fmod(Time, Length) + Length, Length) : FMath::Clamp(Time, 0.f, Length);
		FAnimationPoseData Data(Into);
		Sequence->GetAnimationPose(Data, FAnimExtractContext(double(T), false));
	}

	/** Output = A blended toward B by Alpha (0 = all A). Leaves A alone when Alpha is ~0. */
	void BlendInto(FPoseContext& A, FPoseContext& B, float Alpha, FPoseContext& Output)
	{
		FAnimationPoseData Out(Output);
		FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(A), FAnimationPoseData(B), 1.f - Alpha, Out);
	}

	float Smooth(float Current, float Target, float Dt, float Seconds)
	{
		return FMath::FInterpConstantTo(Current, Target, Dt, 1.f / FMath::Max(Seconds, 0.01f));
	}
}

// The library's moving clips (walk, jog, sprint, lean) have the body from the hips up turned ~13 degrees to his left of
// the way he runs, while the feet track straight ahead (measured: probe_facing.py, probe_slide.py) - from behind he
// looked as if his legs curved left. Turn the pelvis (and so everything above it) back, and give the thighs the
// opposite turn so the legs and feet stay on their straight track. Lean: on Elvis the spine also leaned ~6.5 degrees to
// his right while running (the clip leans ~1.6; the copy onto his skeleton adds the rest) - tilt it back by this much.
static void TurnBodyBack(FPoseContext& Pose, float Degrees, float LeanBack)
{
	const FBoneContainer& Bones = Pose.Pose.GetBoneContainer();
	auto Find = [&Bones](const TCHAR* Name)
	{
		const int32 Mesh = Bones.GetReferenceSkeleton().FindBoneIndex(FName(Name));
		return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
	};
	const FCompactPoseBoneIndex Pelvis = Find(TEXT("pelvis")), ThighL = Find(TEXT("thigh_l")), ThighR = Find(TEXT("thigh_r")), Spine = Find(TEXT("spine_01"));
	if (!Pelvis.IsValid() || !ThighL.IsValid() || !ThighR.IsValid() || !Spine.IsValid())
	{
		return;
	}
	FCSPose<FCompactPose> CS;
	CS.InitPose(Pose.Pose);
	const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(Degrees));     // + turns him to his right (Quinn faces +Y)
	FTransform Hips = CS.GetComponentSpaceTransform(Pelvis);
	const FVector Pivot = Hips.GetLocation();
	TArray<FBoneTransform> Set;
	Hips.SetRotation(Turn * Hips.GetRotation());
	Set.Add(FBoneTransform(Pelvis, Hips));
	for (const FCompactPoseBoneIndex Thigh : { ThighL, ThighR })
	{
		FTransform T = CS.GetComponentSpaceTransform(Thigh);                    // keeps its own orientation...
		T.SetLocation(Pivot + Turn.RotateVector(T.GetLocation() - Pivot));      // ...and rides round with the hips
		Set.Add(FBoneTransform(Thigh, T));
	}
	{
		// the spine rides round with the hips, then tilts toward his left about the (turned) forward axis
		FTransform T = CS.GetComponentSpaceTransform(Spine);
		const FQuat Lean(Turn.RotateVector(FVector(0.f, 1.f, 0.f)), FMath::DegreesToRadians(LeanBack));
		T.SetLocation(Pivot + Turn.RotateVector(T.GetLocation() - Pivot));
		T.SetRotation(Lean * Turn * T.GetRotation());
		Set.Add(FBoneTransform(Spine, T));
	}
	Set.Sort(FCompareBoneTransformIndex());
	CS.SafeSetCSBoneTransforms(Set);
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Pose.Pose);
}

// The same clips plant the right foot turned ~60 degrees inward in play (pigeon-toed; the left points ~10 degrees out, as
// Quinn's own clips do: probe_twist.py) - the "odd angle" of his right leg. Turn it out (tov.RightFootOut degrees).
static TAutoConsoleVariable<float> CVarRightFootOut(TEXT("tov.RightFootOut"), 47.f, TEXT("Degrees the player's right foot is turned out while moving (the library clips plant it pigeon-toed)."));
static void TurnRightFootOut(FPoseContext& Pose, float Degrees)
{
	const FBoneContainer& Bones = Pose.Pose.GetBoneContainer();
	auto Find = [&Bones](const TCHAR* Name)
	{
		const int32 Mesh = Bones.GetReferenceSkeleton().FindBoneIndex(FName(Name));
		return Mesh == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh));
	};
	const FCompactPoseBoneIndex Calf = Find(TEXT("calf_r")), Foot = Find(TEXT("foot_r"));
	if (!Calf.IsValid() || !Foot.IsValid())
	{
		return;
	}
	FCSPose<FCompactPose> CS;
	CS.InitPose(Pose.Pose);
	FTransform T = CS.GetComponentSpaceTransform(Foot);
	// About the shin, like a real ankle turn, so it holds through the whole stride: the twist is a steady ~45-50 degrees
	// against the left foot (probe_shin_twist.py). Turning about the vertical instead was right only while the foot was
	// flat - with the leg back and the toe down it swung the foot sideways. + = toe toward his right: out, for this foot.
	const FVector Shin = (CS.GetComponentSpaceTransform(Calf).GetLocation() - T.GetLocation()).GetSafeNormal();    // ankle -> knee
	T.SetRotation(FQuat(Shin, FMath::DegreesToRadians(Degrees)) * T.GetRotation());
	TArray<FBoneTransform> Set = { FBoneTransform(Foot, T) };
	CS.SafeSetCSBoneTransforms(Set);
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CS), Pose.Pose);
}

// ============================================================ node

void FAnimNode_PlayerLocomotion::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	const float Dt = Context.GetDeltaTime();
	IdleTime += Dt;

	// Idle -> walk -> jog -> sprint by ground speed; sprint only when heading forward.
	const float Forwardness = FMath::Cos(FMath::DegreesToRadians(Direction));
	MoveWeight = Smooth(MoveWeight, FMath::Clamp((Speed - 8.f) / 110.f, 0.f, 1.f), Dt, 0.12f);
	JogWeight = FMath::Clamp((Speed - 170.f) / 260.f, 0.f, 1.f);
	SprintWeight = Smooth(SprintWeight, FMath::Clamp((Speed - 640.f) / 160.f, 0.f, 1.f) * FMath::Clamp((Forwardness - 0.6f) / 0.3f, 0.f, 1.f), Dt, 0.2f);
	ArmedWeight = Smooth(ArmedWeight, bArmed ? 1.f : 0.f, Dt, 0.3f);

	// One shared stride phase, advancing at the blended cadence of the cycles at this speed.
	if (MoveWeight > 0.f)
	{
		auto Rate = [](const UAnimSequence* Clip, float Ratio) { return Clip ? FMath::Clamp(Ratio, 0.7f, 1.45f) / FMath::Max(Clip->GetPlayLength(), 0.1f) : 0.f; };
		const float WalkRate = Rate(Walk[0], Speed / WalkClipSpeed);
		const float JogRate = Rate(Jog[0], Speed / JogClipSpeed);
		const float SprintRate = Rate(Sprint, Speed / SprintClipSpeed);
		const float Cadence = FMath::Lerp(FMath::Lerp(WalkRate, JogRate, JogWeight), SprintRate, SprintWeight);
		Phase = FMath::Fmod(Phase + Dt * Cadence, 1.f);
	}

	// Lean into turns while jogging forward.
	// (at most a third of the lean clips: fully in, they swing both knees ~12 cm sideways)
	const float LeanTarget = FMath::Clamp(YawRate / 260.f, -1.f, 1.f) * 0.35f * JogWeight * FMath::Clamp(Forwardness, 0.f, 1.f);
	Lean = FMath::FInterpTo(Lean, LeanTarget, Dt, 6.f);

	// In the air, and landing.
	if (bInAir && !bWasInAir)
	{
		AirTime = 0.f;
	}
	if (!bInAir && bWasInAir && AirTime > 0.3f)
	{
		LandWeight = 1.f;
		LandTime = 0.f;
	}
	bWasInAir = bInAir;
	AirTime += bInAir ? Dt : 0.f;
	AirWeight = Smooth(AirWeight, bInAir ? 1.f : 0.f, Dt, bInAir ? 0.15f : 0.08f);
	LandTime += Dt;
	LandWeight = FMath::Max(0.f, LandWeight - Dt / (MoveWeight > 0.5f ? 0.3f : 0.7f));

	// Standing still and relaxed for a while: look around once.
	if (MoveWeight < 0.02f && !bInAir && !bArmed && Overlay == nullptr)
	{
		StillTime += Dt;
		if (FidgetTime < 0.f && StillTime > 12.f && IdleFidget)
		{
			FidgetTime = 0.f;
		}
	}
	else
	{
		StillTime = 0.f;
		FidgetTime = -1.f;
	}
	if (FidgetTime >= 0.f)
	{
		FidgetTime += Dt;
		if (!IdleFidget || FidgetTime > IdleFidget->GetPlayLength())
		{
			FidgetTime = -1.f;
			StillTime = 0.f;
		}
	}

	// The upper-body clip fades in and out at its ends.
	if (Overlay)
	{
		const float Length = Overlay->GetPlayLength();
		OverlayWeight = FMath::Clamp(FMath::Min(OverlayTime / 0.12f, (Length - OverlayTime) / 0.25f), 0.f, 1.f);
	}
	else
	{
		OverlayWeight = 0.f;
	}
}

void FAnimNode_PlayerLocomotion::BuildUpperBodyMask(const FPoseContext& Output)
{
	// spine_01 .. spine_05 take more and more of the upper-body pose; everything above inherits 1, legs and hips 0.
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
	UpperMask.SetNumZeroed(Output.Pose.GetNumBones());
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		const FString Name = Ref.GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt()).ToString();
		float Weight = 0.f;
		if (Name.StartsWith(TEXT("spine_0")))
		{
			Weight = FMath::Clamp(FCString::Atof(*Name.RightChop(7)) / 5.f, 0.f, 1.f);
		}
		else
		{
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
			Weight = Parent.IsValid() ? UpperMask[Parent.GetInt()] : 0.f;
		}
		UpperMask[Index.GetInt()] = Weight;
	}
}

void FAnimNode_PlayerLocomotion::Evaluate_AnyThread(FPoseContext& Output)
{
	if (UpperMask.Num() != Output.Pose.GetNumBones())
	{
		BuildUpperBodyMask(Output);
	}
	auto PerBone = [this](FPoseContext& Base, FPoseContext& Top, float Weight, FPoseContext& Into)
	{
		TArray<float> W;
		W.SetNumUninitialized(UpperMask.Num());
		for (int32 i = 0; i < UpperMask.Num(); ++i)
		{
			W[i] = UpperMask[i] * Weight;
		}
		FAnimationPoseData Out(Into);
		FAnimationRuntime::BlendTwoPosesTogetherPerBone(FAnimationPoseData(Base), FAnimationPoseData(Top), W, Out);
	};
	// Where in each cycle the left foot plants, relative to the forward jog's (measured from the clips: probe_phase.py),
	// so every cycle that gets blended puts the same foot down at the same moment. Without this, blending walk with
	// jog, or forward with a diagonal while turning, averaged a planted foot with a lifted one: the feet floated and
	// the knees bent across. Order: forward, forward-right, right, back-right, back, back-left, left, forward-left.
	static const float WalkSync[8] = { 0.23f, 0.10f, 0.81f, 0.81f, 0.69f, 0.69f, 0.21f, 0.10f };
	static const float JogSync[8] = { 0.f, 0.f, 0.73f, -0.02f, -0.02f, -0.02f, 0.94f, 0.f };
	// The eight-direction sets: the two clips either side of the direction of travel.
	auto Directional = [this](TObjectPtr<UAnimSequence>* Set, const float* Sync, FPoseContext& Into)
	{
		const float D = FMath::Fmod(Direction + 360.f, 360.f) / 45.f;
		const int32 I0 = FMath::FloorToInt(D) % 8;
		const int32 I1 = (I0 + 1) % 8;
		const float A = D - FMath::FloorToFloat(D);
		auto At = [this, Set, Sync](int32 I) { return Set[I] ? FMath::Fmod(Phase + Sync[I] + 1.f, 1.f) * Set[I]->GetPlayLength() : 0.f; };
		if (A < 0.02f || !Set[I1])
		{
			SampleAt(Set[I0], At(I0), Into, true);
			return;
		}
		if (A > 0.98f || !Set[I0])
		{
			SampleAt(Set[I1], At(I1), Into, true);
			return;
		}
		FPoseContext P0(Into), P1(Into);
		SampleAt(Set[I0], At(I0), P0, true);
		SampleAt(Set[I1], At(I1), P1, true);
		BlendInto(P0, P1, A, Into);
	};

	// ---- standing
	FPoseContext Stand(Output);
	{
		SampleAt(Idle, IdleTime, Stand, true);
		if (FidgetTime >= 0.f && IdleFidget)
		{
			FPoseContext Fidget(Output), Mixed(Output);
			SampleAt(IdleFidget, FidgetTime, Fidget, false);
			const float W = FMath::Clamp(FMath::Min(FidgetTime / 0.5f, (IdleFidget->GetPlayLength() - FidgetTime) / 0.6f), 0.f, 1.f);
			BlendInto(Stand, Fidget, W, Mixed);
			Stand = Mixed;
		}
		if (ArmedWeight > 0.001f && ArmedIdle)
		{
			FPoseContext Armed(Output), Mixed(Output);
			SampleAt(ArmedIdle, IdleTime, Armed, true);
			BlendInto(Stand, Armed, ArmedWeight, Mixed);
			Stand = Mixed;
		}
	}

	// ---- moving
	FPoseContext Ground(Output);
	if (MoveWeight > 0.001f)
	{
		FPoseContext Move(Output);
		if (JogWeight < 0.999f)
		{
			Directional(Walk, WalkSync, Move);
		}
		if (JogWeight > 0.001f)
		{
			FPoseContext JogPose(Output);
			Directional(Jog, JogSync, JogPose);
			if (FMath::Abs(Lean) > 0.03f && (Lean > 0.f ? LeanRight : LeanLeft))
			{
				FPoseContext LeanPose(Output), Mixed(Output);
				const UAnimSequence* Clip = Lean > 0.f ? LeanRight.Get() : LeanLeft.Get();
				SampleAt(Clip, Phase * Clip->GetPlayLength(), LeanPose, true);
				BlendInto(JogPose, LeanPose, FMath::Abs(Lean), Mixed);
				JogPose = Mixed;
			}
			if (JogWeight < 0.999f)
			{
				FPoseContext Mixed(Output);
				BlendInto(Move, JogPose, JogWeight, Mixed);
				Move = Mixed;
			}
			else
			{
				Move = JogPose;
			}
		}
		if (SprintWeight > 0.001f && Sprint)
		{
			FPoseContext SprintPose(Output), Mixed(Output);
			SampleAt(Sprint, Phase * Sprint->GetPlayLength(), SprintPose, true);
			BlendInto(Move, SprintPose, SprintWeight, Mixed);
			Move = Mixed;
		}
		// Armed: the spear stance on the upper body while the legs run (less of it at a sprint, so the arms pump).
		const float ArmedUpper = ArmedWeight * (1.f - 0.35f * SprintWeight);
		if (ArmedUpper > 0.001f && ArmedIdle)
		{
			FPoseContext Armed(Output), Mixed(Output);
			SampleAt(ArmedIdle, IdleTime, Armed, true);
			PerBone(Move, Armed, ArmedUpper, Mixed);
			Move = Mixed;
		}
		TurnBodyBack(Move, 13.f, 5.f);
		TurnRightFootOut(Move, CVarRightFootOut.GetValueOnAnyThread());
		if (MoveWeight < 0.999f)
		{
			BlendInto(Stand, Move, MoveWeight, Ground);
		}
		else
		{
			Ground = Move;
		}
	}
	else
	{
		Ground = Stand;
	}

	// ---- landing
	if (LandWeight > 0.001f && Land)
	{
		FPoseContext LandPose(Output), Mixed(Output);
		SampleAt(Land, 0.12f + LandTime, LandPose, false);
		BlendInto(Ground, LandPose, LandWeight * (1.f - 0.6f * MoveWeight), Mixed);
		Ground = Mixed;
	}

	// ---- in the air: the take-off, then falling
	FPoseContext Body(Output);
	if (AirWeight > 0.001f)
	{
		FPoseContext Air(Output);
		SampleAt(Fall, AirTime, Air, true);
		const float TakeOff = (JumpStart && VerticalSpeed > 0.f) ? FMath::Clamp(1.f - (AirTime - 0.25f) / 0.3f, 0.f, 1.f) : 0.f;
		if (TakeOff > 0.001f)
		{
			FPoseContext Start(Output), Mixed(Output);
			SampleAt(JumpStart, 0.45f + AirTime, Start, false);
			BlendInto(Air, Start, TakeOff, Mixed);
			Air = Mixed;
		}
		BlendInto(Ground, Air, AirWeight, Body);
	}
	else
	{
		Body = Ground;
	}

	// ---- the fingers: the library clips hold them in one broken pose, folded ~150 degrees back on themselves
	// (probe_hands.py; Quinn's own clips curl them 55-75 degrees, a loose fist). Take them from Quinn's idle instead,
	// except while gripping the spear (the armed pose has its own grip).
	if (Idle && ArmedWeight < 0.999f)
	{
		FPoseContext Hands(Output);
		SampleAt(Idle, IdleTime, Hands, true);
		const FBoneContainer& Bones = Body.Pose.GetBoneContainer();
		const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
		TArray<uint8> InHand;
		InHand.SetNumZeroed(Body.Pose.GetNumBones());
		const float W = 1.f - ArmedWeight;
		for (const FCompactPoseBoneIndex Index : Body.Pose.ForEachBoneIndex())
		{
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
			if (!Parent.IsValid())
			{
				continue;
			}
			const FName ParentName = Ref.GetBoneName(Bones.MakeMeshPoseIndex(Parent).GetInt());
			InHand[Index.GetInt()] = InHand[Parent.GetInt()] || ParentName == TEXT("hand_l") || ParentName == TEXT("hand_r");
			if (InHand[Index.GetInt()])
			{
				FTransform& T = Body.Pose[Index];
				T.BlendWith(Hands.Pose[Index], W);
			}
		}
	}

	// ---- the upper-body clip on top
	if (Overlay && OverlayWeight > 0.001f)
	{
		FPoseContext Top(Output);
		SampleAt(Overlay, OverlayTime, Top, false);
		PerBone(Body, Top, OverlayWeight, Output);
	}
	else
	{
		Output = Body;
	}
}

// ============================================================ proxy

FPlayerAnimInstanceProxy::FPlayerAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_Slot* InSlot, FAnimNode_PlayerLocomotion* InLocomotion)
	: FAnimInstanceProxy(InAnimInstance)
	, Slot(InSlot)
	, Locomotion(InLocomotion)
{
	// Montages (attacks, dodge, death) play on DefaultSlot over everything.
	Slot->Source.SetLinkNode(Locomotion);
}

void FPlayerAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const ACharacter* Character = Cast<ACharacter>(InAnimInstance->GetOwningActor());
	if (!Character)
	{
		return;
	}
	const FVector Velocity = Character->GetVelocity();
	const float Yaw = Character->GetActorRotation().Yaw;
	Locomotion->Speed = Velocity.Size2D();
	Locomotion->VerticalSpeed = Velocity.Z;
	// Turning to face the way he moves (not locked on), the lag between facing and travel isn't sideways movement:
	// the forward cycles only. Locked on, he strafes, and the eight directions blend.
	const UCharacterMovementComponent* Moves = Character->GetCharacterMovement();
	const bool bStrafing = Moves && !Moves->bOrientRotationToMovement;
	Locomotion->Direction = (Locomotion->Speed > 5.f && bStrafing) ? FMath::FindDeltaAngleDegrees(Yaw, Velocity.Rotation().Yaw) : 0.f;
	Locomotion->YawRate = (bHasYaw && DeltaSeconds > 0.f) ? FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / DeltaSeconds : 0.f;
	LastYaw = Yaw;
	bHasYaw = true;
	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	Locomotion->bInAir = Move && Move->IsFalling();
	const UPlayerEquipComponent* Equip = Character->FindComponentByClass<UPlayerEquipComponent>();
	Locomotion->bArmed = !Equip || Equip->IsArmed();

	const UPlayerAnimInstance* Instance = CastChecked<UPlayerAnimInstance>(InAnimInstance);
	Locomotion->Overlay = Instance->UpperBodyTime >= 0.f ? Instance->PendingUpperBody : nullptr;
	Locomotion->OverlayTime = FMath::Max(Instance->UpperBodyTime, 0.f);
}

// ============================================================ instance

UPlayerAnimInstance::UPlayerAnimInstance()
{
}

namespace
{
	UAnimSequence* LoadClip(UAnimSequence* Current, const FString& Path)
	{
		if (Current)
		{
			return Current;
		}
		const FString Name = FPaths::GetBaseFilename(Path);
		UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *Name));
		UE_CLOG(!Clip, LogTemp, Warning, TEXT("PlayerAnimInstance: missing %s"), *Path);
		return Clip;
	}
}

FAnimInstanceProxy* UPlayerAnimInstance::CreateAnimInstanceProxy()
{
	return new FPlayerAnimInstanceProxy(this, &SlotNode, &LocomotionNode);
}

void UPlayerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	// Clips not set in a subclass come from the universal animation library retargeted onto the mannequin.
	static const TCHAR* Dirs[8] = { TEXT("Forward"), TEXT("ForwardRight"), TEXT("Right"), TEXT("BackwardRight"), TEXT("Backward"), TEXT("BackwardLeft"), TEXT("Left"), TEXT("ForwardLeft") };
	const FString UAL = TEXT("/Game/Player/UAL/A_Q_");
	for (int32 i = 0; i < 8; ++i)
	{
		WalkClips[i] = LoadClip(WalkClips[i], UAL + TEXT("Walk") + Dirs[i]);
		JogClips[i] = LoadClip(JogClips[i], UAL + TEXT("Jog") + Dirs[i]);
	}
	// (Quinn's own idle: the library's stands with the left foot raised and the right knee bowed out ~10 cm.)
	IdleClip = LoadClip(IdleClip, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"));
	IdleFidgetClip = LoadClip(IdleFidgetClip, UAL + TEXT("IdleLookAround"));
	ArmedIdleClip = LoadClip(ArmedIdleClip, TEXT("/Game/Player/Animations/RTG_Great_Sword_Idle"));
	SprintClip = LoadClip(SprintClip, UAL + TEXT("Sprint"));
	LeanLeftClip = LoadClip(LeanLeftClip, UAL + TEXT("JogForwardLeanLeft"));
	LeanRightClip = LoadClip(LeanRightClip, UAL + TEXT("JogForwardLeanRight"));
	JumpStartClip = LoadClip(JumpStartClip, UAL + TEXT("JumpStart"));
	FallClip = LoadClip(FallClip, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop"));
	LandClip = LoadClip(LandClip, UAL + TEXT("JumpLand"));
	for (int32 i = 0; i < 8; ++i)
	{
		LocomotionNode.Walk[i] = WalkClips[i];
		LocomotionNode.Jog[i] = JogClips[i];
	}
	LocomotionNode.Idle = IdleClip;
	LocomotionNode.IdleFidget = IdleFidgetClip;
	LocomotionNode.ArmedIdle = ArmedIdleClip;
	LocomotionNode.Sprint = SprintClip;
	LocomotionNode.LeanLeft = LeanLeftClip;
	LocomotionNode.LeanRight = LeanRightClip;
	LocomotionNode.JumpStart = JumpStartClip;
	LocomotionNode.Fall = FallClip;
	LocomotionNode.Land = LandClip;
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (PendingUpperBody && UpperBodyTime < 0.f && UpperBodyLength <= 0.f)
	{
		// just requested
		UpperBodyTime = 0.f;
		UpperBodyLength = PendingUpperBody->GetPlayLength();
		UpperBodyRate = PendingUpperBodyRate;
	}
	else if (UpperBodyTime >= 0.f)
	{
		UpperBodyTime += DeltaSeconds * UpperBodyRate;
		if (UpperBodyTime >= UpperBodyLength)
		{
			UpperBodyTime = -1.f;
			UpperBodyLength = 0.f;
			PendingUpperBody = nullptr;
		}
	}
}

void UPlayerAnimInstance::PlayUpperBody(UAnimSequence* Sequence, float Rate)
{
	PendingUpperBody = Sequence;
	PendingUpperBodyRate = FMath::Max(Rate, 0.05f);
	UpperBodyTime = -1.f;
	UpperBodyLength = 0.f;
}
