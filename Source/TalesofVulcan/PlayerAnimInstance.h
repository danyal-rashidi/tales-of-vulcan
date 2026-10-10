#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "PlayerAnimInstance.generated.h"

class UAnimSequence;

/**
 * The player's locomotion, all in one node:
 *  - idle (relaxed, or the two-handed spear stance when armed), with an occasional look around when relaxed;
 *  - walk / jog in eight directions, blended by the direction of travel relative to facing (so strafing while
 *    locked on works), sprint, and a lean into turns while jogging;
 *  - jump start, fall and landing;
 *  - when armed, the spear stance on the upper body while the legs run;
 *  - a one-shot upper-body clip on top (drawing and sheathing the spear) so the legs keep moving.
 * Clip cadence follows ground speed so the feet don't slide.
 */
USTRUCT()
struct FAnimNode_PlayerLocomotion : public FAnimNode_Base
{
	GENERATED_BODY()

	// Directions, in order: forward, forward-right, right, back-right, back, back-left, left, forward-left.
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk[8];
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Jog[8];
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleFidget;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> ArmedIdle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Sprint;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LeanLeft;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> LeanRight;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> JumpStart;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Fall;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Land;

	/** Ground speeds (cm/s) the cycles were made for. */
	float WalkClipSpeed = 115.f;
	float JogClipSpeed = 600.f;
	float SprintClipSpeed = 700.f;

	// Set by the proxy before each update.
	float Speed = 0.f;
	float Direction = 0.f;       // degrees, velocity relative to facing; + is to the right
	float YawRate = 0.f;         // degrees per second
	float VerticalSpeed = 0.f;
	bool bInAir = false;
	bool bArmed = false;

	// The one-shot upper-body clip.
	TObjectPtr<UAnimSequence> Overlay = nullptr;
	float OverlayTime = 0.f;
	float OverlayRate = 1.f;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override {}
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override {}
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;

private:
	void BuildUpperBodyMask(const FPoseContext& Output);

	float IdleTime = 0.f;
	float StillTime = 0.f;        // how long the player has stood still, for the fidget
	float FidgetTime = -1.f;      // < 0: not fidgeting
	float Phase = 0.f;            // 0..1 through the stride, shared by walk, jog and sprint
	float MoveWeight = 0.f, JogWeight = 0.f, SprintWeight = 0.f;
	float Lean = 0.f;
	float ArmedWeight = 0.f;
	float AirWeight = 0.f, AirTime = 0.f;
	float LandWeight = 0.f, LandTime = 0.f;
	bool bWasInAir = false;
	float OverlayWeight = 0.f;
	TArray<float> UpperMask;      // per compact bone: 0 legs and hips, rising up the spine to 1 for the arms and head
};

USTRUCT()
struct FPlayerAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FPlayerAnimInstanceProxy() = default;
	FPlayerAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_Slot* InSlot, FAnimNode_PlayerLocomotion* InLocomotion);

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual FAnimNode_Base* GetCustomRootNode() override { return Slot; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(Slot); OutNodes.Add(Locomotion); }

private:
	FAnimNode_Slot* Slot = nullptr;
	FAnimNode_PlayerLocomotion* Locomotion = nullptr;
	float LastYaw = 0.f;
	bool bHasYaw = false;
};

/**
 * Runs the player's hidden mannequin (whose pose Elvis copies): the locomotion above, with the attack, dodge and
 * death montages on DefaultSlot on top. All C++, no anim graph; URomePlayerSetupSubsystem puts it on the player.
 * Clips: the universal animation library retargeted onto the mannequin (/Game/Player/UAL, import_ual.py).
 */
UCLASS(Transient)
class TALESOFVULCAN_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPlayerAnimInstance();

	/** Plays a clip once on the upper body (arms, chest, head), over whatever the legs are doing. */
	void PlayUpperBody(UAnimSequence* Sequence, float Rate = 1.f);

	/** Seconds into the upper-body clip, or -1 when none is playing. */
	float GetUpperBodyTime() const { return UpperBodyTime; }

	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> WalkClips[8];
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> JogClips[8];
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> IdleClip;
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> IdleFidgetClip;
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> ArmedIdleClip;
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> SprintClip;
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> LeanLeftClip;
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> LeanRightClip;
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> JumpStartClip;
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> FallClip;
	UPROPERTY(EditDefaultsOnly, Category="Player") TObjectPtr<UAnimSequence> LandClip;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	friend struct FPlayerAnimInstanceProxy;

	UPROPERTY(Transient)
	FAnimNode_Slot SlotNode;

	UPROPERTY(Transient)
	FAnimNode_PlayerLocomotion LocomotionNode;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> PendingUpperBody;
	float PendingUpperBodyRate = 1.f;
	float UpperBodyTime = -1.f;
	float UpperBodyLength = 0.f;
	float UpperBodyRate = 1.f;
};
