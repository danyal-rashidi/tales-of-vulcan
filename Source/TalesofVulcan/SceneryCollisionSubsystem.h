#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SceneryCollisionSubsystem.generated.h"

/**
 * Turns off collision on background scenery that should never block anyone.
 * SM_Dunes is a Nanite mesh with "complex as simple" collision, so its collision uses the
 * simplified Nanite fallback mesh. That simplification bridges the dip under the colosseum
 * and left an invisible sheet 2-3 m above the arena floor that the player and boss stood on.
 * The player can't leave the arena (perimeter walls), so the dunes don't need collision at all.
 */
UCLASS()
class TALESOFVULCAN_API USceneryCollisionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
