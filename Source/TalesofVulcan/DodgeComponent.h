#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DodgeComponent.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDodgeEvent);

/**
 * Dodge for the player. Call TryDodge from the dodge input event.
 * - Spends stamina from the owner's StaminaComponent (if it has one).
 * - Dashes in the direction the player is pressing, or backsteps with no input.
 * - Makes the owner's HealthComponent invulnerable for IFrameDuration (i-frames).
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UDodgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDodgeComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
	float StaminaCost = 25.f;

	/** Dash speed in cm/s. Higher = longer, faster dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
	float DodgeSpeed = 1500.f;

	/** How long the dash lasts, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
	float DodgeDuration = 0.35f;

	/** Seconds of invulnerability from the start of the dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
	float IFrameDuration = 0.3f;

	/** Extra wait after a dodge ends before the next one is allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
	float Cooldown = 0.15f;

	/** Optional roll animation (with root motion OFF). Leave empty for a plain dash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
	TObjectPtr<UAnimMontage> DodgeMontage;

	/** Hook up a whoosh sound or dust effect here. */
	UPROPERTY(BlueprintAssignable, Category="Dodge")
	FOnDodgeEvent OnDodgeStarted;

	UPROPERTY(BlueprintAssignable, Category="Dodge")
	FOnDodgeEvent OnDodgeEnded;

	/** Returns true if the dodge happened (not on cooldown, enough stamina, on the ground). */
	UFUNCTION(BlueprintCallable, Category="Dodge")
	bool TryDodge();

	UFUNCTION(BlueprintPure, Category="Dodge")
	bool IsDodging() const { return bDodging; }

private:
	void EndIFrames();
	void EndDodge();

	bool bDodging = false;
	float NextDodgeTime = 0.f;

	float SavedGroundFriction = 0.f;
	float SavedBrakingFriction = 0.f;
	float SavedBrakingDeceleration = 0.f;

	FTimerHandle IFrameTimer;
	FTimerHandle DodgeTimer;
};
