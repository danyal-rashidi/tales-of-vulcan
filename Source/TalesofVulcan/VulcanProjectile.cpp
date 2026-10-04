#include "VulcanProjectile.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AVulcanProjectile::AVulcanProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 5.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(20.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = Collision;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.45f));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	// Flattened dark blobs sitting on the core's surface (core radius = 50 in its local space).
	const FVector CrustDirections[] =
	{
		FVector(1.f, 0.f, 0.3f), FVector(-0.6f, 0.8f, 0.f), FVector(-0.5f, -0.7f, 0.5f),
		FVector(0.2f, 0.3f, -1.f), FVector(0.3f, -0.4f, 0.9f), FVector(-0.9f, 0.f, -0.5f)
	};
	for (int32 i = 0; i < UE_ARRAY_COUNT(CrustDirections); ++i)
	{
		const FVector Dir = CrustDirections[i].GetSafeNormal();
		UStaticMeshComponent* Patch = CreateDefaultSubobject<UStaticMeshComponent>(FName(*FString::Printf(TEXT("Crust%d"), i)));
		Patch->SetupAttachment(Mesh);
		Patch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Patch->SetCastShadow(false);
		Patch->SetRelativeLocation(Dir * 44.f);
		Patch->SetRelativeRotation(FRotationMatrix::MakeFromZ(Dir).Rotator());
		Patch->SetRelativeScale3D(FVector(0.55f, 0.45f, 0.2f));
		if (SphereMesh.Succeeded())
		{
			Patch->SetStaticMesh(SphereMesh.Object);
		}
		CrustPatches.Add(Patch);
	}

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(25.f);
	Glow->SetLightColor(FLinearColor(1.f, 0.35f, 0.05f));
	Glow->SetAttenuationRadius(350.f);
	Glow->SetCastShadows(false);

	Spin = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spin"));
	Spin->RotationRate = FRotator(360.f, 220.f, 0.f);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 1400.f;
	Movement->MaxSpeed = 1400.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
}

void AVulcanProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Don't collide with Vulcan on the way out of its mouth.
	if (APawn* Shooter = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
	}

	Movement->OnProjectileStop.AddDynamic(this, &AVulcanProjectile::HandleStop);

	// Spin only the visuals; the collision root keeps flying straight.
	Spin->SetUpdatedComponent(Mesh);

	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Lava = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Lava->SetVectorParameterValue(TEXT("Color"), LavaColor);
		Lava->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		Lava->SetScalarParameterValue(TEXT("Roughness"), 0.5f);
		Lava->SetScalarParameterValue(TEXT("Glow"), 5.f);
		Mesh->SetMaterial(0, Lava);

		UMaterialInstanceDynamic* Crust = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Crust->SetVectorParameterValue(TEXT("Color"), CrustColor);
		Crust->SetScalarParameterValue(TEXT("Metallic"), 0.3f);
		Crust->SetScalarParameterValue(TEXT("Roughness"), 0.15f);
		Crust->SetScalarParameterValue(TEXT("Glow"), 0.f);
		for (UStaticMeshComponent* Patch : CrustPatches)
		{
			Patch->SetMaterial(0, Crust);
		}
	}
}

void AVulcanProjectile::HandleStop(const FHitResult& ImpactResult)
{
	AActor* HitActor = ImpactResult.GetActor();
	if (HitActor && HitActor != GetInstigator())
	{
		UGameplayStatics::ApplyDamage(HitActor, Damage, GetInstigatorController(), this, UDamageType::StaticClass());
	}

	OnImpact(ImpactResult.ImpactPoint);
	Destroy();
}
