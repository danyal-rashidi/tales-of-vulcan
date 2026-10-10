#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameHUDWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

/**
 * The player's screen, built in code (no widget asset), in the style of the boss bar:
 *  - top left: health (with a pale trail that drains after a hit) and stamina in thin bronze frames;
 *  - bottom left: the weapon slot, the spear lit when in hand and dimmed when on the back, with its key;
 *  - the name of the part of the Rome map the player walks into, fading in and out under the middle of the screen;
 *  - a dark red edge that pulses at low health and flashes on a hit; YOU DIED;
 *  - the controls, for the first moments of play.
 * Replaces the old WBP_PlayerHUD (hidden while this is up). Added by UPlayerSetupSubsystem.
 */
UCLASS()
class TALESOFVULCAN_API UGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FString AreaAt(const FVector& Location) const;

	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthFill;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthTrail;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> StaminaFill;
	UPROPERTY(Transient) TObjectPtr<UImage> SpearIcon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SpearState;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SpearHint;
	UPROPERTY(Transient) TObjectPtr<UWidget> AreaBanner;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AreaText;
	UPROPERTY(Transient) TObjectPtr<UImage> Vignette;
	UPROPERTY(Transient) TObjectPtr<UWidget> DiedBand;
	UPROPERTY(Transient) TObjectPtr<UWidget> Controls;

	float Time = 0.f;
	float LastHealth = -1.f;
	float HitFlash = 0.f;
	float DeadTime = -1.f;
	bool bOpenMap = false;
	FString Area;
	FString PendingArea;
	float PendingTime = 0.f;
	float BannerTime = -1.f;
};
