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
 * - After HitDelay, damages every pawn inside a sphere in front of the owner
 *   with Apply Damage, so Vulcan's HealthComponent reacts automatically.
 * - Can't attack while dodging, falling or dead.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UMeleeAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeAttackComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float Damage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float StaminaCost = 15.f;

	/** Seconds from pressing attack until the hit lands. Match it to the animation later. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float HitDelay = 0.25f;

	/** Total length of the attack, including recovery, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float AttackDuration = 0.6f;

	/** How far in front of the player the hit sphere is centred. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float HitDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float HitRadius = 90.f;

	/** Walk speed while swinging. 0 = plant your feet like in Souls games. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	float MoveSpeedWhileAttacking = 0.f;

	/** Optional attack animation. Leave empty for now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Draws the hit sphere (green = hit something, red = missed). Turn off for the demo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
	bool bShowDebug = true;

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
	void DoHit();
	void EndAttack();

	bool bAttacking = false;
	float SavedMaxWalkSpeed = 0.f;
	FTimerHandle AttackTimer;
};
