#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeAttackComponent.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttackStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, AActor*, HitActor, FVector, HitLocation);

/**
 * Light attack for the player. Call TryAttack from the attack input event.
 * - Spends stamina from the owner's StaminaComponent (if it has one).
 * - At every "Hit" notify in AttackMontage (or after HitDelay if it has none),
 *   damages every pawn inside a sphere in front of the owner with Apply Damage,
 *   so Vulcan's HealthComponent reacts automatically.
 * - Can't attack while dodging, falling, plunging or dead.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UMeleeAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeAttackComponent();

	/** Damage dealt by each hit. A montage with two Hit notifies deals it twice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float StaminaCost = 15.f;

	/** Seconds from pressing attack until the hit lands. Only used when AttackMontage has no HitNotifyName notify. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float HitDelay = 0.25f;

	/**
	 * Notify in AttackMontage that marks a frame where the weapon connects. Add one per swing
	 * (two for a double slash). Hits land exactly there at any Rate Scale, and HitDelay is ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	FName HitNotifyName = TEXT("Hit");

	/** Total length of the attack, including recovery, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float AttackDuration = 0.6f;

	/** How far in front of the player the hit sphere is centred. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float HitDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float HitRadius = 90.f;

	/**
	 * If the owner has a SpearGripComponent, the hit follows the spear on the Hit frame
	 * (its front two thirds, HitRadius thick) instead of a sphere HitDistance in front.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	bool bHitAlongWeapon = true;

	/** Walk speed while swinging. 0 = plant your feet like in Souls games. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float MoveSpeedWhileAttacking = 0.f;

	/** Optional attack animation. Add a notify named HitNotifyName on the strike frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Draws the hit sphere (green = hit something, red = missed). Turn on while tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	bool bShowDebug = false;

	/** Hook up a swing sound here. */
	UPROPERTY(BlueprintAssignable, Category="Attack")
	FOnAttackStarted OnAttackStarted;

	/** Hook up a hit sound, sparks or camera shake here. */
	UPROPERTY(BlueprintAssignable, Category="Attack")
	FOnAttackHit OnAttackHit;

	/** Returns true if the attack started. */
	UFUNCTION(BlueprintCallable, Category="Attack")
	bool TryAttack();

	UFUNCTION(BlueprintPure, Category="Attack")
	bool IsAttacking() const { return bAttacking; }

private:
	void GatherHitTimes();
	void DoHit();
	void EndAttack();

	bool bAttacking = false;
	/** Seconds after the attack starts at which each hit lands, in order. */
	TArray<float> HitTimes;
	int32 NextHit = 0;
	float SavedMaxWalkSpeed = 0.f;
	FTimerHandle AttackTimer;
	FTimerHandle EndTimer;
};
