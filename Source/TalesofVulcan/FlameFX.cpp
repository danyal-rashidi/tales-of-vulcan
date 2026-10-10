#include "FlameFX.h"
#include "FireFX.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

AFlameFX::AFlameFX()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AFlameFX* AFlameFX::Spawn(UWorld* World, const FVector& Base, float Height, float LightIntensity)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFlameFX* Fire = World->SpawnActor<AFlameFX>(Base, FRotator::ZeroRotator, Params);
	if (Fire)
	{
		Fire->Build(Height, LightIntensity);
	}
	return Fire;
}

void AFlameFX::Build(float Height, float LightIntensity)
{
	UMaterialInterface* Flame = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/FX/M_Flame.M_Flame"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Flame)
	{
		return;
	}

	FRandomStream Random(GetUniqueID());
	Seed = Random.FRand() * 100.f;

	// Tongue layout: offset from the base (in fractions of Height), height and width (fractions of Height),
	// and how fast its flames rise. One tall core, two flanking flames, a ring of small licks.
	struct FTongue { FVector Offset; float Tall; float Wide; float Speed; };
	TArray<FTongue> Layout = {
		{ FVector(0.f, 0.f, 0.f), 1.f, 0.62f, 1.f },
		{ FVector(0.f, -0.16f, 0.f), 0.78f, 0.5f, 1.15f },
		{ FVector(0.f, 0.17f, 0.f), 0.72f, 0.48f, 1.1f },
		{ FVector(0.06f, 0.f, 0.f), 0.86f, 0.4f, 1.3f } };
	for (int32 i = 0; i < 4; ++i)
	{
		const float Angle = UE_TWO_PI * i / 4.f + Random.FRandRange(-0.4f, 0.4f);
		Layout.Add({ FVector(FMath::Cos(Angle) * 0.24f, FMath::Sin(Angle) * 0.24f, 0.f), float(Random.FRandRange(0.35f, 0.5f)), float(Random.FRandRange(0.26f, 0.34f)), float(Random.FRandRange(1.3f, 1.6f)) });
	}

	for (const FTongue& T : Layout)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Flame, this);
		Material->SetScalarParameterValue(TEXT("Seed"), Random.FRandRange(0.f, 100.f));
		Material->SetScalarParameterValue(TEXT("Speed"), T.Speed * Random.FRandRange(0.9f, 1.1f));

		UMaterialBillboardComponent* Tongue = NewObject<UMaterialBillboardComponent>(this);
		Tongue->SetupAttachment(RootComponent);
		// The sprite is centred on the component; its flame starts a little above the sprite's bottom edge.
		Tongue->SetRelativeLocation(T.Offset * Height + FVector(0.f, 0.f, 0.42f * T.Tall * Height));
		Tongue->AddElement(Material, nullptr, false, T.Wide * Height, T.Tall * Height, nullptr);
		Tongue->SetCastShadow(false);
		Tongue->RegisterComponent();
		Tongues.Add(Tongue);
	}

	if (LightIntensity > 0.f)
	{
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(RootComponent);
		LightRest = FVector(0.f, 0.f, 0.45f * Height);
		Light->SetRelativeLocation(LightRest);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(LightIntensity);
		Light->SetLightColor(FLinearColor(1.f, 0.42f, 0.13f));
		Light->SetAttenuationRadius(Height * 18.f);
		Light->SetSourceRadius(Height * 0.15f);
		Light->SetVolumetricScatteringIntensity(2.5f);
		Light->SetCastShadows(true);
		Light->RegisterComponent();
		BaseIntensity = LightIntensity;
	}

	// Embers drifting up out of the flames.
	FFireSettings Embers = AFireFX::EmbersPreset(Height * 0.25f, 1.0e7f);
	Embers.Rate = 14.f;
	Embers.Speed = FVector2D(90.f, 220.f);
	Embers.Lifetime = FVector2D(1.5f, 3.f);
	Embers.Size = FVector2D(2.5f, 5.f);
	Embers.Rise = 90.f;
	Embers.LightIntensity = 0.f;
	Embers.SoundVolume = 0.35f;
	AFireFX::Spawn(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, Height * 0.4f), Embers);
}

void AFlameFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	if (Light)
	{
		// Layered flicker, and the light wanders a little with the flames.
		const float T = Time + Seed;
		const float Flicker = 0.82f + 0.1f * FMath::Sin(T * 9.1f) + 0.06f * FMath::Sin(T * 23.7f + 1.3f) + 0.05f * FMath::Sin(T * 3.3f + 0.4f);
		Light->SetIntensity(BaseIntensity * Flicker);
		Light->SetRelativeLocation(LightRest + FVector(FMath::Sin(T * 5.3f), FMath::Sin(T * 4.1f + 2.f), FMath::Sin(T * 6.7f)) * 6.f);
	}
}
