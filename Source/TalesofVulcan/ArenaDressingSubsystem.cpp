#include "ArenaDressingSubsystem.h"
#include "FireFX.h"
#include "WindGrass.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
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

	/** Tag on the wall-to-wall sand floor laid over the arena. */
	const FName SandFloorTag(TEXT("ArenaSandFloor"));

	/** Top of the centre floor slab; the sand floor sits just above it. */
	constexpr float FloorTop = 2.f;

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
			const bool bFloor = Mesh == FloorMesh || Mesh == ColosseumMesh || (Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(SandFloorTag));
			if (bFloor && Hit.ImpactNormal.Z > 0.85f)
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
	LaySandFloor(InWorld);
	BuildStairs(InWorld);
	PlaceBraziers(InWorld);
	BuildRuins(InWorld);
	PlantGrass(InWorld);
	WeatherMaterials(InWorld);
	ColorGrade(InWorld);
	SetupLighting(InWorld);
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

void UArenaDressingSubsystem::LaySandFloor(UWorld& World)
{
	UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Sand = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ThirdPerson/Colosseum/M_Dunes.M_Dunes"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Disc || !Sand)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Floor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(ArenaLayout::Center, 0.f)), SpawnParams);
	if (!Floor)
	{
		return;
	}

	// A flat elliptical disc a little bigger than the arena, so its edge tucks into the wall,
	// one metre thick, its top just above the old centre slab.
	const FBox Box = Disc->GetBoundingBox();
	const FVector Scale(
		2.f * ArenaLayout::WallRadii.X * 1.03f / Box.GetSize().X,
		2.f * ArenaLayout::WallRadii.Y * 1.03f / Box.GetSize().Y,
		100.f / Box.GetSize().Z);
	const FVector Location(ArenaLayout::Center.X, ArenaLayout::Center.Y, ArenaLayout::FloorTop - Box.Max.Z * Scale.Z);

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Floor, TEXT("SandFloor"));
	Component->SetMobility(EComponentMobility::Static);
	Component->SetStaticMesh(Disc);
	Component->SetWorldLocation(Location);
	Component->SetWorldScale3D(Scale);
	Component->SetCollisionProfileName(TEXT("BlockAll"));
	Component->SetCanEverAffectNavigation(false);
	Component->ComponentTags.Add(ArenaLayout::SandFloorTag);
	Floor->SetRootComponent(Component);
	Component->RegisterComponent();

	// Clean, warm arena sand (the dunes material: world-projected sand with large-scale variation).
	if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Sand, this))
	{
		Material->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.74f, 0.64f, 0.5f));
		Component->SetMaterial(0, Material);
	}
}

void UArenaDressingSubsystem::BuildStairs(UWorld& World)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Chunk = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ThirdPerson/Colosseum/World/SM_RomanBlock_B.SM_RomanBlock_B"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Cube)
	{
		return;
	}

	FRandomStream Random(1871);
	int32 Stairs = 0;

	for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
	{
		UStaticMeshComponent* Ramp = It->GetStaticMeshComponent();
		if (ArenaLayout::MeshName(Ramp) != TEXT("SM_Ramp"))
		{
			continue;
		}

		// Measure the ramp: which local axis it rises along, how long, wide and high it is.
		const FBox Local = Ramp->GetStaticMesh()->GetBoundingBox();
		const FTransform& ToWorld = Ramp->GetComponentTransform();
		const FVector C = Local.GetCenter();
		const FVector E = Local.GetExtent();
		auto TopAt = [&](float LocalX, float LocalY, float& OutZ)
		{
			const FVector Point = ToWorld.TransformPosition(FVector(LocalX, LocalY, C.Z));
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(RampTop), true);
			if (Ramp->LineTraceComponent(Hit, Point + FVector(0.f, 0.f, 2000.f), Point - FVector(0.f, 0.f, 2000.f), Params))
			{
				OutZ = Hit.ImpactPoint.Z;
				return true;
			}
			return false;
		};
		float XPlus, XMinus, YPlus, YMinus;
		if (!TopAt(C.X + 0.4f * E.X, C.Y, XPlus) || !TopAt(C.X - 0.4f * E.X, C.Y, XMinus)
			|| !TopAt(C.X, C.Y + 0.4f * E.Y, YPlus) || !TopAt(C.X, C.Y - 0.4f * E.Y, YMinus))
		{
			continue;
		}
		const bool bAlongX = FMath::Abs(XPlus - XMinus) >= FMath::Abs(YPlus - YMinus);
		const float Sign = bAlongX ? FMath::Sign(XPlus - XMinus) : FMath::Sign(YPlus - YMinus);
		const FVector RunLocal = bAlongX ? FVector(Sign, 0.f, 0.f) : FVector(0.f, Sign, 0.f);
		const FVector WidthLocal = bAlongX ? FVector(0.f, 1.f, 0.f) : FVector(1.f, 0.f, 0.f);

		float Top = 0.f;
		TopAt(C.X + RunLocal.X * 0.97f * E.X, C.Y + RunLocal.Y * 0.97f * E.Y, Top);
		const FVector Scale = ToWorld.GetScale3D().GetAbs();
		const float RunLength = 2.f * (bAlongX ? E.X * Scale.X : E.Y * Scale.Y);
		const float Width = 2.f * (bAlongX ? E.Y * Scale.Y : E.X * Scale.X);
		const float Bottom = FMath::Max(ToWorld.TransformPosition(FVector(C.X, C.Y, Local.Min.Z)).Z, ArenaLayout::FloorTop);
		const float Rise = Top - Bottom;
		if (Rise < 20.f || RunLength < 50.f)
		{
			continue;
		}

		FVector Run = ToWorld.TransformVectorNoScale(RunLocal);
		Run.Z = 0.f;
		Run.Normalize();
		FVector Across = ToWorld.TransformVectorNoScale(WidthLocal);
		Across.Z = 0.f;
		Across.Normalize();
		const FVector Center = ToWorld.TransformPosition(C);
		const FVector LowEnd = FVector(Center.X, Center.Y, 0.f) - Run * (0.5f * RunLength);
		const FRotator Facing = FRotationMatrix::MakeFromXY(Run, Across).Rotator();

		UMaterialInterface* Stone = Ramp->GetMaterial(0);
		AActor* StairActor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(Facing, LowEnd));
		if (!StairActor)
		{
			continue;
		}
		USceneComponent* Root = NewObject<USceneComponent>(StairActor, TEXT("Root"));
		Root->SetMobility(EComponentMobility::Static);
		StairActor->SetRootComponent(Root);
		Root->SetWorldLocationAndRotation(LowEnd, Facing);
		Root->RegisterComponent();

		// One stone block in stair space: X along the run from the low end, Y across, Z up.
		auto Block = [&](UStaticMesh* Mesh, const FVector& Min, const FVector& Max, const FRotator& Tilt, bool bSolid)
		{
			const FVector Size = Max - Min;
			const FBox MeshBox = Mesh->GetBoundingBox();
			UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(StairActor);
			Piece->SetMobility(EComponentMobility::Static);
			Piece->SetStaticMesh(Mesh);
			Piece->SetupAttachment(Root);
			Piece->SetRelativeScale3D(Size / MeshBox.GetSize());
			Piece->SetRelativeRotation(Tilt);
			Piece->SetRelativeLocation((Min + Max) * 0.5f - Tilt.RotateVector(MeshBox.GetCenter() * (Size / MeshBox.GetSize())));
			Piece->SetMaterial(0, Stone);
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
		};
		auto Wobble = [&Random](float Degrees) { return FRotator(Random.FRandRange(-Degrees, Degrees), Random.FRandRange(-Degrees, Degrees), Random.FRandRange(-Degrees, Degrees)); };

		// Steps about 24 cm high, each a solid block from below the sand up to its tread, worn and uneven.
		const int32 StepCount = FMath::Clamp(FMath::RoundToInt(Rise / 24.f), 3, 30);
		const float Depth = RunLength / StepCount;
		const float BaseZ = Bottom - 25.f;
		for (int32 i = 0; i < StepCount; ++i)
		{
			const float Tread = Bottom + Rise * (i + 1) / StepCount;
			const float X0 = i * Depth;
			const float X1 = (i + 1) * Depth + 4.f; // overlap so there are no gaps between steps
			const float Half = 0.5f * Width * Random.FRandRange(0.86f, 1.f);
			const float Shift = Random.FRandRange(-0.04f, 0.04f) * Width;

			if (Random.FRand() < 0.35f && i < StepCount - 1)
			{
				// Broken step: split across, one part sunk and skewed, sometimes chipped short.
				const float Split = Shift + Random.FRandRange(-0.25f, 0.25f) * Width;
				const bool bLeftBroken = Random.FRand() < 0.5f;
				const float Sink = Random.FRandRange(4.f, 12.f);
				const float Chip = Random.FRand() < 0.5f ? Random.FRandRange(0.25f, 0.45f) * Depth : 0.f;
				Block(Cube, FVector(X0, Shift - Half, BaseZ), FVector(X1 - (bLeftBroken ? Chip : 0.f), Split, Tread - (bLeftBroken ? Sink : 0.f)),
					bLeftBroken ? Wobble(4.f) : Wobble(1.2f), true);
				Block(Cube, FVector(X0, Split, BaseZ), FVector(X1 - (bLeftBroken ? 0.f : Chip), Shift + Half, Tread - (bLeftBroken ? 0.f : Sink)),
					bLeftBroken ? Wobble(1.2f) : Wobble(4.f), true);
			}
			else
			{
				Block(Cube, FVector(X0, Shift - Half, BaseZ), FVector(X1, Shift + Half, Tread), Wobble(1.2f), true);
			}

			// Loose chunks lying on some treads.
			if (Chunk && Random.FRand() < 0.3f)
			{
				const float Size = Random.FRandRange(12.f, 30.f);
				const FVector Spot(Random.FRandRange(X0 + Size, X1 - Size), Random.FRandRange(-0.45f, 0.45f) * Width, Tread + Size * 0.3f);
				Block(Chunk, Spot - FVector(Size * 0.5f), Spot + FVector(Size * 0.5f), Wobble(30.f), false);
			}
		}

		// The stairs replace the ramp.
		Ramp->SetVisibility(false);
		Ramp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		++Stairs;
	}

	UE_LOG(LogTemp, Log, TEXT("ArenaDressing: replaced %d ramps with stairs"), Stairs);
}

void UArenaDressingSubsystem::SetupLighting(UWorld& World)
{
	// A low, warm late-afternoon sun: long shadows, golden light and shafts through the arches.
	// Vulcan's storm later swings it from here to the blood-red sun.
	for (TActorIterator<ADirectionalLight> It(&World); It; ++It)
	{
		UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (!Sun || Sun->Mobility == EComponentMobility::Static)
		{
			continue;
		}
		FRotator Angle = Sun->GetComponentRotation();
		Angle.Pitch = -32.f;
		Sun->SetWorldRotation(Angle);
		Sun->SetLightColor(FLinearColor(1.f, 0.8f, 0.6f));
		Sun->bEnableLightShaftBloom = true;
		Sun->BloomScale = 0.25f;
		Sun->BloomThreshold = 0.7f;
		Sun->BloomTint = FColor(255, 210, 160);
		Sun->MarkRenderStateDirty();
		return;
	}
}

void UArenaDressingSubsystem::PlaceBraziers(UWorld& World)
{
	UStaticMesh* Pedestal = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ThirdPerson/Colosseum/World/SM_RomanBlock_A.SM_RomanBlock_A"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Pedestal || !Cylinder || !Shape)
	{
		return;
	}

	UMaterialInstanceDynamic* Iron = UMaterialInstanceDynamic::Create(Shape, this);
	Iron->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.045f, 0.04f));
	Iron->SetScalarParameterValue(TEXT("Metallic"), 0.9f);
	Iron->SetScalarParameterValue(TEXT("Roughness"), 0.45f);
	Iron->SetScalarParameterValue(TEXT("Glow"), 0.f);
	UMaterialInstanceDynamic* Embers = UMaterialInstanceDynamic::Create(Shape, this);
	Embers->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.3f, 0.05f));
	Embers->SetScalarParameterValue(TEXT("Metallic"), 0.f);
	Embers->SetScalarParameterValue(TEXT("Roughness"), 1.f);
	Embers->SetScalarParameterValue(TEXT("Glow"), 3.f);

	// Clear of the gate (90 degrees) and of the four collapsed wall sections.
	for (const float Angle : { 0.f, 55.f, 125.f, 180.f, 250.f, 290.f })
	{
		FVector Ground;
		if (!ArenaLayout::FindGround(World, ArenaLayout::OnWall(Angle, 0.8f), Ground))
		{
			continue;
		}

		AActor* Brazier = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(Ground));
		if (!Brazier)
		{
			continue;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Brazier, TEXT("Root"));
		Root->SetMobility(EComponentMobility::Static);
		Brazier->SetRootComponent(Root);
		Root->SetWorldLocation(Ground);
		Root->RegisterComponent();

		// Size is the box the mesh is stretched into; Bottom is how high its base sits.
		auto Part = [&](UStaticMesh* Mesh, const FVector& Size, float Bottom, UMaterialInterface* Material, bool bSolid)
		{
			const FBox Box = Mesh->GetBoundingBox();
			const FVector Scale = Size / Box.GetSize();
			UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Brazier);
			Piece->SetMobility(EComponentMobility::Static);
			Piece->SetStaticMesh(Mesh);
			Piece->SetupAttachment(Root);
			Piece->SetRelativeScale3D(Scale);
			Piece->SetRelativeLocation(FVector(0.f, 0.f, Bottom + 0.5f * Size.Z) - Box.GetCenter() * Scale);
			if (Material)
			{
				Piece->SetMaterial(0, Material);
			}
			Piece->SetCanEverAffectNavigation(false);
			Piece->SetCastShadow(Material != Embers);
			if (bSolid)
			{
				Piece->SetCollisionProfileName(TEXT("BlockAll"));
			}
			else
			{
				Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			Piece->RegisterComponent();
		};

		Part(Pedestal, FVector(75.f, 75.f, 100.f), -10.f, nullptr, true); // stone, weathered with the rest
		Part(Cylinder, FVector(95.f, 95.f, 26.f), 88.f, Iron, true);       // iron bowl
		Part(Cylinder, FVector(78.f, 78.f, 6.f), 112.f, Embers, false);    // glowing coals

		AFireFX::Spawn(&World, Ground + FVector(0.f, 0.f, 116.f), AFireFX::TorchPreset(28.f));
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

	UMaterialInterface* ColosseumStone = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/ThirdPerson/Colosseum/MI_ColosseumSandstone.MI_ColosseumSandstone"), nullptr, LOAD_NoWarn | LOAD_Quiet);

	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			if (Component->ComponentHasTag(ArenaLayout::SandFloorTag))
			{
				continue; // the arena sand keeps its own clean look
			}
			for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* Material = Component->GetMaterial(Slot);
				const UMaterial* Base = Material ? Material->GetBaseMaterial() : nullptr;
				// One stone for everything: columns and rubble (limestone) switch to the colosseum's stone.
				if (Base && Base->GetName() == TEXT("M_RomanColumn") && ColosseumStone)
				{
					Material = ColosseumStone;
					Base = ColosseumStone->GetBaseMaterial();
				}
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
	Settings.AutoExposureBias = 0.1f;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.65f;
	Settings.bOverride_FilmGrainIntensity = true;
	Settings.FilmGrainIntensity = 0.2f;
}
