#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, NewStamina, float, MaxStamina);

/**
 * Stamina for the player (or anything else that dodges/attacks).
 * Before an action, call TryUseStamina(Cost). If it returns true, do the action.
 * Stamina refills automatically after RegenDelay seconds of not spending any.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStaminaComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stamina", meta=(ClampMin="1"))
	float MaxStamina = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stamina")
	float Stamina = 0.f;

	/** Stamina regained per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stamina")
	float RegenPerSecond = 30.f;

	/** Seconds after spending stamina before it starts refilling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stamina")
	float RegenDelay = 1.0f;

	/**
	 * Soulslike rule. True: you can act as long as you have ANY stamina (the bar can hit 0).
	 * False: you need the full cost to act.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stamina")
	bool bAllowActionWhenLow = true;

	/** Fires whenever stamina changes. Use it to update the stamina bar. */
	UPROPERTY(BlueprintAssignable, Category="Stamina")
	FOnStaminaChanged OnStaminaChanged;

	/** Call before a dodge or attack. Returns true and spends stamina if the action is allowed. */
	UFUNCTION(BlueprintCallable, Category="Stamina")
	bool TryUseStamina(float Cost);

	UFUNCTION(BlueprintPure, Category="Stamina")
	bool CanAfford(float Cost) const;

	/** 0..1, handy for progress bars. */
	UFUNCTION(BlueprintPure, Category="Stamina")
	float GetStaminaPercent() const { return MaxStamina > 0.f ? Stamina / MaxStamina : 0.f; }

	UFUNCTION(BlueprintCallable, Category="Stamina")
	void RefillStamina();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	float TimeSinceSpent = 0.f;

	void SetStamina(float NewValue);
};
