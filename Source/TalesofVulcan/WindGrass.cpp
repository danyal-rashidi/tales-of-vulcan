#include "WindGrass.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace GrassLook
{
	// Sun-bleached straw, darker dry stems, and a few olive ones.
	const FLinearColor Colors[3] = { FLinearColor(0.56f, 0.48f, 0.32f), FLinearColor(0.38f, 0.31f, 0.21f), FLinearColor(0.46f, 0.44f, 0.3f) };
}

AWindGrass::AWindGrass()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	for (int32 Band = 0; Band < 3; ++Band)
	{
		UInstancedStaticMeshComponent* Mesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Blades%d"), Band));
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetStaticMesh(Cone.Object);
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
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Bands[Band] = Mesh;
	}

	if (Cone.Object)
	{
		const FBox Box = Cone.Object->GetBoundingBox();
		LocalBaseZ = Box.Min.Z;
		LocalHeight = FMath::Max(Box.Max.Z - Box.Min.Z, 1.f);
		LocalWidth = FMath::Max(Box.Max.X - Box.Min.X, 1.f);
	}
}

void AWindGrass::AddTuft(const FVector& Ground, FRandomStream& Random)
{
	const int32 Count = Random.RandRange(8, 13);
	const float TuftHeight = Random.FRandRange(45.f, 90.f);
	const int32 TuftBand = Random.RandRange(0, 2);

	for (int32 i = 0; i < Count; ++i)
	{
		FBlade Blade;
		// Blades fan out from the middle of the tuft; outer ones lean further.
		const float Azimuth = Random.FRandRange(0.f, 360.f);
		const float Lean = Random.FRandRange(4.f, 32.f);
		const FVector Out = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Azimuth, FVector::UpVector);
		Blade.Base = Ground + Out * Random.FRandRange(0.f, 6.f);
		Blade.Rest = FQuat(FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal(), FMath::DegreesToRadians(Lean))
			* FQuat(FVector::UpVector, FMath::DegreesToRadians(Random.FRandRange(0.f, 360.f)));

		const float Height = TuftHeight * Random.FRandRange(0.55f, 1.f);
		const float Width = Random.FRandRange(1.2f, 2.4f);
		Blade.Scale = FVector(Width / LocalWidth, Width / LocalWidth, Height / LocalHeight);
		Blade.Phase = Random.FRandRange(0.f, 10.f);
		Blade.Flex = Random.FRandRange(0.7f, 1.3f) * (Height / 60.f);
		Blade.Band = Random.FRand() < 0.7f ? TuftBand : Random.RandRange(0, 2);
		Blade.Index = Transforms[Blade.Band].Num();

		Transforms[Blade.Band].Add(BladeTransform(Blade, Blade.Rest));
		Blades.Add(Blade);
	}
}

void AWindGrass::FinishTufts()
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	for (int32 Band = 0; Band < 3; ++Band)
	{
		if (Base)
		{
			BandMaterials[Band] = UMaterialInstanceDynamic::Create(Base, this);
			BandMaterials[Band]->SetVectorParameterValue(TEXT("Color"), GrassLook::Colors[Band]);
			BandMaterials[Band]->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			BandMaterials[Band]->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
			BandMaterials[Band]->SetScalarParameterValue(TEXT("Glow"), 0.1f); // thin blades are mostly lit edge-on; a little glow keeps them straw-coloured
			Bands[Band]->SetMaterial(0, BandMaterials[Band]);
		}
		Bands[Band]->ClearInstances();
		Bands[Band]->AddInstances(Transforms[Band], false, true);
	}
}

FTransform AWindGrass::BladeTransform(const FBlade& Blade, const FQuat& Rotation) const
{
	// Keep the cone's base on the ground whatever way it bends.
	const FVector Location = Blade.Base - Rotation.RotateVector(FVector(0.f, 0.f, LocalBaseZ) * Blade.Scale);
	return FTransform(Rotation, Location, Blade.Scale);
}

void AWindGrass::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	FVector Wind = WindDirection;
	Wind.Z = 0.f;
	Wind = Wind.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector BendAxis = FVector::CrossProduct(FVector::UpVector, Wind);
	const float MaxBend = FMath::DegreesToRadians(WindBendDegrees);

	for (const FBlade& Blade : Blades)
	{
		// Gusts roll across the arena along the wind; each blade also flutters on its own.
		const float Along = FVector::DotProduct(Blade.Base, Wind);
		const float Gust = 0.55f + 0.45f * FMath::Sin(Time * 0.9f - Along * 0.0022f);
		const float Flutter = 0.75f + 0.25f * FMath::Sin(Time * 3.3f + Blade.Phase);
		const float Bend = MaxBend * Gust * Flutter * Blade.Flex;
		const float Twitch = 0.12f * Bend * FMath::Sin(Time * 5.1f + Blade.Phase * 2.f);

		const FQuat Rotation = FQuat(BendAxis, Bend) * FQuat(Wind, Twitch) * Blade.Rest;
		Transforms[Blade.Band][Blade.Index] = BladeTransform(Blade, Rotation);
	}

	for (int32 Band = 0; Band < 3; ++Band)
	{
		if (Transforms[Band].Num() > 0)
		{
			Bands[Band]->BatchUpdateInstancesTransforms(0, Transforms[Band], true, true, true);
		}
	}
}
