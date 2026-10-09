#include "VulcanAnimInstance.h"
#include "VulcanBoss.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "UObject/ConstructorHelpers.h"

void FAnimNode_VulcanLocomotion::Weights(float& OutIdle, float& OutWalk, float& OutRun) const
{
	// Idle -> walk up to WalkSpeed, walk -> run up to RunSpeed.
	const float ToWalk = FMath::Clamp((Speed - 10.f) / FMath::Max(WalkSpeed - 10.f, 1.f), 0.f, 1.f);
	const float ToRun = FMath::Clamp((Speed - WalkSpeed) / FMath::Max(RunSpeed - WalkSpeed, 1.f), 0.f, 1.f);
	OutIdle = 1.f - ToWalk;
	OutWalk = ToWalk * (1.f - ToRun);
	OutRun = ToWalk * ToRun;
}

void FAnimNode_VulcanLocomotion::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	const float Dt = Context.GetDeltaTime();
	IdleTime += Dt;

	float WIdle, WWalk, WRun;
	Weights(WIdle, WWalk, WRun);
	const float Moving = WWalk + WRun;
	if (Moving > 0.f && Walk && Run)
	{
		// Each cycle's natural cadence at this speed, blended; the shared phase keeps both on the same step.
		const float WalkRate = FMath::Clamp(Speed / WalkSpeed, 0.6f, 1.6f) / FMath::Max(Walk->GetPlayLength(), 0.1f);
		const float RunRate = FMath::Clamp(Speed / RunSpeed, 0.6f, 1.6f) / FMath::Max(Run->GetPlayLength(), 0.1f);
		StridePhase = FMath::Fmod(StridePhase + Dt * (WalkRate * WWalk + RunRate * WRun) / Moving, 1.f);
	}
}

void FAnimNode_VulcanLocomotion::Evaluate_AnyThread(FPoseContext& Output)
{
	auto Sample = [](const UAnimSequence* Sequence, float Time, FPoseContext& Into)
	{
		if (!Sequence)
		{
			Into.ResetToRefPose();
			return;
		}
		FAnimationPoseData Data(Into);
		Sequence->GetAnimationPose(Data, FAnimExtractContext(double(FMath::Fmod(Time, FMath::Max(Sequence->GetPlayLength(), 0.01f))), false));
	};
	auto Blend = [&Output](FPoseContext& A, FPoseContext& B, float WeightOfA)
	{
		FAnimationPoseData Out(Output);
		FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(A), FAnimationPoseData(B), WeightOfA, Out);
	};

	if (bStatue)
	{
		Sample(Statue ? Statue.Get() : Idle.Get(), 0.f, Output);
		return;
	}

	float WIdle, WWalk, WRun;
	Weights(WIdle, WWalk, WRun);
	const float WalkTime = StridePhase * (Walk ? Walk->GetPlayLength() : 0.f);
	const float RunTime = StridePhase * (Run ? Run->GetPlayLength() : 0.f);

	if (WIdle >= 0.999f)
	{
		Sample(Idle, IdleTime, Output);
	}
	else if (WIdle > 0.001f)
	{
		FPoseContext A(Output), B(Output);
		Sample(Idle, IdleTime, A);
		Sample(Walk, WalkTime, B);
		Blend(A, B, WIdle);
	}
	else if (WRun <= 0.001f)
	{
		Sample(Walk, WalkTime, Output);
	}
	else if (WWalk <= 0.001f)
	{
		Sample(Run, RunTime, Output);
	}
	else
	{
		FPoseContext A(Output), B(Output);
		Sample(Walk, WalkTime, A);
		Sample(Run, RunTime, B);
		Blend(A, B, WWalk / (WWalk + WRun));
	}
}

FVulcanAnimInstanceProxy::FVulcanAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_Slot* InSlot, FAnimNode_VulcanLocomotion* InLocomotion)
	: FAnimInstanceProxy(InAnimInstance)
	, Slot(InSlot)
	, Locomotion(InLocomotion)
{
	// Montages play on DefaultSlot over the locomotion.
	Slot->Source.SetLinkNode(Locomotion);
}

void FVulcanAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const AActor* Owner = InAnimInstance->GetOwningActor();
	Locomotion->Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;
	const AVulcanBoss* Boss = Cast<AVulcanBoss>(Owner);
	Locomotion->bStatue = Boss && Boss->IsStatue();
}

UVulcanAnimInstance::UVulcanAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Vulcan/Otter/Animations/A_VulcanOtter_Idle.A_VulcanOtter_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Vulcan/Otter/Animations/A_VulcanOtter_Walk.A_VulcanOtter_Walk"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Run(TEXT("/Game/Vulcan/Otter/Animations/A_VulcanOtter_Run.A_VulcanOtter_Run"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Statue(TEXT("/Game/Vulcan/Otter/Animations/A_VulcanOtter_Statue.A_VulcanOtter_Statue"));
	IdleAnimation = Idle.Object;
	WalkAnimation = Walk.Object;
	RunAnimation = Run.Object;
	StatueAnimation = Statue.Object;
}

FAnimInstanceProxy* UVulcanAnimInstance::CreateAnimInstanceProxy()
{
	return new FVulcanAnimInstanceProxy(this, &SlotNode, &LocomotionNode);
}

void UVulcanAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UVulcanAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	LocomotionNode.Idle = IdleAnimation;
	LocomotionNode.Walk = WalkAnimation;
	LocomotionNode.Run = RunAnimation;
	LocomotionNode.Statue = StatueAnimation;
}
