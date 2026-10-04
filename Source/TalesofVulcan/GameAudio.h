#pragma once

#include "CoreMinimal.h"

class UAudioComponent;
class USceneComponent;

/**
 * Plays the game's sounds (CC0 sets imported into /Game/Audio) straight from C++: no sound cues or
 * Blueprint wiring. A name like TEXT("Roar") picks one of S_Roar_01, S_Roar_02, ... (or S_Roar) at
 * random with a slightly random pitch, so repeats don't sound identical. Range is roughly how far
 * away (cm) a sound can still be heard. Missing sounds are silently skipped.
 */
namespace GameAudio
{
	TALESOFVULCAN_API void Play(const UObject* WorldContext, const TCHAR* Name, const FVector& Location, float Volume = 1.f, float Pitch = 1.f, float Range = 4000.f);

	TALESOFVULCAN_API void Play2D(const UObject* WorldContext, const TCHAR* Name, float Volume = 1.f, float Pitch = 1.f);

	/** A looping sound (S_...Loop) following AttachTo, or at Location without one. Stop or fade the returned component to end it. */
	TALESOFVULCAN_API UAudioComponent* Loop(const UObject* WorldContext, const TCHAR* Name, USceneComponent* AttachTo, const FVector& Location, float Volume = 1.f, float Range = 2000.f);

	/** A looping sound everywhere at once (music, wind). */
	TALESOFVULCAN_API UAudioComponent* Loop2D(const UObject* WorldContext, const TCHAR* Name, float Volume = 1.f);
}
