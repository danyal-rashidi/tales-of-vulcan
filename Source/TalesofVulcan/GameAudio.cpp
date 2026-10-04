#include "GameAudio.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	// ponytail: sounds and attenuations stay rooted for the whole session (a few MB); unload per level if memory ever matters.
	USoundBase* Pick(const TCHAR* Name)
	{
		static TMap<FString, TArray<USoundBase*>> Cache;
		TArray<USoundBase*>* Sounds = Cache.Find(Name);
		if (!Sounds)
		{
			Sounds = &Cache.Add(Name);
			auto TryLoad = [Sounds](const FString& Asset)
			{
				USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s.%s"), *Asset, *Asset), nullptr, LOAD_NoWarn | LOAD_Quiet);
				if (Sound)
				{
					Sound->AddToRoot();
					Sounds->Add(Sound);
				}
				return Sound != nullptr;
			};
			TryLoad(FString::Printf(TEXT("S_%s"), Name));
			for (int32 i = 1; TryLoad(FString::Printf(TEXT("S_%s_%02d"), Name, i)); ++i)
			{
			}
		}
		return Sounds->Num() > 0 ? (*Sounds)[FMath::RandRange(0, Sounds->Num() - 1)] : nullptr;
	}

	USoundAttenuation* Attenuation(float Range)
	{
		static TMap<int32, USoundAttenuation*> Cache;
		USoundAttenuation*& Found = Cache.FindOrAdd(FMath::RoundToInt(Range));
		if (!Found)
		{
			Found = NewObject<USoundAttenuation>(GetTransientPackage());
			Found->AddToRoot();
			FSoundAttenuationSettings& Settings = Found->Attenuation;
			Settings.bAttenuate = true;
			Settings.bSpatialize = true;
			Settings.AttenuationShape = EAttenuationShape::Sphere;
			Settings.AttenuationShapeExtents = FVector(Range * 0.15f, 0.f, 0.f); // full volume this close
			Settings.FalloffDistance = Range;
			Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		}
		return Found;
	}

	float Vary(float Pitch)
	{
		return Pitch * FMath::FRandRange(0.94f, 1.06f);
	}
}

void GameAudio::Play(const UObject* WorldContext, const TCHAR* Name, const FVector& Location, float Volume, float Pitch, float Range)
{
	if (USoundBase* Sound = Pick(Name))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, FRotator::ZeroRotator, Volume, Vary(Pitch), 0.f, Attenuation(Range));
	}
}

void GameAudio::Play2D(const UObject* WorldContext, const TCHAR* Name, float Volume, float Pitch)
{
	if (USoundBase* Sound = Pick(Name))
	{
		UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume, Vary(Pitch));
	}
}

UAudioComponent* GameAudio::Loop(const UObject* WorldContext, const TCHAR* Name, USceneComponent* AttachTo, const FVector& Location, float Volume, float Range)
{
	USoundBase* Sound = Pick(Name);
	if (!Sound)
	{
		return nullptr;
	}
	if (AttachTo)
	{
		return UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
			true, Volume, 1.f, FMath::FRandRange(0.f, 2.f), Attenuation(Range));
	}
	return UGameplayStatics::SpawnSoundAtLocation(WorldContext, Sound, Location, FRotator::ZeroRotator, Volume, 1.f, FMath::FRandRange(0.f, 2.f), Attenuation(Range));
}

UAudioComponent* GameAudio::Loop2D(const UObject* WorldContext, const TCHAR* Name, float Volume)
{
	USoundBase* Sound = Pick(Name);
	return Sound ? UGameplayStatics::SpawnSound2D(WorldContext, Sound, Volume) : nullptr;
}
