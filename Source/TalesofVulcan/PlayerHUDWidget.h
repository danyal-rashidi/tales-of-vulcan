#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * Parent class for WBP_PlayerHUD. In the widget designer, add:
 *  - a Progress Bar named exactly HealthBar
 *  - a Progress Bar named exactly StaminaBar
 *  - (optional) a Text named exactly YouDiedText
 * The widget finds the player's Health, Stamina and PlayerDeath components and
 * updates the bars and the "YOU DIED" text by itself.
 */
UCLASS()
class TALESOFVULCAN_API UPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Connects the HUD to a character. Call it with Self right after Create Widget. */
	UFUNCTION(BlueprintCallable, Category="HUD")
	void SetupForPawn(APawn* Pawn);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> YouDiedText;

private:
	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth);

	UFUNCTION()
	void HandleStaminaChanged(float NewStamina, float MaxStamina);

	UFUNCTION()
	void HandlePlayerDied();

	UFUNCTION()
	void HandleHealthDeath(AActor* Killer);

	void Unbind();

	TWeakObjectPtr<APawn> BoundPawn;
};
