#include "FireFX.h"
#include "GameAudio.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AFireFX::AFireFX()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (int32 Band = 0; Band < BandCount; ++Band)
	{
		UInstancedStaticMeshComponent* Mesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Flames%d"), Band));
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetStaticMesh(Sphere.Object);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		// Moving every frame: keep out of Lumen, distance fields and ray tracing (see DriftParticles).
		Mesh->bAffectDistanceFieldLighting = false;
		Mesh->bAffectDynamicIndirectLighting = false;
		Mesh->bVisibleInRayTracing = false;
		Mesh->bReceivesDecals = false;
		// Flames are placed in world space.
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Bands[Band] = Mesh;
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetUsingAbsoluteLocation(true);
	Light->SetCastShadows(false);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.f);
	Light->SetLightColor(FLinearColor(1.f, 0.42f, 0.12f));
}

AFireFX* AFireFX::Spawn(UWorld* World, const FVector& Location, const FFireSettings& InSettings, USceneComponent* InFollow)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFireFX* Fire = World->SpawnActor<AFireFX>(Location, FRotator::ZeroRotator, Params);
	if (Fire)
	{
		Fire->Start(InSettings, InFollow);
	}
	return Fire;
}

FFireSettings AFireFX::BreathPreset(float Range, float HalfAngleDegrees, float Duration)
{
	FFireSettings S;
	S.Rate = 240.f;
	S.Duration = Duration;
	S.SpreadDegrees = HalfAngleDegrees * 0.8f;
	S.Lifetime = FVector2D(0.32f, 0.5f);
	// Reach roughly Range by the end of a flame's life.
	S.Speed = FVector2D(Range * 2.1f, Range * 2.8f);
	S.Size = FVector2D(14.f, 28.f);
	S.GrowTo = 3.2f;
	S.Rise = 180.f;
	S.Drag = 0.7f;
	S.Stretch = 2.2f;
	S.Mix = FVector(0.35f, 0.45f, 0.2f);
	S.LightIntensity = 120.f;
	S.SoundVolume = 2.5f;
	S.SoundRange = 3500.f;
	S.LightRadius = Range * 1.6f;
	return S;
}

FFireSettings AFireFX::BurstPreset(float Radius, int32 Count)
{
	FFireSettings S;
	S.Burst = Count;
	S.SpawnRadius = Radius * 0.6f;
	S.SpreadDegrees = 35.f;
	S.Speed = FVector2D(250.f, 650.f);
	S.Outward = 300.f;
	S.Lifetime = FVector2D(0.5f, 1.0f);
	S.Size = FVector2D(30.f, 65.f);
	S.GrowTo = 1.5f;
	S.Rise = 250.f;
	S.Drag = 1.8f;
	S.Stretch = 1.6f;
	S.Mix = FVector(0.2f, 0.5f, 0.3f);
	S.LightIntensity = 250.f;
	S.LightRadius = FMath::Max(Radius * 4.f, 900.f);
	S.SoundVolume = 2.5f;
	S.SoundRange = 4000.f;
	return S;
}

FFireSettings AFireFX::EmbersPreset(float Radius, float Duration)
{
	FFireSettings S;
	S.Rate = 45.f;
	S.Duration = Duration;
	S.SpawnRadius = Radius;
	S.SpreadDegrees = 15.f;
	S.Speed = FVector2D(60.f, 160.f);
	S.Lifetime = FVector2D(0.8f, 1.6f);
	S.Size = FVector2D(6.f, 12.f);
	S.GrowTo = 0.8f;
	S.Rise = 120.f;
	S.Drag = 0.5f;
	S.Stretch = 2.5f;
	S.Mix = FVector(0.3f, 0.4f, 0.3f);
	S.LightIntensity = 30.f;
	S.LightRadius = Radius * 2.f;
	S.SoundVolume = 0.8f;
	return S;
}

FFireSettings AFireFX::GroundFirePreset(float Radius, float Duration)
{
	FFireSettings S;
	S.Rate = 80.f;
	S.Duration = Duration;
	S.SpawnRadius = Radius * 0.9f;
	S.SpreadDegrees = 12.f;
	S.Speed = FVector2D(80.f, 220.f);
	S.Lifetime = FVector2D(0.5f, 1.1f);
	S.Size = FVector2D(25.f, 55.f);
	S.GrowTo = 1.3f;
	S.Rise = 300.f;
	S.Drag = 1.2f;
	S.Stretch = 1.7f;
	S.Mix = FVector(0.25f, 0.5f, 0.25f);
	S.LightIntensity = 90.f;
	S.LightRadius = Radius * 2.f;
	S.SoundVolume = 1.5f;
	return S;
}

FFireSettings AFireFX::TorchPreset(float Radius)
{
	FFireSettings S;
	S.Rate = 45.f;
	S.Duration = 1.0e7f; // burns for the whole game
	S.SpawnRadius = Radius;
	S.SpreadDegrees = 10.f;
	S.Speed = FVector2D(60.f, 140.f);
	S.Lifetime = FVector2D(0.4f, 0.8f);
	S.Size = FVector2D(14.f, 28.f);
	S.GrowTo = 0.6f;
	S.Rise = 260.f;
	S.Drag = 1.f;
	S.Stretch = 1.5f;
	S.Mix = FVector(0.3f, 0.5f, 0.2f);
	S.LightIntensity = 160.f;
	S.LightRadius = 1600.f;
	S.SoundVolume = 0.7f;
	S.SoundRange = 1500.f;
	return S;
}

FFireSettings AFireFX::DustPreset(float Radius, int32 Count)
{
	FFireSettings S;
	S.Burst = Count;
	S.SpawnRadius = Radius * 0.25f;
	S.SpreadDegrees = 75.f;
	S.Speed = FVector2D(Radius * 0.3f, Radius * 0.8f);
	S.Outward = Radius * 1.5f;
	S.Lifetime = FVector2D(0.7f, 1.4f);
	S.Size = FVector2D(Radius * 0.15f, Radius * 0.3f);
	S.GrowTo = 3.f;
	S.Rise = 30.f;
	S.Drag = 2.5f;
	S.Stretch = 1.f;
	S.Flicker = 0.f;
	S.Mix = FVector(0.4f, 0.4f, 0.2f);
	// Same sand as the blowing desert sand (Scripts/Colosseum/build_roman_world.py).
	S.Colors[0] = FLinearColor(0.66f, 0.51f, 0.33f);
	S.Colors[1] = FLinearColor(0.55f, 0.43f, 0.29f);
	S.Colors[2] = FLinearColor(0.74f, 0.62f, 0.45f);
	// Unlit, so kept dim to sit in both the golden-hour and the storm light.
	S.Glow[0] = S.Glow[1] = S.Glow[2] = 0.22f;
	S.Opacity = 0.3f;
	return S;
}

void AFireFX::Start(const FFireSettings& InSettings, USceneComponent* InFollow)
{
	Settings = InSettings;
	Follow = InFollow;

	const TCHAR* MaterialPath = Settings.Opacity > 0.f ? TEXT("/Game/ThirdPerson/Colosseum/M_SandWisp.M_SandWisp") : TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape");
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		for (int32 Band = 0; Band < BandCount; ++Band)
		{
			BandMaterials[Band] = UMaterialInstanceDynamic::Create(Base, this);
			BandMaterials[Band]->SetVectorParameterValue(TEXT("Color"), Settings.Colors[Band]);
			BandMaterials[Band]->SetScalarParameterValue(TEXT("Glow"), Settings.Glow[Band]);
			BandMaterials[Band]->SetScalarParameterValue(TEXT("Opacity"), Settings.Opacity);
			BandMaterials[Band]->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			BandMaterials[Band]->SetScalarParameterValue(TEXT("Roughness"), 1.f);
			Bands[Band]->SetMaterial(0, BandMaterials[Band]);
		}
	}

	// Enough slots for everything alive at once; each colour band gets its share.
	const int32 MaxFlames = FMath::Clamp(Settings.Burst + FMath::CeilToInt(Settings.Rate * Settings.Lifetime.Y) + 8, 8, 600);
	const float MixTotal = FMath::Max(Settings.Mix.X + Settings.Mix.Y + Settings.Mix.Z, KINDA_SMALL_NUMBER);
	const FTransform Hidden(FQuat::Identity, GetActorLocation(), FVector::ZeroVector);
	for (int32 Band = 0; Band < BandCount; ++Band)
	{
		const int32 Slots = FMath::Max(1, FMath::RoundToInt(MaxFlames * Settings.Mix[Band] / MixTotal));
		Flames[Band].SetNum(Slots);
		Transforms[Band].Init(Hidden, Slots);
		Bands[Band]->ClearInstances();
		Bands[Band]->AddInstances(Transforms[Band], false, true);
	}

	Light->SetAttenuationRadius(Settings.LightRadius);
	Light->SetVisibility(Settings.LightIntensity > 0.f);
	if (Settings.SoundVolume > 0.f)
	{
		// Stays where the effect started (stops with this actor); a breath barely moves while it lasts.
		Audio = GameAudio::Loop(this, TEXT("FireLoop"), RootComponent, GetActorLocation(), Settings.SoundVolume, Settings.SoundRange);
	}
	Light->SetWorldLocation(EmitterLocation());

	for (int32 i = 0; i < Settings.Burst; ++i)
	{
		SpawnFlame();
	}
	bEmitting = Settings.Rate > 0.f && Settings.Duration > 0.f;
}

void AFireFX::Stop()
{
	bEmitting = false;
}

FVector AFireFX::EmitterLocation() const
{
	const USceneComponent* FollowComponent = Follow.Get();
	return FollowComponent ? FollowComponent->GetComponentLocation() : GetActorLocation();
}

FVector AFireFX::EmitterDirection() const
{
	const USceneComponent* FollowComponent = Follow.Get();
	if (const AActor* FollowOwner = FollowComponent ? FollowComponent->GetOwner() : nullptr)
	{
		FRotator Aim = FollowOwner->GetActorRotation();
		Aim.Pitch += Settings.FollowPitchDegrees;
		return Aim.Vector();
	}
	return Settings.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
}

void AFireFX::SpawnFlame()
{
	// Pick a colour band by the mix, falling back to any band with a free slot.
	const float MixTotal = FMath::Max(Settings.Mix.X + Settings.Mix.Y + Settings.Mix.Z, KINDA_SMALL_NUMBER);
	float Pick = FMath::FRand() * MixTotal;
	int32 Band = 0;
	while (Band < BandCount - 1 && Pick > Settings.Mix[Band])
	{
		Pick -= Settings.Mix[Band];
		++Band;
	}

	FFlame* Flame = nullptr;
	for (int32 Try = 0; Try < BandCount && !Flame; ++Try)
	{
		TArray<FFlame>& Slots = Flames[(Band + Try) % BandCount];
		for (FFlame& Slot : Slots)
		{
			if (!Slot.bAlive)
			{
				Flame = &Slot;
				break;
			}
		}
	}
	if (!Flame)
	{
		return;
	}

	FVector Offset = FVector::ZeroVector;
	if (Settings.SpawnRadius > 0.f)
	{
		const float Angle = FMath::FRandRange(0.f, UE_TWO_PI);
		const float Distance = Settings.SpawnRadius * FMath::Sqrt(FMath::FRand());
		Offset = FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.f);
	}

	const FVector Direction = FMath::VRandCone(EmitterDirection(), FMath::DegreesToRadians(Settings.SpreadDegrees));
	Flame->Position = EmitterLocation() + Offset;
	Flame->Velocity = Direction * FMath::FRandRange(Settings.Speed.X, Settings.Speed.Y)
		+ Offset.GetSafeNormal() * Settings.Outward * FMath::FRandRange(0.5f, 1.f);
	Flame->Age = 0.f;
	Flame->Life = FMath::FRandRange(Settings.Lifetime.X, Settings.Lifetime.Y);
	Flame->Size = FMath::FRandRange(Settings.Size.X, Settings.Size.Y);
	Flame->Phase = FMath::FRandRange(0.f, 10.f);
	Flame->bAlive = true;
}

void AFireFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	if (bEmitting)
	{
		// A breath stops when its source is gone or hidden (Vulcan diving, dying).
		const USceneComponent* FollowComponent = Follow.Get();
		if (Follow.IsStale() || (FollowComponent && FollowComponent->GetOwner() && FollowComponent->GetOwner()->IsHidden()))
		{
			bEmitting = false;
		}
		else
		{
			EmitTime += DeltaSeconds;
			SpawnBudget += Settings.Rate * DeltaSeconds;
			while (SpawnBudget >= 1.f)
			{
				SpawnFlame();
				SpawnBudget -= 1.f;
			}
			bEmitting = EmitTime < Settings.Duration;
		}
	}

	int32 Alive = 0;
	FVector Sum = FVector::ZeroVector;
	const float Damping = FMath::Max(0.f, 1.f - Settings.Drag * DeltaSeconds);

	for (int32 Band = 0; Band < BandCount; ++Band)
	{
		TArray<FFlame>& Slots = Flames[Band];
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			FFlame& Flame = Slots[i];
			FTransform& Out = Transforms[Band][i];
			if (Flame.bAlive)
			{
				Flame.Age += DeltaSeconds;
				Flame.bAlive = Flame.Age < Flame.Life;
			}
			if (!Flame.bAlive)
			{
				Out.SetScale3D(FVector::ZeroVector);
				continue;
			}

			Flame.Velocity.Z += Settings.Rise * DeltaSeconds;
			Flame.Velocity *= Damping;
			Flame.Position += Flame.Velocity * DeltaSeconds;

			// Pop in fast, widen as it travels, burn out at the end, with a flicker.
			const float T = Flame.Age / Flame.Life;
			const float Envelope = FMath::Min(T * 6.f, 1.f) * (1.f - T * T);
			const float Flicker = 1.f + Settings.Flicker * FMath::Sin(Time * 27.f + Flame.Phase * 5.f);
			const float Size = Flame.Size * FMath::Lerp(1.f, Settings.GrowTo, T) * Envelope * Flicker;

			const float Speed = Flame.Velocity.Size();
			const FVector Direction = Speed > 1.f ? Flame.Velocity / Speed : FVector::UpVector;
			const float Length = 1.f + (Settings.Stretch - 1.f) * FMath::Clamp(Speed / 400.f, 0.f, 1.f);

			// Engine sphere is 100 cm across.
			Out = FTransform(FRotationMatrix::MakeFromZ(Direction).ToQuat(), Flame.Position, FVector(Size, Size, Size * Length) / 100.f);

			Sum += Flame.Position;
			++Alive;
		}

		Bands[Band]->BatchUpdateInstancesTransforms(0, Transforms[Band], true, true, true);
	}

	if (Settings.LightIntensity > 0.f)
	{
		const int32 Capacity = Flames[0].Num() + Flames[1].Num() + Flames[2].Num();
		const float Target = Capacity > 0 ? FMath::Clamp(Alive / (0.5f * Capacity), 0.f, 1.f) : 0.f;
		LightLevel = FMath::FInterpTo(LightLevel, Target, DeltaSeconds, 12.f);
		const float Flicker = 0.8f + 0.12f * FMath::Sin(Time * 31.f) + 0.08f * FMath::Sin(Time * 17.f + 1.3f);
		Light->SetIntensity(Settings.LightIntensity * LightLevel * Flicker);
		if (Alive > 0)
		{
			Light->SetWorldLocation(Sum / Alive);
		}
	}

	if (!bEmitting && Audio)
	{
		Audio->FadeOut(0.6f, 0.f);
		Audio = nullptr;
	}

	if (!bEmitting && Alive == 0)
	{
		Destroy();
	}
}
