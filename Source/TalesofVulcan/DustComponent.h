#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "DustComponent.generated.h"

/**
 * Sand kicked up by a character: small puffs while running, a puff on landing and on dodge rolls,
 * and a big burst where a plunge attack slams down. Sized by the character's capsule, so Vulcan
 * kicks up more than the player. Uses FireFX's dust preset (no Niagara assets).
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

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	UFUNCTION()
	void HandleDodge();

	UFUNCTION()
	void HandlePlunge(FVector ImpactLocation);

	/** Capsule radius relative to the player's. */
	float Scale = 1.f;
	float Travelled = 0.f;
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
