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
 * -EnvShot      films the arena (overview, ground level, grass close-up, wall) for checking fog/grass/floor.
 * -FireShot     spawns each FireFX preset next to the player and films it.
 * -BossShot     starts the fight and films Vulcan (wake-up roar, attacks, flinches from a few hits, death).
 * -LockShot     starts the fight, locks on to Vulcan and films from the player camera with UI: a roll,
 *               a jump and plunge (dust), a hit (boss bar), then Vulcan's death (victory banner). Also records
 *               the game audio to Saved/SpearShots/lock_audio.wav.
 * -RomeShot     films RomeDressingSubsystem's pieces close up (banner, eagle standard, palm, the gate end).
 * -DragonShot   stands the dragon beast (/Game/Dragon) in front of the player and films each of his animations.
 */
UCLASS()
class TALESOFVULCAN_API UGroundCheckSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** True when the game was started with one of the flags above (the main menu then goes straight to the arena). */
	static bool IsTestRun();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void Report();
	void StartSpearShot();
	void StartEnvShot();
	void StartFireShot();
	void StartBossShot();
	void StartBossProbe();
	void StartLockShot();
	void StartElvisShot();
	void StartOtterShot();
	void StartRomeShot();
	void StartDragonShot();
	void Shot(const FString& Name);
	void ShootFrom(const FVector& Location, const FVector& LookAt);
	void After(float Seconds, TFunction<void()> Action);

	FTimerHandle ReportTimer;
	TArray<FTimerHandle> ShotTimers;

	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> ShotCamera;
};
