#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerDeathComponent.generated.h"

class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

/**
 * What happens when the player's HealthComponent reaches 0:
 * stops any attack/roll animation, plays DeathMontage, disables the player's controls,
 * fires OnPlayerDied (show the "YOU DIED" screen there), then restarts the level.
 *
 * Tip: untick "Enable Auto Blend Out" on the death montage so the body stays on the ground.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UPlayerDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerDeathComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** Seconds after dying before the level restarts. 0 = never restart automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death")
	float RestartDelay = 4.f;

	/** Show the "YOU DIED" screen here. */
	UPROPERTY(BlueprintAssignable, Category="Death")
	FOnPlayerDied OnPlayerDied;

	/** Reloads the current level. Also handy for a "Try Again" button. */
	UFUNCTION(BlueprintCallable, Category="Death")
	void RestartLevel();

	UFUNCTION(BlueprintPure, Category="Death")
	bool IsPlayerDead() const { return bDead; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	bool bDead = false;
	FTimerHandle RestartTimer;
};
