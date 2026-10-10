#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RomeDressingSubsystem.generated.h"

/**
 * Roman-desert dressing around the colosseum, applied when play starts (no level files change):
 * - date palms (/Game/Rome/Trees, swaying in the wind through their material): tall groves just outside the
 *   colosseum whose crowns rise over the rim, and a few palms growing out of the collapsed-wall rubble inside,
 * - desert ground cover on the arena floor (/Game/Rome/Ground): esparto grass, living and dead shrubs, pebbles,
 *   and photo-scanned rocks from the Kite demo pack; the centre platform is left open for the fight,
 * - red-and-gold SPQR banners hanging from the top of the arena wall (/Game/Rome/Props), billowing in the wind,
 * - four gold eagle standards (aquilae) with little SPQR flags: a pair at the gate, a pair facing them.
 * Placed one frame after play starts, on top of ArenaDressingSubsystem's sand floor and rubble.
 * Console: tov.RomeDressing 0 turns it off (takes effect on the next Play).
 */
UCLASS()
class TALESOFVULCAN_API URomeDressingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void PlantPalms(UWorld& World);
	void DressGround(UWorld& World);
	void HangBanners(UWorld& World);
	void PlaceStandards(UWorld& World);
};
