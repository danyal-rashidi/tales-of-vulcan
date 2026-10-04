#include "DriftParticles.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Particles->SetStaticMesh(Sphere.Object);
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

	if (bWaitForStorm)
	{
		Intensity = 0.f;
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
	const int32 Visible = FMath::RoundToInt(N * FMath::Clamp(Intensity, 0.f, 1.f));
	const FVector BaseScale = ParticleSize / 100.f; // engine sphere is 100 cm across

	auto Wrap = [](double Value, double HalfSize)
	{
		const double Size = HalfSize * 2.0;
		Value = FMath::Fmod(Value + HalfSize, Size);
		return (Value < 0.0 ? Value + Size : Value) - HalfSize;
	};

	for (int32 i = 0; i < N; ++i)
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

	Particles->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}
