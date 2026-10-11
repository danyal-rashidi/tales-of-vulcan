#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "DustComponent.generated.h"

/**
 * Sand kicked up by a character: small puffs while running, a puff on landing and on dodge rolls,
 * and a big burst where a plunge attack slams down. Sized by the character's capsule, so Vulcan
 * kicks up more than the player. Uses FireFX's dust preset (no Niagara assets).
 * Also the footstep sounds, at any pace: the player's steps sound like what's underfoot (S_Step_Sand on open
 * desert, S_Step_Stone on the town's paving and stone floors, S_Step_Wood on boards, S_Step_Gravel on rocks and
 * steep rocky ground); Vulcan's are S_BossStep.
 * Added to every character automatically by DustSetupSubsystem.
 */
UCLASS(ClassGroup=(Effects), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UDustComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDustComponent();

	/** Ground covered between footstep puffs (cm, for a player-sized character). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dust")
	float StrideLength = 150.f;

	/** No footstep puffs below this speed (cm/s), so walking stays clean. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dust")
	float MinSpeed = 300.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void Puff(const FVector& Location, float Radius, int32 Count) const;
	FVector GetFeet() const;
	void PlayStep(float Pace) const;
	/** Which footstep set fits the ground under the feet. */
	const TCHAR* StepSoundUnderfoot() const;

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	UFUNCTION()
	void HandleDodge();

	UFUNCTION()
	void HandlePlunge(FVector ImpactLocation);

	/** Capsule radius relative to the player's. */
	float Scale = 1.f;
	float Travelled = 0.f;
	float StepTravelled = 0.f;
	bool bLeftFoot = false;
};

/** Gives every character a DustComponent at the start of play. */
UCLASS()
class TALESOFVULCAN_API UDustSetupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void AddToCharacters();

	FTimerHandle SetupTimer;
};
