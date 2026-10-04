#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "BossHUDWidget.generated.h"

class AVulcanBoss;
class UProgressBar;

/**
 * Souls-style boss health bar along the bottom of the screen (with a pale trail that drains after
 * each hit), and a victory banner when Vulcan dies. Built in code (no widget asset needed).
 * Finds Vulcan by itself and stays hidden until he wakes from the statue.
 * Added to the screen automatically by BossHUDSubsystem.
 */
UCLASS()
class TALESOFVULCAN_API UBossHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TWeakObjectPtr<AVulcanBoss> Boss;
	float DefeatedTime = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> BossBar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Fill;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Trail;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Victory;

	/** Boss music: starts when Vulcan wakes, fades out when he dies. */
	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> Music;

	bool bVictoryToll = false;
};

/** Puts the boss HUD on the player's screen at the start of play. */
UCLASS()
class TALESOFVULCAN_API UBossHUDSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void AddToScreen();

	FTimerHandle SetupTimer;
};
