#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArenaDressingSubsystem.generated.h"

/**
 * Look-and-feel for the colosseum, applied when play starts (no level files change):
 * - a low, dusty fog that thickens with distance, so the arena edges and desert fade ominously
 *   (Vulcan's storm later darkens it further from these values),
 * - one clean sand floor from wall to wall, level with the old centre slab (which hides the slab's
 *   striped sides and the colosseum's tiled floor 71 cm below it),
 * - tufts of dry desert grass bending in the wind around the arena,
 * - the ramps up to the centre platform replaced by worn, broken Roman stone stairs,
 * - a ruined look: rubble heaps where the wall has collapsed, toppled broken columns, debris on the stands,
 * - every stone surface (colosseum, columns, rubble, stairs) in the same weathered stone material,
 * - a low, warm late-afternoon sun with light shafts, and six fire braziers casting flickering light,
 * - a dark-fantasy grade: weathered dark stone and ashen ground (material tints), desaturated,
 *   high-contrast, cold-shadowed colour grading with vignette and grain.
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
	void LaySandFloor(UWorld& World);
	void BuildStairs(UWorld& World);
	void BuildRuins(UWorld& World);
	void PlantGrass(UWorld& World);
	void WeatherMaterials(UWorld& World);
	void ColorGrade(UWorld& World);
	void SetupLighting(UWorld& World);
	void PlaceBraziers(UWorld& World);

	/** One darkened copy of each original material, shared by everything that used it. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UMaterialInterface>, TObjectPtr<class UMaterialInstanceDynamic>> Weathered;
};
