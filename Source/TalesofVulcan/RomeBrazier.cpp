#include "RomeBrazier.h"
#include "FlameFX.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

ARomeBrazier::ARomeBrazier()
{
	Footprint = CreateDefaultSubobject<UBoxComponent>(TEXT("Footprint"));
	Footprint->SetBoxExtent(FVector(40.f, 40.f, 70.f));
	Footprint->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Footprint->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Footprint;
}

void ARomeBrazier::BeginPlay()
{
	Super::BeginPlay();

	auto Load = [](const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); };
	UStaticMesh* Pedestal = Load(TEXT("/Game/ThirdPerson/Colosseum/Meshes/SM_RomanColumn_BrokenB.SM_RomanColumn_BrokenB"));
	UStaticMesh* Cylinder = Load(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = Load(TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterialInterface* Solid = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Rome/Materials/M_RomeSolid.M_RomeSolid"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Pedestal || !Cylinder || !Cone || !Solid)
	{
		return;
	}
	auto Paint = [this, Solid](const FLinearColor& Color, float Metallic, float Roughness)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Solid, this);
		M->SetVectorParameterValue(TEXT("Tint"), Color);
		M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		M->SetScalarParameterValue(TEXT("WindAmount"), 0.f);
		return M;
	};
	UMaterialInstanceDynamic* Iron = Paint(FLinearColor(0.04f, 0.036f, 0.032f), 0.85f, 0.5f);
	UMaterialInstanceDynamic* Coals = Paint(FLinearColor(1.f, 0.25f, 0.04f), 0.f, 1.f);

	// Size is the box the mesh is stretched into; Bottom is how high its base sits; Flip turns it upside down.
	const FVector Base = GetActorLocation();
	auto Part = [&](UStaticMesh* Mesh, const FVector& Size, float Bottom, UMaterialInterface* Material, bool bFlip)
	{
		const FBox Box = Mesh->GetBoundingBox();
		const FVector Scale = Size / Box.GetSize();
		const FRotator Rotation(0.f, 0.f, bFlip ? 180.f : 0.f);
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetStaticMesh(Mesh);
		Piece->SetupAttachment(RootComponent);
		Piece->SetWorldScale3D(Scale);
		Piece->SetWorldRotation(Rotation);
		Piece->SetWorldLocation(Base + FVector(0.f, 0.f, Bottom + 0.5f * Size.Z) - Rotation.RotateVector(Box.GetCenter() * Scale));
		if (Material)
		{
			Piece->SetMaterial(0, Material);
		}
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->RegisterComponent();
	};
	const float Top = bStand ? 92.f : 0.f;
	if (bStand)
	{
		Part(Pedestal, FVector(60.f, 60.f, 95.f), -3.f, nullptr, false);
	}
	Part(Cone, FVector(120.f, 120.f, 38.f), Top, Iron, true);
	Part(Cylinder, FVector(106.f, 106.f, 4.f), Top + 34.f, Coals, false);
	AFlameFX::Spawn(GetWorld(), Base + FVector(0.f, 0.f, Top + 34.f), FlameHeight, LightCandelas);
}
