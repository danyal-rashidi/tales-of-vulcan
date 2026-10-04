#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArenaDressingSubsystem.generated.h"

/**
 * Look-and-feel for the colosseum, applied when play starts (no level files change):
 * - a low, dusty fog that thickens with distance, so the arena edges and desert fade ominously
 *   (Vulcan's storm later darkens it further from these values),
 * - the centre floor slab switched from busy pebbles to the clean sand texture,
 * - tufts of dry desert grass bending in the wind around the arena.
 * Console: tov.ArenaDressing 0 turns it all off (takes effect on the next Play).
 */
UCLASS()
class TALESOFVULCAN_API UArenaDressingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void SetupFog(UWorld& World);
	void CleanFloor(UWorld& World);
	void PlantGrass(UWorld& World);
};
