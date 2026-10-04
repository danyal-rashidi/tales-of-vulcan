#include "DriftParticles.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarWeatherDensity(
	TEXT("tov.WeatherDensity"),
	1.f,
	TEXT("Fraction of rain/sand particles drawn: 1 = all, 0.3 = light, 0 = weather off."),
	ECVF_Scalability);

static TAutoConsoleVariable<bool> CVarBloodRain(
	TEXT("tov.BloodRain"),
	false,
	TEXT("Blood rain during Vulcan's storm. Off by default: it made the game unplayable on some PCs. Takes effect on the next Play."),
	ECVF_Default);

ADriftParticles::ADriftParticles()
{
	PrimaryActorTick.bCanEverTick = true;

	Particles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Particles"));
	RootComponent = Particles;
	Particles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Particles->SetCastShadow(false);
	Particles->SetCanEverAffectNavigation(false);
	Particles->SetGenerateOverlapEvents(false);
	Particles->SetMobility(EComponentMobility::Movable);

	// Thousands of instances move every frame. Keep them out of Lumen, distance fields and ray tracing,
	// which would otherwise be rebuilt around every particle each frame (the main cause of the lag).
	Particles->bAffectDistanceFieldLighting = false;
	Particles->bAffectDynamicIndirectLighting = false;
	Particles->bVisibleInRayTracing = false;
	Particles->bReceivesDecals = false;

	// Streaks are a few cm thick, so a 12-triangle box looks the same as a full sphere.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	if (CubeMesh)
	{
		Particles->SetStaticMesh(CubeMesh);
	}
}

FVector ADriftParticles::GetBoxCenter() const
{
	if (bFollowCamera)
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			return Camera->GetCameraLocation();
		}
	}
	return GetActorLocation();
}

void ADriftParticles::BeginPlay()
{
	Super::BeginPlay();

	// The storm-driven weather is the blood rain, which is switched off unless tov.BloodRain is 1.
	if (bWaitForStorm && !CVarBloodRain.GetValueOnGameThread())
	{
		SetActorTickEnabled(false);
		SetActorHiddenInGame(true);
		return;
	}

	if (bWaitForStorm)
	{
		Intensity = 0.f;
	}

	// Soft (translucent) wisps fade out at their silhouette, so they keep round sides; solid streaks use the box.
	if (UStaticMesh* Mesh = CustomMaterial ? CylinderMesh.Get() : CubeMesh.Get())
	{
		Particles->SetStaticMesh(Mesh);
	}

	UMaterialInterface* Base = CustomMaterial ? CustomMaterial.Get()
		: LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Base)
	{
		Material = UMaterialInstanceDynamic::Create(Base, this);
		Material->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		Particles->SetMaterial(0, Material);
	}

	// Particles are placed in world space; the component itself stays at the origin.
	Particles->SetUsingAbsoluteLocation(true);
	Particles->SetUsingAbsoluteRotation(true);
	Particles->SetUsingAbsoluteScale(true);
	Particles->SetWorldTransform(FTransform::Identity);

	const FVector Center = GetBoxCenter();
	const int32 N = FMath::Clamp(Count, 1, 20000);
	Positions.SetNum(N);
	Phases.SetNum(N);
	Sizes.SetNum(N);
	Transforms.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		Positions[i] = Center + FVector(FMath::FRandRange(-0.5f, 0.5f) * Area.X, FMath::FRandRange(-0.5f, 0.5f) * Area.Y, FMath::FRandRange(-0.5f, 0.5f) * Area.Z);
		Phases[i] = FMath::FRandRange(0.f, 100.f);
		Sizes[i] = 1.f + FMath::FRandRange(-SizeVariation, SizeVariation);
		Transforms[i] = FTransform(FQuat::Identity, Positions[i], FVector::ZeroVector);
	}
	Particles->ClearInstances();
	Particles->AddInstances(Transforms, false, true);
}

void ADriftParticles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const int32 N = Positions.Num();
	if (N == 0)
	{
		return;
	}
	Time += DeltaSeconds;

	const float Density = FMath::Clamp(CVarWeatherDensity.GetValueOnGameThread(), 0.f, 1.f);
	const int32 Visible = FMath::RoundToInt(N * FMath::Clamp(Intensity, 0.f, 1.f) * Density);

	// Nothing to show (e.g. the rain before Vulcan's storm): hide it and skip all the per-particle work.
	if (Visible == 0)
	{
		if (Particles->IsVisible())
		{
			Particles->SetVisibility(false);
		}
		return;
	}
	if (!Particles->IsVisible())
	{
		Particles->SetVisibility(true);
	}

	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetScalarParameterValue(TEXT("Glow"), Glow);
		Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}

	const FVector Center = GetBoxCenter();
	const FVector Half = Area * 0.5f;
	const FVector Anchor = GetActorLocation();
	const bool bExclude = ExcludeEllipse.X > 0.f && ExcludeEllipse.Y > 0.f;
	const FVector BaseScale = ParticleSize / 100.f; // engine basic shapes are 100 cm across

	// Particles past the visible count already have zero scale; only touch them again when they switch off.
	const int32 UpdateCount = FMath::Min(FMath::Max(Visible, LastVisible), N);
	LastVisible = Visible;

	auto Wrap = [](double Value, double HalfSize)
	{
		const double Size = HalfSize * 2.0;
		Value = FMath::Fmod(Value + HalfSize, Size);
		return (Value < 0.0 ? Value + Size : Value) - HalfSize;
	};

	for (int32 i = 0; i < UpdateCount; ++i)
	{
		const float P = Phases[i];
		const FVector Gust(FMath::Sin(Time * 1.3f + P), FMath::Sin(Time * 1.7f + P * 1.9f), 0.4f * FMath::Sin(Time * 2.1f + P * 0.7f));
		const FVector Move = Velocity + Gust * Turbulence;
		FVector Pos = Positions[i] + Move * DeltaSeconds;

		// Keep every particle inside the box around the centre (rain follows the camera this way).
		FVector Rel = Pos - Center;
		Rel.X = Wrap(Rel.X, Half.X);
		Rel.Y = Wrap(Rel.Y, Half.Y);
		Rel.Z = Wrap(Rel.Z, Half.Z);
		Pos = Center + Rel;
		Positions[i] = Pos;

		bool bShow = i < Visible;
		if (bShow && bExclude)
		{
			const FVector Local = Pos - Anchor;
			const double E = FMath::Square(Local.X / ExcludeEllipse.X) + FMath::Square(Local.Y / ExcludeEllipse.Y);
			bShow = E > 1.0 || Local.Z > ExcludeHeight;
		}

		const FVector Dir = Move.GetSafeNormal();
		Transforms[i] = FTransform(FRotationMatrix::MakeFromZ(Dir.IsNearlyZero() ? FVector::UpVector : Dir).ToQuat(), Pos,
			bShow ? BaseScale * Sizes[i] : FVector::ZeroVector);
	}

	Particles->BatchUpdateInstancesTransforms(0, TArrayView<const FTransform>(Transforms.GetData(), UpdateCount), true, true, true);
}
