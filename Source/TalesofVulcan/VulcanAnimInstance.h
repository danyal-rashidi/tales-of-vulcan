#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "VulcanAnimInstance.generated.h"

class UAnimSequence;

/** Idle / walk / run blended by speed, or the statue pose while Vulcan is a statue. */
USTRUCT()
struct FAnimNode_VulcanLocomotion : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Walk;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Run;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Statue;

	/** Ground speeds (cm/s, at the boss's 1.6 scale) the walk and run cycles were made for: played at these, the feet don't slide. */
	float WalkSpeed = 160.f;
	float RunSpeed = 700.f;

	/** Set before each update. */
	float Speed = 0.f;
	bool bStatue = false;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override {}
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override {}
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;

private:
	void Weights(float& OutIdle, float& OutWalk, float& OutRun) const;

	float IdleTime = 0.f;
	float StridePhase = 0.f;   // 0..1 through the walk/run cycle, shared so feet line up when blending
};

USTRUCT()
struct FVulcanAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FVulcanAnimInstanceProxy() = default;
	FVulcanAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_Slot* InSlot, FAnimNode_VulcanLocomotion* InLocomotion);

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return Slot; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(Slot); OutNodes.Add(Locomotion); }

private:
	FAnimNode_Slot* Slot = nullptr;
	FAnimNode_VulcanLocomotion* Locomotion = nullptr;
};

/**
 * Runs the rigged otter Vulcan: walk/run/idle by speed with montages (attacks, roar, death) on DefaultSlot
 * on top, and the arms-folded statue pose until the statue wakes. All C++, no anim graph: it is
 * AVulcanBoss's default Anim Class when the otter model is in the project.
 */
UCLASS(Transient)
class TALESOFVULCAN_API UVulcanAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UVulcanAnimInstance();

	UPROPERTY(EditDefaultsOnly, Category="Vulcan")
	TObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Vulcan")
	TObjectPtr<UAnimSequence> WalkAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Vulcan")
	TObjectPtr<UAnimSequence> RunAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Vulcan")
	TObjectPtr<UAnimSequence> StatueAnimation;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeInitializeAnimation() override;

private:
	UPROPERTY(Transient)
	FAnimNode_Slot SlotNode;

	UPROPERTY(Transient)
	FAnimNode_VulcanLocomotion LocomotionNode;
};
