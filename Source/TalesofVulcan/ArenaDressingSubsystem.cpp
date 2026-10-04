#include "ArenaDressingSubsystem.h"
#include "WindGrass.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"

static TAutoConsoleVariable<bool> CVarArenaDressing(
	TEXT("tov.ArenaDressing"),
	true,
	TEXT("Fog, clean centre floor and wind-blown grass in the colosseum. Takes effect on the next Play."),
	ECVF_Default);

namespace ArenaLayout
{
	// From the colosseum scripts (perimeter_columns.py): arena centre and inner wall semi-axes, in cm.
	const FVector2D Center(-44.f, -5.f);
	const FVector2D WallRadii(3294.f, 2612.f);

	const FName FloorMesh(TEXT("SM_Template_Map_Floor"));
	const FName ColosseumMesh(TEXT("colosseum"));
	const FName DunesMesh(TEXT("SM_Dunes"));

	FName MeshName(const UPrimitiveComponent* Component)
	{
		const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
		const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
		return Mesh ? Mesh->GetFName() : NAME_None;
	}

	/** Open arena floor straight below Point (not a column, platform or other prop). */
	bool FindGround(UWorld& World, const FVector2D& Point, FVector& OutGround)
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaGround), true);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic); // the centre floor slab is BlockAllDynamic
		World.LineTraceMultiByObjectType(Hits, FVector(Point, 1500.f), FVector(Point, -800.f), Objects, Params);
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });

		for (const FHitResult& Hit : Hits)
		{
			const FName Mesh = MeshName(Hit.GetComponent());
			if (Mesh == DunesMesh)
			{
				continue; // background scenery, no collision in play (SceneryCollisionSubsystem)
			}
			if ((Mesh == FloorMesh || Mesh == ColosseumMesh) && Hit.ImpactNormal.Z > 0.85f)
			{
				OutGround = Hit.ImpactPoint;
				return true;
			}
			return false;
		}
		return false;
	}
}

void UArenaDressingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld() || !CVarArenaDressing.GetValueOnGameThread())
	{
		return;
	}

	// Only in the colosseum level.
	bool bArena = false;
	for (TActorIterator<AStaticMeshActor> It(&InWorld); It && !bArena; ++It)
	{
		bArena = ArenaLayout::MeshName(It->GetStaticMeshComponent()) == ArenaLayout::ColosseumMesh;
	}
	if (!bArena)
	{
		return;
	}

	SetupFog(InWorld);
	CleanFloor(InWorld);
	PlantGrass(InWorld);
}

void UArenaDressingSubsystem::SetupFog(UWorld& World)
{
	for (TActorIterator<AExponentialHeightFog> It(&World); It; ++It)
	{
		UExponentialHeightFogComponent* Fog = It->GetComponent();
		if (!Fog)
		{
			continue;
		}

		// Vulcan's storm thins the main fog layer to show its clouds, so the ground haze goes in the
		// second layer, which the storm leaves alone. The fog actor sits far below the arena; the
		// second layer is lifted to the arena floor and falls off quickly above it.
		float Density = 0.25f;
		float Falloff = 0.6f;
		float Start = 1200.f;
		const TCHAR* CommandLine = FCommandLine::Get();
		FParse::Value(CommandLine, TEXT("ArenaFogDensity="), Density);
		FParse::Value(CommandLine, TEXT("ArenaFogFalloff="), Falloff);
		FParse::Value(CommandLine, TEXT("ArenaFogStart="), Start);

		Fog->SetSecondFogDensity(Density);
		Fog->SetSecondFogHeightFalloff(Falloff);
		Fog->SetSecondFogHeightOffset(-Fog->GetComponentLocation().Z);
		// Starts a little way out, so the fight stays clear while the walls and desert fade.
		Fog->SetStartDistance(Start);
		Fog->SetFogMaxOpacity(0.95f);
		// Dusty brown; the storm later turns it dark red from here.
		Fog->SetFogInscatteringColor(FLinearColor(0.3f, 0.24f, 0.19f));
		return;
	}
}

void UArenaDressingSubsystem::CleanFloor(UWorld& World)
{
	// The centre slab uses M_ArenaGround with busy pebble textures; give it the plain sand set.
	static const TCHAR* Suffixes[] = { TEXT("D"), TEXT("N"), TEXT("R") };
	UTexture* Sand[3] = {};
	for (int32 i = 0; i < 3; ++i)
	{
		const FString Path = FString::Printf(TEXT("/Game/ThirdPerson/Colosseum/Textures/T_Sand_%s.T_Sand_%s"), Suffixes[i], Suffixes[i]);
		Sand[i] = LoadObject<UTexture>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!Sand[0])
	{
		return;
	}

	for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
	{
		UStaticMeshComponent* Component = It->GetStaticMeshComponent();
		if (ArenaLayout::MeshName(Component) != ArenaLayout::FloorMesh)
		{
			continue;
		}

		if (UMaterialInstanceDynamic* Material = Component->CreateDynamicMaterialInstance(0))
		{
			for (int32 i = 0; i < 3; ++i)
			{
				if (Sand[i])
				{
					Material->SetTextureParameterValue(*FString::Printf(TEXT("RockySand_%s"), Suffixes[i]), Sand[i]);
				}
			}
		}
	}
}

void UArenaDressingSubsystem::PlantGrass(UWorld& World)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWindGrass* Grass = World.SpawnActor<AWindGrass>(FVector(ArenaLayout::Center, 0.f), FRotator::ZeroRotator, Params);
	if (!Grass)
	{
		return;
	}

	FRandomStream Random(1717);
	const int32 TargetTufts = 700;
	int32 Tufts = 0;
	for (int32 Try = 0; Try < TargetTufts * 4 && Tufts < TargetTufts; ++Try)
	{
		// Clumps of grass, thicker toward the walls, none in the middle where the fight is.
		const float Angle = Random.FRandRange(0.f, UE_TWO_PI);
		const float Radius = FMath::Lerp(0.42f, 0.93f, FMath::Sqrt(Random.FRand()));
		const FVector2D ClumpCenter = ArenaLayout::Center
			+ FVector2D(FMath::Cos(Angle) * ArenaLayout::WallRadii.X, FMath::Sin(Angle) * ArenaLayout::WallRadii.Y) * Radius;

		const int32 ClumpSize = Random.RandRange(1, 3);
		for (int32 i = 0; i < ClumpSize; ++i)
		{
			const FVector2D Point = ClumpCenter + FVector2D(Random.FRandRange(-60.f, 60.f), Random.FRandRange(-60.f, 60.f));
			FVector Ground;
			if (ArenaLayout::FindGround(World, Point, Ground))
			{
				Grass->AddTuft(Ground, Random);
				++Tufts;
			}
		}
	}

	Grass->FinishTufts();
	UE_LOG(LogTemp, Log, TEXT("ArenaDressing: planted %d grass tufts (%d blades)"), Tufts, Grass->NumBlades());
}
