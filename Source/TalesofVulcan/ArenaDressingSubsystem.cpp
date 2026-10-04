#include "ArenaDressingSubsystem.h"
#include "WindGrass.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/Material.h"
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

	/** Topmost solid surface below Point (floor, stands, platforms, rubble), ignoring the background dunes. */
	bool FindSurface(UWorld& World, const FVector2D& Point, FVector& OutHit)
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaSurface), true);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		World.LineTraceMultiByObjectType(Hits, FVector(Point, 3000.f), FVector(Point, -800.f), Objects, Params);
		Hits.Sort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });

		for (const FHitResult& Hit : Hits)
		{
			if (MeshName(Hit.GetComponent()) != DunesMesh)
			{
				OutHit = Hit.ImpactPoint;
				return true;
			}
		}
		return false;
	}

	/** Point on the ellipse of the inner arena wall (Scale 1), or inside/outside it. */
	FVector2D OnWall(float Degrees, float Scale)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		return Center + FVector2D(FMath::Cos(Radians) * WallRadii.X, FMath::Sin(Radians) * WallRadii.Y) * Scale;
	}

	/** The arena gate opening is at +Y (90 degrees); keep it clear. */
	bool NearGate(float Degrees)
	{
		return FMath::Abs(FMath::FindDeltaAngleDegrees(Degrees, 90.f)) < 14.f;
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
	BuildRuins(InWorld);
	PlantGrass(InWorld);
	WeatherMaterials(InWorld);
	ColorGrade(InWorld);
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

void UArenaDressingSubsystem::BuildRuins(UWorld& World)
{
	UStaticMesh* Blocks[3] = {
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ThirdPerson/Colosseum/World/SM_RomanBlock_A.SM_RomanBlock_A"), nullptr, LOAD_NoWarn | LOAD_Quiet),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ThirdPerson/Colosseum/World/SM_RomanBlock_B.SM_RomanBlock_B"), nullptr, LOAD_NoWarn | LOAD_Quiet),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ThirdPerson/Colosseum/World/SM_RomanBlock_C.SM_RomanBlock_C"), nullptr, LOAD_NoWarn | LOAD_Quiet) };
	UStaticMesh* BrokenColumns[2] = {
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ThirdPerson/Colosseum/Meshes/SM_RomanColumn_BrokenA.SM_RomanColumn_BrokenA"), nullptr, LOAD_NoWarn | LOAD_Quiet),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ThirdPerson/Colosseum/Meshes/SM_RomanColumn_BrokenB.SM_RomanColumn_BrokenB"), nullptr, LOAD_NoWarn | LOAD_Quiet) };
	if (!Blocks[0] || !Blocks[1] || !Blocks[2])
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Origin(ArenaLayout::Center, 0.f);
	AActor* Ruins = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(Origin), SpawnParams);
	if (!Ruins)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Ruins, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	Ruins->SetRootComponent(Root);
	Root->SetWorldLocation(Origin);
	Root->RegisterComponent();

	FRandomStream Random(4242);
	int32 Pieces = 0;
	auto RandomRotation = [&Random]() { return FRotator(Random.FRandRange(-40.f, 40.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-40.f, 40.f)); };

	// Drops Mesh (scaled so its longest side is Size cm) onto whatever is below Point, its centre CenterHeight
	// cm above it. Solid pieces block the player, and later pieces pile up on top of them.
	auto Place = [&](UStaticMesh* Mesh, const FVector2D& Point, float Size, const FRotator& Rotation, float CenterHeight, bool bSolid)
	{
		FVector Surface;
		if (!Mesh || !ArenaLayout::FindSurface(World, Point, Surface))
		{
			return false;
		}
		const FBox Box = Mesh->GetBoundingBox();
		const float Scale = Size / FMath::Max(Box.GetSize().GetMax(), 1.f);
		const FVector Center = Surface + FVector(0.f, 0.f, CenterHeight);
		const FVector Location = Center - Rotation.RotateVector(Box.GetCenter() * Scale);

		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Ruins);
		Piece->SetMobility(EComponentMobility::Static);
		Piece->SetStaticMesh(Mesh);
		Piece->SetupAttachment(Root);
		Piece->SetRelativeLocationAndRotation(Location - Origin, Rotation);
		Piece->SetRelativeScale3D(FVector(Scale));
		Piece->SetCanEverAffectNavigation(false);
		if (bSolid)
		{
			Piece->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Piece->RegisterComponent();
		++Pieces;
		return true;
	};

	// Four places where the wall has collapsed into the arena: huge slabs leaning out of it and a heap
	// of blocks spilling inward, biggest at the wall.
	for (const float Angle : { 28.f, 152.f, 214.f, 327.f })
	{
		const FVector2D Wall = ArenaLayout::OnWall(Angle, 0.99f);
		const FVector2D Inward = (ArenaLayout::Center - Wall).GetSafeNormal();
		const FVector2D Along(-Inward.Y, Inward.X);
		const float FacingYaw = FMath::RadiansToDegrees(FMath::Atan2(Inward.Y, Inward.X));

		for (int32 i = 0; i < 3; ++i)
		{
			const FVector2D Point = Wall + Inward * Random.FRandRange(120.f, 260.f) + Along * Random.FRandRange(-320.f, 320.f);
			const float Size = Random.FRandRange(280.f, 400.f);
			Place(Blocks[Random.RandRange(0, 2)], Point, Size,
				FRotator(Random.FRandRange(-60.f, -35.f), FacingYaw + Random.FRandRange(-25.f, 25.f), Random.FRandRange(-15.f, 15.f)),
				Size * 0.3f, true);
		}
		for (int32 i = 0; i < 36; ++i)
		{
			const float Out = Random.FRand() * Random.FRand(); // most pieces close to the wall
			const FVector2D Point = Wall + Inward * (60.f + Out * 650.f) + Along * Random.FRandRange(-1.f, 1.f) * (440.f - Out * 220.f);
			const float Size = FMath::Lerp(200.f, 45.f, Out) * Random.FRandRange(0.7f, 1.2f);
			Place(Blocks[Random.RandRange(0, 2)], Point, Size, RandomRotation(), Size * 0.25f, Size > 70.f);
		}
	}

	// Toppled column drums lying on the arena floor.
	for (int32 Try = 0, Placed = 0; Try < 60 && Placed < 7; ++Try)
	{
		const float Angle = Random.FRandRange(0.f, 360.f);
		UStaticMesh* Column = BrokenColumns[Random.RandRange(0, 1)];
		if (ArenaLayout::NearGate(Angle) || !Column)
		{
			continue;
		}
		const FBox Box = Column->GetBoundingBox();
		const float Radius = 0.5f * FMath::Max(Box.GetSize().X, Box.GetSize().Y);
		const FRotator Lying(90.f + Random.FRandRange(-6.f, 6.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(0.f, 360.f));
		if (Place(Column, ArenaLayout::OnWall(Angle, Random.FRandRange(0.6f, 0.86f)), Box.GetSize().GetMax(), Lying, Radius * 0.85f, true))
		{
			++Placed;
		}
	}

	// Chunks fallen from the stands, and smaller debris across the outer arena floor.
	for (int32 i = 0; i < 110; ++i)
	{
		const float Angle = Random.FRandRange(0.f, 360.f);
		if (!ArenaLayout::NearGate(Angle))
		{
			const float Size = Random.FRandRange(30.f, 120.f);
			Place(Blocks[Random.RandRange(0, 2)], ArenaLayout::OnWall(Angle, Random.FRandRange(1.06f, 1.45f)), Size, RandomRotation(), Size * 0.25f, false);
		}
	}
	for (int32 i = 0; i < 130; ++i)
	{
		const float Angle = Random.FRandRange(0.f, 360.f);
		const float Size = Random.FRandRange(14.f, 45.f);
		Place(Blocks[Random.RandRange(0, 2)], ArenaLayout::OnWall(Angle, Random.FRandRange(0.55f, 0.97f)), Size, RandomRotation(), Size * 0.2f, false);
	}

	UE_LOG(LogTemp, Log, TEXT("ArenaDressing: placed %d pieces of rubble"), Pieces);
}

void UArenaDressingSubsystem::WeatherMaterials(UWorld& World)
{
	// Weathered, grimy stone and ashen ground instead of bright sun-baked sandstone and sand.
	struct FLook
	{
		const TCHAR* BaseMaterial;
		FLinearColor Tint;
	};
	static const FLook Looks[] = {
		{ TEXT("M_ColosseumSandstone"), FLinearColor(0.5f, 0.47f, 0.43f) },
		{ TEXT("M_RomanColumn"), FLinearColor(0.54f, 0.5f, 0.46f) },
		{ TEXT("M_ArenaGround"), FLinearColor(0.44f, 0.4f, 0.36f) },
		{ TEXT("M_Dunes"), FLinearColor(0.4f, 0.36f, 0.3f) },
	};

	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* Material = Component->GetMaterial(Slot);
				const UMaterial* Base = Material ? Material->GetBaseMaterial() : nullptr;
				const FLook* Look = nullptr;
				for (const FLook& Candidate : Looks)
				{
					if (Base && Base->GetName() == Candidate.BaseMaterial)
					{
						Look = &Candidate;
					}
				}
				if (!Look)
				{
					continue;
				}

				UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
				if (!Dynamic)
				{
					TObjectPtr<UMaterialInstanceDynamic>& Shared = Weathered.FindOrAdd(Material);
					if (!Shared)
					{
						Shared = UMaterialInstanceDynamic::Create(Material, this);
					}
					Dynamic = Shared;
					Component->SetMaterial(Slot, Dynamic);
				}

				Dynamic->SetVectorParameterValue(TEXT("Tint"), Look->Tint);
				if (Base->GetName() == TEXT("M_ColosseumSandstone"))
				{
					// Less sand drifted onto ledges, deeper cracks.
					Dynamic->SetScalarParameterValue(TEXT("SandOnFloors"), 0.4f);
					Dynamic->SetScalarParameterValue(TEXT("NormalStrength"), 1.6f);
				}
			}
		}
	}
}

void UArenaDressingSubsystem::ColorGrade(UWorld& World)
{
	APostProcessVolume* Volume = nullptr;
	for (TActorIterator<APostProcessVolume> It(&World); It && !Volume; ++It)
	{
		Volume = It->bUnbound ? *It : nullptr;
	}
	if (!Volume)
	{
		Volume = World.SpawnActor<APostProcessVolume>();
		if (!Volume)
		{
			return;
		}
		Volume->bUnbound = true;
	}

	// Dark-fantasy grade: drained colour, hard contrast, cold shadows, darker overall, vignette and grain.
	FPostProcessSettings& Settings = Volume->Settings;
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(0.68f, 0.68f, 0.68f, 1.f);
	Settings.bOverride_ColorContrast = true;
	Settings.ColorContrast = FVector4(1.2f, 1.2f, 1.2f, 1.f);
	Settings.bOverride_ColorGain = true;
	Settings.ColorGain = FVector4(0.97f, 0.98f, 1.02f, 1.f);
	Settings.bOverride_ColorOffsetShadows = true;
	Settings.ColorOffsetShadows = FVector4(-0.004f, 0.f, 0.008f, 0.f);
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = -0.15f;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.65f;
	Settings.bOverride_FilmGrainIntensity = true;
	Settings.FilmGrainIntensity = 0.2f;
}
