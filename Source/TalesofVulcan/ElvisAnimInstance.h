#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "ElvisAnimInstance.generated.h"

class UIKRetargeter;

/**
 * Runs Elvis: copies the pose of the hidden Quinn mesh he is attached to (like ABP_NightHunter does),
 * then simulates his cape. The cape is three chains of bones (Cape_L/M/R_01..04) hanging from Spine2;
 * each chain is a string of particles with gravity, a little shape memory, links to the neighbouring
 * chain, and collision with his body, legs, arms and the floor. Everything lives in C++, so there is
 * no anim graph to edit: put this class in the skin mesh component's Anim Class.
 */
USTRUCT()
struct FElvisAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FElvisAnimInstanceProxy() = default;
	FElvisAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode);

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return RetargetNode; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(RetargetNode); }

private:
	static constexpr int32 NumChains = 3;
	static constexpr int32 NumPoints = 5;   // 4 bones + the tip

	struct FCapsule
	{
		int32 BoneA = INDEX_NONE;
		int32 BoneB = INDEX_NONE;
		float Radius = 0.f;
	};

	void SetupCape(const USkeletalMeshComponent* Mesh);
	void SimulateCape(FPoseContext& Output);
	void Step(const FVector (&Targets)[NumChains][NumPoints], const TArray<FVector>& CapA, const TArray<FVector>& CapB, const TArray<float>& CapR, float Dt);
	bool ChainFrame(const FVector (&P)[NumChains][NumPoints], int32 Chain, int32 Index, const FVector& Head, FQuat& OutFrame) const;

	FAnimNode_RetargetPoseFromMesh* RetargetNode = nullptr;

	// Set up from the reference pose (mesh bone indices).
	bool bCapeReady = false;
	int32 Spine2Bone = INDEX_NONE;
	int32 HipsBone = INDEX_NONE;
	int32 CapeBones[NumChains][NumPoints - 1];
	FVector RestInSpine2[NumChains][NumPoints];
	FQuat RestFrameOffset[NumChains][NumPoints - 1];
	float LinkLength[NumChains][NumPoints - 1];
	float SideLength[NumChains - 1][NumPoints];
	TArray<FCapsule> Capsules;
	FVector FlapInHips[2];

	// Simulation state, in world space (cm).
	bool bSimulate = true;
	bool bSimStarted = false;
	FVector Pos[NumChains][NumPoints];
	FVector PrevPos[NumChains][NumPoints];
	FVector LastTargets[NumChains][NumPoints];
	TArray<FVector> LastCapA, LastCapB;
	float TimeAccumulator = 0.f;
	float PendingDeltaTime = 0.f;
	float FloorZ = -UE_BIG_NUMBER;
};

UCLASS(Transient)
class TALESOFVULCAN_API UElvisAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UElvisAnimInstance();

	/** Copies Quinn's animation onto Elvis's skeleton. */
	UPROPERTY(EditDefaultsOnly, Category="Elvis")
	TSoftObjectPtr<UIKRetargeter> Retargeter;

	/** Turn off to see the cape stiff on his back (for comparison). */
	UPROPERTY(EditDefaultsOnly, Category="Elvis|Cape")
	bool bSimulateCape = true;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeInitializeAnimation() override;

private:
	UPROPERTY(Transient)
	FAnimNode_RetargetPoseFromMesh RetargetNode;

	friend struct FElvisAnimInstanceProxy;
};
