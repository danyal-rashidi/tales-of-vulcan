#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "SlamCameraShake.generated.h"

/**
 * Short, hard jolt for the player's camera when the plunging attack hits the ground.
 * PlungeAttackComponent uses it by default as its Landing Camera Shake.
 */
UCLASS()
class TALESOFVULCAN_API USlamCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	USlamCameraShake(const FObjectInitializer& ObjectInitializer);
};
