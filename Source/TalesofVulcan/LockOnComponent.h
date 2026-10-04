#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "LockOnComponent.generated.h"

/**
 * Elden Ring style lock-on for the player. Middle mouse (or Q) locks onto the enemy nearest the
 * centre of the screen: the camera tracks it, a dot marks it, and the character keeps facing it
 * (strafing and rolling still follow the movement input). Pressing again, the target dying, getting
 * too far away or the target staying hidden too long (Vulcan diving) releases it. With nothing to
 * lock onto, the camera recentres behind the player.
 * Added to the player automatically by LockOnSetupSubsystem if the Blueprint doesn't have one.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock On")
	FKey LockKey = EKeys::MiddleMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock On")
	FKey AltLockKey = EKeys::Q;

	/** Furthest target you can lock onto (cm). The lock breaks at 25% beyond this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock On")
	float MaxDistance = 3500.f;

	/** How quickly the camera swings onto the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock On")
	float CameraTurnSpeed = 8.f;

	/** The camera looks this many degrees below the target, so the player stays in view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock On")
	float PitchOffset = -10.f;

	/** Aims this many degrees to the side of the target, so the player doesn't block the view of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock On")
	float YawOffset = 6.f;

	/** Aim point above the target's centre, as a fraction of its capsule half height (chest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock On")
	float TargetHeight = 0.35f;

	/** Seconds the target may stay hidden (e.g. diving) before the lock breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock On")
	float HiddenGrace = 6.f;

	/** Locks onto the best target, or releases the current one. Returns true if now locked. */
	UFUNCTION(BlueprintCallable, Category="Lock On")
	bool ToggleLock();

	UFUNCTION(BlueprintCallable, Category="Lock On")
	void ReleaseLock();

	UFUNCTION(BlueprintPure, Category="Lock On")
	bool IsLocked() const { return Target.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Lock On")
	AActor* GetTarget() const { return Target.Get(); }

	/** Where the camera and attacks aim on the target. */
	UFUNCTION(BlueprintPure, Category="Lock On")
	FVector GetTargetPoint() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	AActor* FindBestTarget() const;
	void SetFacingTarget(bool bFacing);
	class APlayerController* GetPlayerController() const;

	TWeakObjectPtr<AActor> Target;
	float HiddenTime = 0.f;
	bool bSavedOrientToMovement = true;
	bool bSavedUseControllerDesiredRotation = false;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Marker;
};

/** The white lock-on dot, built in code (no widget asset needed). */
UCLASS()
class TALESOFVULCAN_API ULockOnMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
};

/** Gives the player character a LockOnComponent at the start of play if it doesn't have one. */
UCLASS()
class TALESOFVULCAN_API ULockOnSetupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void AddToPlayer();

	FTimerHandle SetupTimer;
};
