#include "SlamCameraShake.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

USlamCameraShake::USlamCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;

	UPerlinNoiseCameraShakePattern* Pattern = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("SlamPattern"));
	Pattern->Duration = 0.35f;
	Pattern->BlendInTime = 0.f;     // hits instantly
	Pattern->BlendOutTime = 0.25f;

	// Mostly a vertical thump.
	Pattern->X.Amplitude = 2.f;
	Pattern->X.Frequency = 20.f;
	Pattern->Y.Amplitude = 3.f;
	Pattern->Y.Frequency = 22.f;
	Pattern->Z.Amplitude = 10.f;
	Pattern->Z.Frequency = 28.f;

	// A little kick in the view, strongest in pitch.
	Pattern->Pitch.Amplitude = 2.f;
	Pattern->Pitch.Frequency = 24.f;
	Pattern->Yaw.Amplitude = 0.5f;
	Pattern->Yaw.Frequency = 18.f;
	Pattern->Roll.Amplitude = 1.5f;
	Pattern->Roll.Frequency = 20.f;
	Pattern->FOV.Amplitude = 0.f;
	Pattern->FOV.Frequency = 0.f;

	SetRootShakePattern(Pattern);
}
