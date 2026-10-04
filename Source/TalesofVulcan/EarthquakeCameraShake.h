#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "EarthquakeCameraShake.generated.h"

/**
 * Low, rolling ground rumble for the player's camera while Vulcan's statue comes alive.
 * Runs until stopped; the boss raises ShakeScale as the quake builds.
 */
UCLASS()
class TALESOFVULCAN_API UEarthquakeCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UEarthquakeCameraShake(const FObjectInitializer& ObjectInitializer);
};
