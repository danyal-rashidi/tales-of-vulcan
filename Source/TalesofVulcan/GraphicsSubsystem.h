#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GraphicsSubsystem.generated.h"

/**
 * Picture quality at play time, on top of the dressing subsystems' looks (they own the colour grade, vignette and grain):
 *  - Lumen with hardware ray tracing where the graphics card supports it, a little sharpening;
 *  - volumetric fog, so the sun throws shafts through dust and between buildings;
 *  - sun contact shadows (small things sit on the ground) and a slightly soft shadow edge;
 *  - bloom, ambient occlusion and motion blur tuned in their own post-process volume;
 *  - a soft key light that only the player receives (lighting channel 1), so Elvis reads against Vulcan's storm.
 * Console: tov.Graphics 0 turns it off (before play starts).
 */
UCLASS()
class TALESOFVULCAN_API UGraphicsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void Apply();
	void LightThePlayer();

	FTimerHandle Timer;
	FTimerHandle PlayerTimer;
	float PlayerWait = 0.f;
};
