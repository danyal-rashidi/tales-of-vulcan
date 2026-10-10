#include "PlayerAnimInstance.h"
#include "PlayerEquipComponent.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
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
	const float LeanTarget = FMath::Clamp(YawRate / 260.f, -1.f, 1.f) * JogWeight * FMath::Clamp(Forwardness, 0.f, 1.f);
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
	// The eight-direction sets: the two clips either side of the direction of travel.
	auto Directional = [this](TObjectPtr<UAnimSequence>* Set, FPoseContext& Into)
	{
		const float D = FMath::Fmod(Direction + 360.f, 360.f) / 45.f;
		const int32 I0 = FMath::FloorToInt(D) % 8;
		const int32 I1 = (I0 + 1) % 8;
		const float A = D - FMath::FloorToFloat(D);
		auto At = [this](const UAnimSequence* Clip) { return Clip ? Phase * Clip->GetPlayLength() : 0.f; };
		if (A < 0.02f || !Set[I1])
		{
			SampleAt(Set[I0], At(Set[I0]), Into, true);
			return;
		}
		if (A > 0.98f || !Set[I0])
		{
			SampleAt(Set[I1], At(Set[I1]), Into, true);
			return;
		}
		FPoseContext P0(Into), P1(Into);
		SampleAt(Set[I0], At(Set[I0]), P0, true);
		SampleAt(Set[I1], At(Set[I1]), P1, true);
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
			Directional(Walk, Move);
		}
		if (JogWeight > 0.001f)
		{
			FPoseContext JogPose(Output);
			Directional(Jog, JogPose);
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
	Locomotion->Direction = Locomotion->Speed > 5.f ? FMath::FindDeltaAngleDegrees(Yaw, Velocity.Rotation().Yaw) : 0.f;
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
	IdleClip = LoadClip(IdleClip, UAL + TEXT("Idle"));
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
