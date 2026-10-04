#include "EarthquakeCameraShake.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

UEarthquakeCameraShake::UEarthquakeCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;

	UPerlinNoiseCameraShakePattern* Pattern = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("EarthquakePattern"));
	Pattern->Duration = 0.f;      // until the boss stops it
	Pattern->BlendInTime = 0.3f;
	Pattern->BlendOutTime = 0.8f;

	// Ground heave: mostly up/down and side to side, a few cm at full strength.
	Pattern->X.Amplitude = 2.5f;
	Pattern->X.Frequency = 9.f;
	Pattern->Y.Amplitude = 3.5f;
	Pattern->Y.Frequency = 11.f;
	Pattern->Z.Amplitude = 6.f;
	Pattern->Z.Frequency = 13.f;

	// The view rocks and rolls a little, like standing on moving ground.
	Pattern->Pitch.Amplitude = 0.9f;
	Pattern->Pitch.Frequency = 10.f;
	Pattern->Yaw.Amplitude = 0.6f;
	Pattern->Yaw.Frequency = 8.f;
	Pattern->Roll.Amplitude = 1.4f;
	Pattern->Roll.Frequency = 7.f;
	Pattern->FOV.Amplitude = 0.f;
	Pattern->FOV.Frequency = 0.f;

	SetRootShakePattern(Pattern);
}
