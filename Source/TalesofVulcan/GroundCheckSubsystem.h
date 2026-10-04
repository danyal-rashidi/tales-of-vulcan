#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GroundCheckSubsystem.generated.h"

/**
 * Debug tools, inactive in normal play. Start the game (UnrealEditor.exe <uproject> -game) with:
 * -GroundCheck  a few seconds in, logs what every character is standing on (floor actor, gap under
 *               the capsule, lowest visible point, every collision surface below), then quits.
 * -SpearShot    previews the spear setup: adds a SpearGrip if the player has none, uses the great sword
 *               attack montages, films the player from the side (-ShotYaw=90 -ShotDist=260) during idle,
 *               light and heavy attack into Saved/SpearShots, then quits. Grip overrides for testing:
 *               -GripFront=0.4 -GripRoll=0 -GripFlip -GripSwapHands
 */
UCLASS()
class TALESOFVULCAN_API UGroundCheckSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void Report();
	void StartSpearShot();
	void Shot(const FString& Name);
	void After(float Seconds, TFunction<void()> Action);

	FTimerHandle ReportTimer;
	TArray<FTimerHandle> ShotTimers;
};
