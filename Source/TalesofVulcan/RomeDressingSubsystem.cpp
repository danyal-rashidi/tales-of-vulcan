#include "RomeDressingSubsystem.h"
#include "RomeExitGate.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeProxy.h"
#include "TimerManager.h"

static TAutoConsoleVariable<bool> CVarRomeDressing(
	TEXT("tov.RomeDressing"),
	true,
	TEXT("Roman-desert dressing around the colosseum (date palms). Takes effect on the next Play."),
	ECVF_Default);

namespace RomeLayout
{
	// Same arena as ArenaDressingSubsystem's ArenaLayout: centre and inner wall semi-axes, in cm.
	const FVector2D Center(-44.f, -5.f);
	const FVector2D WallRadii(3294.f, 2612.f);
	/** Outside of the colosseum (half its bounds); the rim is about 17.5 m up. */
	const FVector2D OuterRadii(5025.f, 3983.f);

	const FName ColosseumMesh(TEXT("colosseum"));
	const FName DunesMesh(TEXT("SM_Dunes"));

	/** The Rome map's oasis (L_Rome; the plan's rterrain.py puts it 560 m east and 170 m north of the colosseum). */
	const FVector2D OasisCenter(-44.f + 56000.f, -5.f + 17000.f);
	const FVector2D OasisRadii(12500.f, 8500.f);

	/** Where the wall has collapsed into the arena (ArenaDressingSubsystem::BuildRuins). */
	const float CollapsedWalls[] = { 28.f, 152.f, 214.f, 327.f };

	FVector2D OnEllipse(float Degrees, const FVector2D& Radii, float Scale)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		return Center + FVector2D(FMath::Cos(Radians) * Radii.X, FMath::Sin(Radians) * Radii.Y) * Scale;
	}

	FName MeshName(const UPrimitiveComponent* Component)
	{
		const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
		const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
		return Mesh ? Mesh->GetFName() : NAME_None;
	}

	/** Topmost solid surface below Point inside the arena (floor, stands, rubble), ignoring the background dunes. */
	bool FindSurface(UWorld& World, const FVector2D& Point, FVector& OutHit)
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RomeSurface), true);
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
}

void URomeDressingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld() || !CVarRomeDressing.GetValueOnGameThread())
	{
		return;
	}

	// Only in the colosseum level.
	bool bArena = false;
	for (TActorIterator<AStaticMeshActor> It(&InWorld); It && !bArena; ++It)
	{
		bArena = RomeLayout::MeshName(It->GetStaticMeshComponent()) == RomeLayout::ColosseumMesh;
	}
	if (!bArena)
	{
		return;
	}

	// In the Rome map the old dune backdrop would bury the town: sink it so only its far mountains show past the
	// landscape's edge.
	bool bLandscape = false;
	for (TActorIterator<ALandscapeProxy> It(&InWorld); It && !bLandscape; ++It)
	{
		bLandscape = true;
	}
	if (bLandscape)
	{
		for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
		{
			UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
			if (RomeLayout::MeshName(Mesh) == RomeLayout::DunesMesh)
			{
				Mesh->SetMobility(EComponentMobility::Movable);
				Mesh->AddWorldOffset(FVector(0.f, 0.f, -4000.f));
			}
		}
	}

	// Next frame, so ArenaDressingSubsystem has laid its sand floor and rubble to plant on.
	InWorld.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, World = &InWorld]()
	{
		PlantPalms(*World);
		DressGround(*World);
		HangBanners(*World);
		PlaceStandards(*World);
		DressStreets(*World);
	}));
}

void URomeDressingSubsystem::DressGround(UWorld& World)
{
	auto Load = [](const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); };
	UStaticMesh* Grass[3] = { Load(TEXT("/Game/Rome/Ground/SM_Esparto_01.SM_Esparto_01")), Load(TEXT("/Game/Rome/Ground/SM_Esparto_02.SM_Esparto_02")),
		Load(TEXT("/Game/Rome/Ground/SM_Esparto_03.SM_Esparto_03")) };
	UStaticMesh* Shrubs[4] = { Load(TEXT("/Game/Rome/Ground/SM_DesertShrub_01.SM_DesertShrub_01")), Load(TEXT("/Game/Rome/Ground/SM_DesertShrub_02.SM_DesertShrub_02")),
		Load(TEXT("/Game/Rome/Ground/SM_DeadShrub_01.SM_DeadShrub_01")), Load(TEXT("/Game/Rome/Ground/SM_DeadShrub_02.SM_DeadShrub_02")) };
	UStaticMesh* Pebbles[3] = { Load(TEXT("/Game/Rome/Ground/SM_Pebbles_01.SM_Pebbles_01")), Load(TEXT("/Game/Rome/Ground/SM_Pebbles_02.SM_Pebbles_02")),
		Load(TEXT("/Game/Rome/Ground/SM_Pebbles_03.SM_Pebbles_03")) };
	// Photo-scanned rocks from the Kite demo pack.
	UStaticMesh* Boulders[2] = { Load(TEXT("/Game/KiteDemo/Environments/Rocks/Medium_Boulder_002/Medium_Boulder_LowPoly.Medium_Boulder_LowPoly")),
		Load(TEXT("/Game/KiteDemo/Environments/Rocks/Medium_Boulder_001/Medium_Boulder_001.Medium_Boulder_001")) };
	UStaticMesh* RiverRock = Load(TEXT("/Game/KiteDemo/Environments/Rocks/River_Rock_01/SM_River_Rock_01.SM_River_Rock_01"));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Origin(RomeLayout::Center, 0.f);
	AActor* Ground = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(Origin), SpawnParams);
	if (!Ground)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Ground, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	Ground->SetRootComponent(Root);
	Root->SetWorldLocation(Origin);
	Root->RegisterComponent();

	// One instancer per mesh. Wind stops at a distance, where nobody can see it.
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> Instancers;
	auto Instance = [&](UStaticMesh* Mesh, const FTransform& Transform, bool bCastShadow)
	{
		if (!Mesh)
		{
			return;
		}
		UInstancedStaticMeshComponent*& Instancer = Instancers.FindOrAdd(Mesh);
		if (!Instancer)
		{
			Instancer = NewObject<UInstancedStaticMeshComponent>(Ground);
			Instancer->SetMobility(EComponentMobility::Static);
			Instancer->SetStaticMesh(Mesh);
			Instancer->SetupAttachment(Root);
			Instancer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Instancer->SetCanEverAffectNavigation(false);
			Instancer->SetCastShadow(bCastShadow);
			Instancer->WorldPositionOffsetDisableDistance = 4000;
			Instancer->RegisterComponent();
		}
		Instancer->AddInstance(Transform, /*bWorldSpace=*/true);
	};

	// A point on the arena floor at Fraction of the way from the centre to the wall (an ellipse), or false if
	// something other than open floor is there (the centre platform, a column, a rubble heap).
	FRandomStream Random(31415);
	// Where the player starts stays clear, so the camera doesn't start inside a bush or behind a boulder.
	TArray<FVector> KeepClear;
	for (TActorIterator<APlayerStart> It(&World); It; ++It)
	{
		KeepClear.Add(It->GetActorLocation());
	}
	// ...and so do the spots the colosseum's gates bring the player out at (L_Rome).
	for (TActorIterator<ARomeExitGate> It(&World); It; ++It)
	{
		KeepClear.Add(It->GetActorTransform().TransformPosition(It->ExitLocation));
	}
	auto FloorPoint = [&](float MinFraction, float MaxFraction, FVector& OutGround, float& OutAngle)
	{
		OutAngle = Random.FRandRange(0.f, 360.f);
		const float Fraction = FMath::Lerp(MinFraction, MaxFraction, FMath::Sqrt(Random.FRand())); // even spread over the area
		FVector Hit;
		if (!RomeLayout::FindSurface(World, RomeLayout::OnEllipse(OutAngle, RomeLayout::WallRadii, Fraction), Hit) || Hit.Z > 40.f || Hit.Z < -100.f)
		{
			return false;
		}
		for (const FVector& Start : KeepClear)
		{
			if (FVector::DistSquared2D(Hit, Start) < FMath::Square(500.f))
			{
				return false;
			}
		}
		OutGround = Hit;
		return true;
	};
	auto Upright = [&](const FVector& Location, float Scale, float Tilt = 4.f)
	{
		return FTransform(FRotator(Random.FRandRange(-Tilt, Tilt), Random.FRandRange(0.f, 360.f), Random.FRandRange(-Tilt, Tilt)), Location, FVector(Scale));
	};

	// Esparto grass in clumps between the centre platform and the walls.
	for (int32 Clump = 0; Clump < 240; ++Clump)
	{
		FVector Center; float Angle;
		if (!FloorPoint(0.36f, 0.97f, Center, Angle))
		{
			continue;
		}
		const int32 Count = Random.RandRange(1, 4);
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector Location = Center + FVector(Random.FRandRange(-140.f, 140.f), Random.FRandRange(-140.f, 140.f), -3.f);
			Instance(Grass[Random.RandRange(0, 2)], Upright(Location, Random.FRandRange(0.55f, 1.15f), 6.f), true);
		}
	}

	// Shrubs, living and dead, in the outer ring.
	for (int32 i = 0; i < 55; ++i)
	{
		FVector Location; float Angle;
		if (FloorPoint(0.55f, 0.95f, Location, Angle))
		{
			Instance(Shrubs[Random.RandRange(0, 3)], Upright(Location - FVector(0.f, 0.f, 4.f), Random.FRandRange(0.8f, 1.5f)), true);
		}
	}

	// Pebbles everywhere but on the platform.
	for (int32 i = 0; i < 180; ++i)
	{
		FVector Location; float Angle;
		if (FloorPoint(0.3f, 0.97f, Location, Angle))
		{
			Instance(Pebbles[Random.RandRange(0, 2)], Upright(Location - FVector(0.f, 0.f, 2.f), Random.FRandRange(0.7f, 1.5f), 1.f), false);
		}
	}

	// Half-buried rocks: small river rocks scattered, boulders along the wall (clear of the gate at +Y).
	for (int32 i = 0; i < 35; ++i)
	{
		FVector Location; float Angle;
		if (FloorPoint(0.42f, 0.95f, Location, Angle))
		{
			const float Scale = Random.FRandRange(0.5f, 1.f);
			Instance(RiverRock, FTransform(FRotator(Random.FRandRange(0.f, 360.f), Random.FRandRange(0.f, 360.f), 0.f), Location - FVector(0.f, 0.f, 12.f * Scale), FVector(Scale)), true);
		}
	}
	for (int32 i = 0, Placed = 0; i < 60 && Placed < 18; ++i)
	{
		FVector Location; float Angle;
		if (!FloorPoint(0.84f, 0.95f, Location, Angle) || FMath::Abs(FMath::FindDeltaAngleDegrees(Angle, 90.f)) < 16.f)
		{
			continue;
		}
		UStaticMesh* Mesh = Boulders[Random.RandRange(0, 1)];
		if (!Mesh)
		{
			continue;
		}
		const float Scale = Random.FRandRange(0.8f, 1.6f);
		// Plain components rather than instances: the pack's material isn't set up for instancing. They block movement.
		UStaticMeshComponent* Boulder = NewObject<UStaticMeshComponent>(Ground);
		Boulder->SetMobility(EComponentMobility::Static);
		Boulder->SetStaticMesh(Mesh);
		Boulder->SetupAttachment(Root);
		Boulder->SetWorldTransform(FTransform(FRotator(Random.FRandRange(-10.f, 10.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-10.f, 10.f)),
			Location - FVector(0.f, 0.f, 0.3f * Mesh->GetBoundingBox().GetSize().Z * Scale), FVector(Scale)));
		Boulder->SetCollisionProfileName(TEXT("BlockAll"));
		Boulder->SetCanEverAffectNavigation(false);
		Boulder->RegisterComponent();
		++Placed;
	}
}

void URomeDressingSubsystem::PlantPalms(UWorld& World)
{
	UStaticMesh* Palms[3] = {
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rome/Trees/SM_DatePalm_01.SM_DatePalm_01"), nullptr, LOAD_NoWarn | LOAD_Quiet),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rome/Trees/SM_DatePalm_02.SM_DatePalm_02"), nullptr, LOAD_NoWarn | LOAD_Quiet),
		LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rome/Trees/SM_DatePalm_03.SM_DatePalm_03"), nullptr, LOAD_NoWarn | LOAD_Quiet) };
	if (!Palms[0] || !Palms[1] || !Palms[2])
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Origin(RomeLayout::Center, 0.f);
	AActor* Grove = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(Origin), SpawnParams);
	if (!Grove)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Grove, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	Grove->SetRootComponent(Root);
	Root->SetWorldLocation(Origin);
	Root->RegisterComponent();

	UInstancedStaticMeshComponent* Instances[3];
	for (int32 i = 0; i < 3; ++i)
	{
		Instances[i] = NewObject<UInstancedStaticMeshComponent>(Grove);
		Instances[i]->SetMobility(EComponentMobility::Static);
		Instances[i]->SetStaticMesh(Palms[i]);
		Instances[i]->SetupAttachment(Root);
		Instances[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision); // trunks inside the arena get capsules below
		Instances[i]->SetCanEverAffectNavigation(false);
		Instances[i]->RegisterComponent();
	}

	FRandomStream Random(2718);
	auto AddPalm = [&](const FVector& Ground, float Scale, int32 Variant)
	{
		const FRotator Rotation(Random.FRandRange(-3.f, 3.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-3.f, 3.f));
		const FVector Scale3D(Scale * Random.FRandRange(0.95f, 1.05f), Scale * Random.FRandRange(0.95f, 1.05f), Scale);
		Instances[Variant]->AddInstance(FTransform(Rotation, Ground, Scale3D), /*bWorldSpace=*/true);
	};

	// Outside: the dunes have no collision in play (SceneryCollisionSubsystem), so give them query collision
	// just long enough to find the sand under each palm.
	UStaticMeshComponent* Dunes = nullptr;
	for (TActorIterator<AStaticMeshActor> It(&World); It && !Dunes; ++It)
	{
		if (RomeLayout::MeshName(It->GetStaticMeshComponent()) == RomeLayout::DunesMesh)
		{
			Dunes = It->GetStaticMeshComponent();
		}
	}
	const ECollisionEnabled::Type DunesCollision = Dunes ? Dunes->GetCollisionEnabled() : ECollisionEnabled::NoCollision;
	if (Dunes && DunesCollision == ECollisionEnabled::NoCollision)
	{
		Dunes->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		if (!Dunes->GetBodyInstance() || !Dunes->GetBodyInstance()->IsValidBodyInstance())
		{
			Dunes->RecreatePhysicsState();
		}
	}
	// In a level with a landscape (the Rome map), the palms stand on that instead.
	bool bLandscape = false;
	for (TActorIterator<ALandscapeProxy> It(&World); It && !bLandscape; ++It)
	{
		bLandscape = true;
	}
	auto DuneHeight = [&](const FVector2D& Point)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RomeDunes), true);
		if (bLandscape)
		{
			TArray<FHitResult> Hits;
			World.LineTraceMultiByObjectType(Hits, FVector(Point, 30000.f), FVector(Point, -5000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
			for (const FHitResult& Ground : Hits)
			{
				if (Ground.GetActor() && Ground.GetActor()->IsA<ALandscapeProxy>())
				{
					return float(Ground.ImpactPoint.Z);
				}
			}
			return 0.f;
		}
		return Dunes && Dunes->LineTraceComponent(Hit, FVector(Point, 30000.f), FVector(Point, -5000.f), Params) ? float(Hit.ImpactPoint.Z) : 0.f;
	};

	// Nothing grows in a gateway or where a gate lets the player out.
	TArray<FVector2D> GateSpots;
	for (TActorIterator<ARomeExitGate> It(&World); It; ++It)
	{
		GateSpots.Add(FVector2D(It->GetActorLocation()));
		GateSpots.Add(FVector2D(It->GetActorTransform().TransformPosition(It->ExitLocation)));
	}
	auto NearGate = [&GateSpots](const FVector2D& Point)
	{
		for (const FVector2D& Spot : GateSpots)
		{
			if (FVector2D::DistSquared(Point, Spot) < FMath::Square(900.f))
			{
				return true;
			}
		}
		return false;
	};

	// Groves hugging the outside of the colosseum. They are 21-27 m tall so the crowns clear the 17.5 m rim
	// as seen from the arena floor. Bases sink into the sand, since the dunes' collision is only approximate.
	const int32 Groves = 11;
	for (int32 g = 0; g < Groves; ++g)
	{
		const float GroveAngle = 360.f * g / Groves + Random.FRandRange(-10.f, 10.f);
		const int32 Count = Random.RandRange(3, 6);
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector2D Point = RomeLayout::OnEllipse(GroveAngle + Random.FRandRange(-7.f, 7.f), RomeLayout::OuterRadii, Random.FRandRange(1.06f, 1.3f));
			if (NearGate(Point))
			{
				continue;
			}
			AddPalm(FVector(Point, DuneHeight(Point) - 150.f), Random.FRandRange(1.2f, 1.5f), 2);
		}
	}

	// The Rome map's oasis (see the plan's rterrain.py): a grove ringing the water, thicker on the town side.
	if (bLandscape)
	{
		for (int32 i = 0; i < 70; ++i)
		{
			const float Angle = Random.FRandRange(0.f, 360.f);
			const FVector2D Point = RomeLayout::OasisCenter + FVector2D(FMath::Cos(FMath::DegreesToRadians(Angle)) * RomeLayout::OasisRadii.X,
				FMath::Sin(FMath::DegreesToRadians(Angle)) * RomeLayout::OasisRadii.Y) * Random.FRandRange(0.78f, 1.45f);
			const float Height = DuneHeight(Point);
			if (Height < -220.f)
			{
				continue; // that's in the water
			}
			AddPalm(FVector(Point, Height - 40.f), Random.FRandRange(0.7f, 1.15f), Random.RandRange(0, 2));
		}
	}

	if (Dunes && DunesCollision == ECollisionEnabled::NoCollision)
	{
		Dunes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Inside: two or three palms taking root in each collapsed-wall heap, at the arena's edge, clear of the fight.
	for (const float Angle : RomeLayout::CollapsedWalls)
	{
		const int32 Count = Random.RandRange(2, 3);
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector2D Point = RomeLayout::OnEllipse(Angle + Random.FRandRange(-9.f, 9.f), RomeLayout::WallRadii, Random.FRandRange(0.86f, 0.94f));
			FVector Ground;
			if (!RomeLayout::FindSurface(World, Point, Ground))
			{
				continue;
			}
			Ground.Z -= 30.f;
			AddPalm(Ground, Random.FRandRange(0.75f, 1.f), Random.RandRange(0, 1));

			// The lower trunk blocks the player and boss; the fronds don't.
			UCapsuleComponent* Trunk = NewObject<UCapsuleComponent>(Grove);
			Trunk->SetMobility(EComponentMobility::Static);
			Trunk->SetupAttachment(Root);
			Trunk->InitCapsuleSize(38.f, 220.f);
			Trunk->SetWorldLocation(Ground + FVector(0.f, 0.f, 220.f));
			Trunk->SetCollisionProfileName(TEXT("BlockAll"));
			Trunk->SetCanEverAffectNavigation(false);
			Trunk->RegisterComponent();
		}
	}
}

void URomeDressingSubsystem::HangBanners(UWorld& World)
{
	UStaticMesh* Banner = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rome/Props/SM_BannerSPQR.SM_BannerSPQR"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Banner)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Origin(RomeLayout::Center, 0.f);
	AActor* Banners = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(Origin), SpawnParams);
	if (!Banners)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Banners, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	Banners->SetRootComponent(Root);
	Root->SetWorldLocation(Origin);
	Root->RegisterComponent();

	// The colosseum itself, so traces see only its walls (not the columns, rubble and braziers in front of them).
	UStaticMeshComponent* Colosseum = nullptr;
	for (TActorIterator<AStaticMeshActor> It(&World); It && !Colosseum; ++It)
	{
		if (RomeLayout::MeshName(It->GetStaticMeshComponent()) == RomeLayout::ColosseumMesh)
		{
			Colosseum = It->GetStaticMeshComponent();
		}
	}
	if (!Colosseum)
	{
		return;
	}

	// Distance from the arena centre outward to the wall face at Height, or -1 if the trace finds nothing.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RomeBanners), true);
	auto WallAt = [&](float Degrees, float Height, FVector& OutHit, FVector& OutNormal)
	{
		const FVector Start(RomeLayout::OnEllipse(Degrees, RomeLayout::WallRadii, 0.8f), Height);
		const FVector End(RomeLayout::OnEllipse(Degrees, RomeLayout::WallRadii, 1.25f), Height);
		FHitResult Hit;
		if (!Colosseum->LineTraceComponent(Hit, Start, End, Params))
		{
			return -1.f;
		}
		OutHit = Hit.ImpactPoint;
		OutNormal = Hit.ImpactNormal;
		return float(FVector::Dist2D(Hit.ImpactPoint, FVector(RomeLayout::Center, 0.f)));
	};

	// Scan the wall in small steps for solid stretches (piers between the arches, solid from the floor up), then
	// hang a banner in the middle of every stretch wide enough for one, clear of the gate, the collapsed sections
	// and each other. Each hangs from the top of the arena wall (about 3.4 m), just in front of it.
	const float Step = 0.75f;
	const int32 Steps = FMath::RoundToInt(360.f / Step);
	TArray<bool> Solid;
	Solid.Init(false, Steps);
	TArray<float> Distances;
	Distances.Init(-1.f, Steps);
	for (int32 i = 0; i < Steps; ++i)
	{
		FVector Hit, Normal, Other, OtherNormal;
		const float Distance = WallAt(i * Step, 260.f, Hit, Normal);
		const float Low = WallAt(i * Step, 90.f, Other, OtherNormal);
		const float Middle = WallAt(i * Step, 170.f, Other, OtherNormal);
		Solid[i] = Distance > 0.f && Low > 0.f && Middle > 0.f && FMath::Abs(Low - Distance) < 12.f && FMath::Abs(Middle - Distance) < 12.f && Normal.Z < 0.3f;
		Distances[i] = Distance;
	}
	TArray<float> Candidates;
	const int32 Start = Solid.Find(false); // begin a run scan at a gap, so runs don't wrap around
	if (Start == INDEX_NONE)
	{
		return;
	}
	for (int32 k = 0, RunLength = 0; k <= Steps; ++k)
	{
		const int32 i = (Start + k) % Steps;
		if (k < Steps && Solid[i])
		{
			++RunLength;
			continue;
		}
		if (RunLength > 0)
		{
			const int32 Middle = (i - (RunLength + 1) / 2 + Steps) % Steps;
			const float Width = FMath::DegreesToRadians(RunLength * Step) * Distances[Middle];
			if (Width >= 150.f)
			{
				Candidates.Add(Middle * Step);
			}
		}
		RunLength = 0;
	}

	TArray<float> Hung;
	auto Near = [](float A, float B, float Within) { return FMath::Abs(FMath::FindDeltaAngleDegrees(A, B)) < Within; };
	for (const float Angle : Candidates)
	{
		bool bSkip = Near(Angle, 90.f, 10.f);
		for (const float Collapsed : RomeLayout::CollapsedWalls)
		{
			bSkip |= Near(Angle, Collapsed, 16.f);
		}
		for (const float Other : Hung)
		{
			bSkip |= Near(Angle, Other, 14.f);
		}
		if (bSkip)
		{
			continue;
		}

		FVector Face, Normal;
		const float Distance = WallAt(Angle, 260.f, Face, Normal);
		if (Distance < 0.f)
		{
			continue;
		}
		// Climb the face until it ends: that is the top of the wall.
		float Top = 260.f;
		for (float Height = 300.f; Height <= 1400.f; Height += 40.f)
		{
			FVector Hit, HitNormal;
			const float D = WallAt(Angle, Height, Hit, HitNormal);
			if (D < 0.f || FMath::Abs(D - Distance) > 35.f)
			{
				break;
			}
			Top = Height;
		}
		if (Top < 300.f)
		{
			continue; // too low to hang a banner on
		}

		const FVector Inward = FVector(Normal.X, Normal.Y, 0.f).GetSafeNormal();
		const float Scale = FMath::Clamp((Top - 25.f) / 440.f, 0.55f, 1.35f); // the hem just clear of the floor
		UStaticMeshComponent* Cloth = NewObject<UStaticMeshComponent>(Banners);
		Cloth->SetMobility(EComponentMobility::Static);
		Cloth->SetStaticMesh(Banner);
		Cloth->SetupAttachment(Root);
		// The banner faces its +X, so turn it to face into the arena; its rod sits at the wall top.
		Cloth->SetWorldTransform(FTransform(Inward.Rotation(), FVector(Face.X, Face.Y, Top - 10.f) + Inward * 14.f, FVector(Scale)));
		Cloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cloth->SetCanEverAffectNavigation(false);
		Cloth->RegisterComponent();
		Hung.Add(Angle);
		UE_LOG(LogTemp, Display, TEXT("RomeDressing: banner at %.1f deg, wall top %.0f cm"), Angle, Top);
	}
}

void URomeDressingSubsystem::PlaceStandards(UWorld& World)
{
	UStaticMesh* Standard = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Rome/Props/SM_EagleStandard.SM_EagleStandard"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Standard)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Origin(RomeLayout::Center, 0.f);
	AActor* Standards = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform(Origin), SpawnParams);
	if (!Standards)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Standards, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	Standards->SetRootComponent(Root);
	Root->SetWorldLocation(Origin);
	Root->RegisterComponent();

	// Eagle standards: a pair guarding the gate (+Y) and a pair facing them across the arena, all facing the centre.
	for (const float Angle : { 82.f, 98.f, 262.f, 278.f })
	{
		FVector Ground;
		if (!RomeLayout::FindSurface(World, RomeLayout::OnEllipse(Angle, RomeLayout::WallRadii, 0.84f), Ground) || Ground.Z > 60.f)
		{
			continue;
		}
		const FVector ToCentre = (FVector(RomeLayout::Center, Ground.Z) - Ground).GetSafeNormal2D();
		UStaticMeshComponent* Pole = NewObject<UStaticMeshComponent>(Standards);
		Pole->SetMobility(EComponentMobility::Static);
		Pole->SetStaticMesh(Standard);
		Pole->SetupAttachment(Root);
		Pole->SetWorldTransform(FTransform(ToCentre.Rotation(), Ground - FVector(0.f, 0.f, 12.f), FVector(1.15f)));
		Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pole->SetCanEverAffectNavigation(false);
		Pole->RegisterComponent();

		// The shaft blocks the player and boss.
		UCapsuleComponent* Shaft = NewObject<UCapsuleComponent>(Standards);
		Shaft->SetMobility(EComponentMobility::Static);
		Shaft->SetupAttachment(Root);
		Shaft->InitCapsuleSize(12.f, 160.f);
		Shaft->SetWorldLocation(Ground + FVector(0.f, 0.f, 160.f));
		Shaft->SetCollisionProfileName(TEXT("BlockAll"));
		Shaft->SetCanEverAffectNavigation(false);
		Shaft->RegisterComponent();
	}
}

void URomeDressingSubsystem::DressStreets(UWorld& World)
{
	// Only the Rome map has a town.
	if (!TActorIterator<ALandscapeProxy>(&World))
	{
		return;
	}
	auto Load = [](const FString& Name) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Rome/Props/Detail/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet); };
	// What stands against a house front, with how often it's picked.
	TArray<TPair<UStaticMesh*, float>> FrontProps;
	for (int32 i = 1; i <= 6; ++i) { FrontProps.Add({ Load(FString::Printf(TEXT("SM_RomePlanter_%02d"), i)), 1.f }); }
	for (int32 i = 1; i <= 3; ++i) { FrontProps.Add({ Load(FString::Printf(TEXT("SM_RomePot_%02d"), i)), 0.8f }); }
	FrontProps.Add({ Load(TEXT("SM_RomeAmphora_01")), 1.f });
	FrontProps.Add({ Load(TEXT("SM_RomeAmphorae_01")), 0.8f });
	TArray<UStaticMesh*> Market;
	for (int32 i = 1; i <= 5; ++i) { Market.Add(Load(FString::Printf(TEXT("SM_RomeBasket_%02d"), i))); }
	Market.Add(Load(TEXT("SM_RomeSack_01")));
	Market.Add(Load(TEXT("SM_RomeSack_02")));
	Market.Add(Load(TEXT("SM_RomeAmphorae_01")));
	FrontProps.RemoveAll([](const TPair<UStaticMesh*, float>& P) { return P.Key == nullptr; });
	Market.RemoveAll([](const UStaticMesh* M) { return M == nullptr; });
	if (FrontProps.IsEmpty())
	{
		return;
	}
	float TotalWeight = 0.f;
	for (const TPair<UStaticMesh*, float>& P : FrontProps) { TotalWeight += P.Value; }

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Holder = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	USceneComponent* Root = NewObject<USceneComponent>(Holder);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> Instancers;
	auto Add = [&](UStaticMesh* Mesh, const FTransform& Transform)
	{
		UInstancedStaticMeshComponent*& Instancer = Instancers.FindOrAdd(Mesh);
		if (!Instancer)
		{
			Instancer = NewObject<UInstancedStaticMeshComponent>(Holder);
			Instancer->SetStaticMesh(Mesh);
			Instancer->SetupAttachment(Root);
			Instancer->SetCollisionProfileName(TEXT("BlockAll"));
			Instancer->SetCanEverAffectNavigation(false);
			Instancer->RegisterComponent();
		}
		Instancer->AddInstance(Transform, /*bWorldSpace=*/true);
	};

	FRandomStream Random(2718);
	int32 Count = 0;
	for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
	{
		const UStaticMeshComponent* MeshComponent = It->GetStaticMeshComponent();
		const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}
		const FString Name = Mesh->GetName();
		const FTransform Frame = It->GetActorTransform();
		const FBox Box = Mesh->GetBoundingBox();
		if (Name.StartsWith(TEXT("SM_RomeHouse_")))
		{
			// The front faces local -Y: a few things stand against it, clear of the corners.
			const int32 Items = Random.FRand() < 0.55f ? Random.RandRange(1, 3) : 0;
			const float Half = Box.GetExtent().X - 80.f;
			if (Half < 60.f)
			{
				continue;
			}
			const float At = Random.FRandRange(-Half, Half);
			for (int32 i = 0; i < Items; ++i)
			{
				float Pick = Random.FRandRange(0.f, TotalWeight);
				UStaticMesh* Prop = FrontProps[0].Key;
				for (const TPair<UStaticMesh*, float>& P : FrontProps)
				{
					Pick -= P.Value;
					if (Pick <= 0.f)
					{
						Prop = P.Key;
						break;
					}
				}
				const FVector Local(FMath::Clamp(At + i * 75.f, -Half, Half), Box.Min.Y - Random.FRandRange(35.f, 55.f), 0.f);
				const FRotator Turn(0.f, Random.FRandRange(0.f, 360.f), 0.f);
				Add(Prop, FTransform(Turn, Frame.TransformPosition(Local), FVector(Random.FRandRange(0.9f, 1.1f))));
				++Count;
			}
		}
		else if (Name.StartsWith(TEXT("SM_RomeStall_")) && !Market.IsEmpty())
		{
			// Produce heaped around the market stalls.
			for (int32 i = 0, n = Random.RandRange(2, 4); i < n; ++i)
			{
				const float Side = Random.FRand() < 0.5f ? -1.f : 1.f;
				const FVector Local(Side * Random.FRandRange(200.f, 250.f), Random.FRandRange(-120.f, 120.f), 0.f);
				Add(Market[Random.RandRange(0, Market.Num() - 1)], FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Frame.TransformPosition(Local)));
				++Count;
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("RomeDressing: %d street props"), Count);
}
