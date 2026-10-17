#include "MainMenuGameMode.h"
#include "DriftParticles.h"
#include "FlameFX.h"
#include "GameAudio.h"
#include "GroundCheckSubsystem.h"
#include "MainMenuWidget.h"
#include "ElvisAnimInstance.h"
#include "PlayerHUDWidget.h"
#include "RomeProgressSubsystem.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GroomAsset.h"
#include "GroomComponent.h"
#include "Misc/CommandLine.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace MenuSet
{
	const TCHAR* ArenaMap = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"); // test runs
	const TCHAR* GameMap = TEXT("/Game/Rome/L_Rome");                  // New Game: the Rome map, the colosseum inside it
	const TCHAR* Songs[] = { TEXT("/Game/Audio/S_MenuMusic_01.S_MenuMusic_01"), TEXT("/Game/Audio/S_MenuMusic_02.S_MenuMusic_02"),
		TEXT("/Game/Audio/S_MenuMusic_03.S_MenuMusic_03") };

	// The stage, in cm, inside the colosseum (centre at the origin, the same size as the arena's: 50 x 40 m outside,
	// rim 17.5 m). The camera stands near the south wall looking north across the sand at the statue; the menu column
	// sits in the dark wall on the left, the statue centre-right, Elvis in the right foreground with his back to us.
	const FVector CameraAt(-2700.f, 420.f, 210.f);
	const FVector LookAt(0.f, -300.f, 470.f);
	const FVector StatueAt(0.f, 0.f, 0.f);
	const float StatueScale = 4.5f;
	const FVector ElvisAt(-1500.f, 360.f, 0.f);
	const FVector ColosseumScale(18.96f, 15.03f, 18.6f); // the 530 x 530 x 94 mesh stretched to the arena's size (100 x 80 m, rim 17.5 m)
	const FVector Braziers[] = { FVector(-480.f, -470.f, 0.f), FVector(-480.f, 470.f, 0.f), FVector(460.f, -470.f, 0.f), FVector(460.f, 470.f, 0.f) };
	const FVector CapeWind(-40.f, 220.f, 30.f); // the storm wind lifting Elvis's cape
	const FLinearColor BloodRed(1.f, 0.16f, 0.06f);

	template <typename T>
	T* Load(const TCHAR* Path)
	{
		return LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FVector SunAt()
	{
		// Low over the far wall, a little left of the statue, where it sits beside the crown on screen.
		const FVector Look = (LookAt - CameraAt).GetSafeNormal2D();
		const float Yaw = FMath::Atan2(Look.Y, Look.X) + FMath::DegreesToRadians(19.f);
		const float Elevation = FMath::DegreesToRadians(13.5f);
		return CameraAt + 9000.f * FVector(FMath::Cos(Elevation) * FMath::Cos(Yaw), FMath::Cos(Elevation) * FMath::Sin(Yaw), FMath::Sin(Elevation));
	}
}

AMainMenuGameMode::AMainMenuGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr; // nobody to play: the camera is the view
}

void AMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Back at the menu: the next game starts from the beginning.
	if (URomeProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<URomeProgressSubsystem>())
	{
		Progress->Reset();
	}

	// Test runs (-EnvShot, -OtterShot, ...) want the arena, not the menu.
	if (UGroundCheckSubsystem::IsTestRun())
	{
		UGameplayStatics::OpenLevel(this, FName(MenuSet::ArenaMap));
		return;
	}

	UMainMenuWidget::ApplySavedVolume();
	BuildScene();
	StartMusic();

	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (Player)
	{
		Player->SetViewTarget(Camera);
		Menu = CreateWidget<UMainMenuWidget>(Player, UMainMenuWidget::StaticClass());
		Menu->SetIsFocusable(true);
		Menu->OnNewGame.BindUObject(this, &AMainMenuGameMode::StartNewGame);
		Menu->OnQuit.BindUObject(this, &AMainMenuGameMode::Quit);
		Menu->AddToViewport();

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Menu->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Player->SetInputMode(InputMode);
		Player->bShowMouseCursor = true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("MenuShot")))
	{
		StartMenuShot();
	}
}

void AMainMenuGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	// The player character puts up its HUD when it spawns; the menu has no use for it.
	if (Time < 3.f)
	{
		TArray<UUserWidget*> Huds;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Huds, UPlayerHUDWidget::StaticClass(), false);
		for (UUserWidget* Hud : Huds)
		{
			Hud->RemoveFromParent();
		}

		// The storm wind in Elvis's cape (his anim instance exists once he has ticked).
		if (Elvis)
		{
			TArray<USkeletalMeshComponent*> Skins;
			Elvis->GetComponents(Skins);
			for (USkeletalMeshComponent* Skin : Skins)
			{
				if (UElvisAnimInstance* Anim = Cast<UElvisAnimInstance>(Skin->GetAnimInstance()))
				{
					Anim->CapeWind = MenuSet::CapeWind;
				}
			}
		}
	}

	// A slow, breathing drift, never quite repeating.
	if (Camera)
	{
		const FVector Drift(
			20.f * FMath::Sin(Time * 0.11f),
			40.f * FMath::Sin(Time * 0.07f + 1.f) + 10.f * FMath::Sin(Time * 0.19f),
			12.f * FMath::Sin(Time * 0.13f + 2.f));
		const FVector Location = CameraBase + Drift;
		const FVector Target = CameraLookAt + FVector(0.f, 20.f * FMath::Sin(Time * 0.05f), 8.f * FMath::Sin(Time * 0.09f));
		Camera->SetActorLocationAndRotation(Location, (Target - Location).Rotation());
	}

	UpdateStorm(DeltaSeconds);
}

FVector AMainMenuGameMode::CrownLocation() const
{
	// Just above the statue's head bone (the god model uses the mannequin names).
	if (Statue && Statue->DoesSocketExist(TEXT("head")))
	{
		return Statue->GetSocketLocation(TEXT("head")) + FVector(0.f, 0.f, 16.f * MenuSet::StatueScale);
	}
	return MenuSet::StatueAt + FVector(0.f, 0.f, 175.f * MenuSet::StatueScale);
}

void AMainMenuGameMode::UpdateStorm(float DeltaSeconds)
{
	// The crown breathes: a slow red pulse with a flicker on top, as if something inside it is awake.
	if (CrownLight)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 1.7f);
		const float Flicker = 0.85f + 0.15f * FMath::Sin(Time * 23.f) * FMath::Sin(Time * 7.3f);
		CrownLight->SetIntensity((120.f + 420.f * Pulse * Pulse) * Flicker);
		const FVector At = CrownLocation();
		CrownLight->SetWorldLocation(At);
		if (CrownEmbers)
		{
			CrownEmbers->SetActorLocation(At + FVector(0.f, 0.f, 120.f));
		}
		// The spikes ring the brow (the pose settles after the first frames, so they follow the head bone).
		const float S = MenuSet::StatueScale;
		for (int32 i = 0; i < CrownSpikes.Num(); ++i)
		{
			const float Angle = UE_TWO_PI * i / CrownSpikes.Num();
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			const float Tall = CrownSpikes[i]->GetComponentScale().Z * 100.f; // cone is 100 units tall
			const FQuat Lean = FQuat(FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal(), FMath::DegreesToRadians(14.f));
			CrownSpikes[i]->SetWorldLocationAndRotation(At + Out * 10.5f * S + Lean.RotateVector(FVector(0.f, 0.f, 0.5f * Tall)) - FVector(0.f, 0.f, 8.f * S), Lean);
		}
		if (CrownMaterial)
		{
			CrownMaterial->SetScalarParameterValue(TEXT("Glow"), (5.f + 9.f * Pulse * Pulse) * Flicker);
		}
	}

	// Lightning now and then: a double flash over the colosseum, its thunder a moment later.
	if (Lightning)
	{
		if (Time >= NextFlash)
		{
			FlashTime = Time;
			NextFlash = Time + FMath::FRandRange(8.f, 16.f);
			ThunderAt = Time + FMath::FRandRange(0.5f, 1.4f);
			Lightning->SetWorldRotation(FRotator(-FMath::FRandRange(35.f, 60.f), FMath::FRandRange(120.f, 220.f), 0.f));
		}
		const float T = Time - FlashTime;
		float Flash = 0.f;
		if (T >= 0.f && T < 0.6f)
		{
			Flash = FMath::Max(FMath::Exp(-T * 30.f), 0.7f * FMath::Exp(-FMath::Max(T - 0.18f, 0.f) * 18.f) * (T > 0.18f ? 1.f : 0.f));
		}
		Lightning->SetIntensity(18.f * Flash);
		if (ThunderAt > 0.f && Time >= ThunderAt)
		{
			ThunderAt = -1.f;
			if (!bLeaving)
			{
				GameAudio::Play2D(this, TEXT("Thunder"), 0.55f, FMath::FRandRange(0.8f, 1.05f));
			}
		}
	}
}

// ---------------------------------------------------------------- scene

void AMainMenuGameMode::BuildScene()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Set = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	USceneComponent* Root = NewObject<USceneComponent>(Set, TEXT("Root"));
	Set->SetRootComponent(Root);
	Root->RegisterComponent();

	auto AddMesh = [&](UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material = nullptr)
	{
		if (!Mesh)
		{
			return (UStaticMeshComponent*)nullptr;
		}
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Set);
		Component->SetStaticMesh(Mesh);
		Component->SetupAttachment(Root);
		Component->SetWorldTransform(Transform);
		if (Material)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetCollisionProfileName(TEXT("BlockAll"));
		Component->RegisterComponent();
		return Component;
	};
	auto Mesh = [](const TCHAR* Folder, const TCHAR* Name) { return MenuSet::Load<UStaticMesh>(*FString::Printf(TEXT("%s%s.%s"), Folder, Name, Name)); };

	// The arena's sand, out under the walls.
	UStaticMesh* Cylinder = MenuSet::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Dunes = MenuSet::Load<UMaterialInterface>(TEXT("/Game/ThirdPerson/Colosseum/M_Dunes.M_Dunes"));
	if (Cylinder && Dunes)
	{
		UMaterialInstanceDynamic* Sand = UMaterialInstanceDynamic::Create(Dunes, Set);
		Sand->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.5f, 0.42f, 0.34f));
		for (const TCHAR* Map : { TEXT("D"), TEXT("N"), TEXT("R") })
		{
			if (UTexture* Texture = MenuSet::Load<UTexture>(*FString::Printf(TEXT("/Game/Environment/Textures/T_DesertSand_%s.T_DesertSand_%s"), Map, Map)))
			{
				Sand->SetTextureParameterValue(*FString::Printf(TEXT("Sand_%s"), Map), Texture);
			}
		}
		const FBox Box = Cylinder->GetBoundingBox();
		AddMesh(Cylinder, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100.f), FVector(16000.f / Box.GetSize().X, 16000.f / Box.GetSize().Y, 200.f / Box.GetSize().Z)), Sand);
	}

	// The colosseum around it all, the arena's own mesh at the arena's size.
	if (UStaticMesh* Colosseum = MenuSet::Load<UStaticMesh>(TEXT("/Game/ThirdPerson/colosseum.colosseum")))
	{
		const FBox Box = Colosseum->GetBoundingBox();
		const FVector S = MenuSet::ColosseumScale;
		AddMesh(Colosseum, FTransform(FRotator::ZeroRotator, FVector(-Box.GetCenter().X * S.X, -Box.GetCenter().Y * S.Y, -Box.Min.Z * S.Z - 30.f), S));
	}

	// Toppled columns and broken drums lying in the sand, as the story has them round the colosseum.
	const TCHAR* Meshes = TEXT("/Game/ThirdPerson/Colosseum/Meshes/");
	const TCHAR* WorldSet = TEXT("/Game/ThirdPerson/Colosseum/World/");
	AddMesh(Mesh(Meshes, TEXT("SM_RomanColumn")), FTransform(FRotator::ZeroRotator, FVector(-1300.f, -1700.f, 0.f), FVector(1.4f)));
	AddMesh(Mesh(Meshes, TEXT("SM_RomanColumn_BrokenA")), FTransform(FRotator(0.f, 40.f, 0.f), FVector(900.f, 1500.f, 0.f), FVector(1.3f)));
	AddMesh(Mesh(Meshes, TEXT("SM_RomanColumn_BrokenB")), FTransform(FRotator(0.f, 10.f, 0.f), FVector(-2100.f, -900.f, 0.f), FVector(1.2f)));
	struct FFallen { const TCHAR* Name; FVector At; float Yaw; float Scale; };
	const FFallen Fallen[] = {
		{ TEXT("SM_RomanColumn"), FVector(-1500.f, -1200.f, 0.f), 70.f, 1.3f },
		{ TEXT("SM_RomanColumn_BrokenA"), FVector(1300.f, -1300.f, 0.f), -35.f, 1.2f },
		{ TEXT("SM_RomanColumn_BrokenB"), FVector(-900.f, 1400.f, 0.f), 30.f, 1.1f },
		{ TEXT("SM_RomanColumn"), FVector(1600.f, 900.f, 0.f), 100.f, 1.2f } };
	for (const FFallen& Piece : Fallen)
	{
		AddMesh(Mesh(Meshes, Piece.Name), FTransform(FRotator(-90.f, Piece.Yaw, 0.f), Piece.At + FVector(0.f, 0.f, 52.f * Piece.Scale), FVector(Piece.Scale)));
	}
	AddMesh(Mesh(WorldSet, TEXT("SM_RomanBlock_B")), FTransform(FRotator(8.f, 25.f, 4.f), FVector(-1700.f, -500.f, -10.f), FVector(2.2f, 1.6f, 1.2f)));
	AddMesh(Mesh(WorldSet, TEXT("SM_RomanBlock_C")), FTransform(FRotator(-6.f, -40.f, 3.f), FVector(1100.f, 1900.f, -10.f), FVector(2.f, 1.5f, 1.3f)));

	// Sparse dry grass and stones in the sand.
	FRandomStream Random(1977);
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> Instancers;
	auto Instance = [&](UStaticMesh* InstanceMesh, const FTransform& Transform)
	{
		if (!InstanceMesh)
		{
			return;
		}
		UInstancedStaticMeshComponent*& Instancer = Instancers.FindOrAdd(InstanceMesh);
		if (!Instancer)
		{
			Instancer = NewObject<UInstancedStaticMeshComponent>(Set);
			Instancer->SetStaticMesh(InstanceMesh);
			Instancer->SetupAttachment(Root);
			Instancer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Instancer->RegisterComponent();
		}
		Instancer->AddInstance(Transform, true);
	};
	auto Clear = [](const FVector& P)
	{
		// Keep the plinth, the braziers and the space between Elvis and the statue open.
		return FVector::Dist2D(P, MenuSet::StatueAt) > 800.f && FVector::Dist2D(P, MenuSet::ElvisAt) > 250.f
			&& FMath::Abs(P.Y - 0.5f * (MenuSet::ElvisAt.Y + MenuSet::StatueAt.Y)) > 300.f;
	};
	const TCHAR* Ground = TEXT("/Game/Rome/Ground/");
	UStaticMesh* Grass[3] = { Mesh(Ground, TEXT("SM_Esparto_01")), Mesh(Ground, TEXT("SM_Esparto_02")), Mesh(Ground, TEXT("SM_DeadShrub_01")) };
	UStaticMesh* Pebbles[3] = { Mesh(Ground, TEXT("SM_Pebbles_01")), Mesh(Ground, TEXT("SM_Pebbles_02")), Mesh(Ground, TEXT("SM_Pebbles_03")) };
	auto Upright = [&](const FVector& P, float Scale) { return FTransform(FRotator(Random.FRandRange(-4.f, 4.f), Random.FRandRange(0.f, 360.f), 0.f), P, FVector(Scale)); };
	for (int32 i = 0; i < 90; ++i)
	{
		const FVector P(Random.FRandRange(-2600.f, 2600.f), Random.FRandRange(-2200.f, 2200.f), -3.f);
		if (Clear(P))
		{
			Instance(Grass[Random.RandRange(0, 2)], Upright(P, Random.FRandRange(0.5f, 1.f)));
		}
	}
	for (int32 i = 0; i < 140; ++i)
	{
		const FVector P(Random.FRandRange(-2600.f, 2600.f), Random.FRandRange(-2200.f, 2200.f), -2.f);
		if (FVector::Dist2D(P, MenuSet::ElvisAt) > 120.f)
		{
			Instance(Pebbles[Random.RandRange(0, 2)], Upright(P, Random.FRandRange(0.7f, 1.4f)));
		}
	}

	// The possessed god in bronze, braziers burning round his plinth.
	SpawnStatue(MenuSet::StatueAt, 180.f, MenuSet::StatueScale);
	for (const FVector& At : MenuSet::Braziers)
	{
		SpawnBrazier(At);
	}
	auto Fill = [&](const FVector& At, float Candelas, float Radius, const FLinearColor& Color)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Set);
		Light->SetupAttachment(Root);
		Light->SetWorldLocation(At);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Candelas);
		Light->SetAttenuationRadius(Radius);
		Light->SetLightColor(Color);
		Light->SetVolumetricScatteringIntensity(0.3f);
		Light->RegisterComponent();
		return Light;
	};
	Fill(MenuSet::StatueAt + FVector(-700.f, -150.f, 250.f), 160.f, 2200.f, FLinearColor(1.f, 0.45f, 0.18f)); // firelight up the bronze
	Fill(MenuSet::ElvisAt + FVector(-250.f, -300.f, 220.f), 60.f, 900.f, FLinearColor(1.f, 0.5f, 0.25f));   // firelight on Elvis's back and cape
	CrownLight = Fill(CrownLocation(), 300.f, 1100.f, MenuSet::BloodRed);
	// The crown itself: a ring of glowing red spikes round the statue's brow, the one thing in the bronze that is alive.
	UStaticMesh* Cone = MenuSet::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterialInterface* ShapeMat = MenuSet::Load<UMaterialInterface>(TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"));
	if (Cone && ShapeMat && Statue && !CrownMaterial) // Boss Vulcan wears his real crown (SpawnStatue)
	{
		UMaterialInstanceDynamic* Ember = UMaterialInstanceDynamic::Create(ShapeMat, Set);
		Ember->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.18f, 0.04f));
		Ember->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		Ember->SetScalarParameterValue(TEXT("Roughness"), 0.6f);
		Ember->SetScalarParameterValue(TEXT("Glow"), 9.f);
		CrownMaterial = Ember;
		const FVector Size = Cone->GetBoundingBox().GetSize();
		const float S = MenuSet::StatueScale;
		for (int32 i = 0; i < 9; ++i)
		{
			const float Angle = UE_TWO_PI * i / 9.f;
			const float Tall = (i % 2 == 0 ? 15.f : 9.f) * S;
			UStaticMeshComponent* Spike = NewObject<UStaticMeshComponent>(Set);
			Spike->SetStaticMesh(Cone);
			Spike->SetMaterial(0, Ember);
			Spike->SetCastShadow(false);
			Spike->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Spike->SetupAttachment(Set->GetRootComponent());
			Spike->SetWorldScale3D(FVector(3.2f * S, 3.2f * S, Tall) / Size); // placed round the head each tick (UpdateStorm)
			Spike->RegisterComponent();
			CrownSpikes.Add(Spike);
		}
	}
	CrownLight->SetVolumetricScatteringIntensity(2.f);
	SpawnElvis(MenuSet::ElvisAt, FMath::RadiansToDegrees(FMath::Atan2(MenuSet::StatueAt.Y - MenuSet::ElvisAt.Y, MenuSet::StatueAt.X - MenuSet::ElvisAt.X)));

	// The blood sun, low over the far wall.
	UStaticMesh* Sphere = MenuSet::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Shape = MenuSet::Load<UMaterialInterface>(TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"));
	if (Sphere && Shape)
	{
		UMaterialInstanceDynamic* Glow = UMaterialInstanceDynamic::Create(Shape, Set);
		Glow->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.08f, 0.03f));
		Glow->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		Glow->SetScalarParameterValue(TEXT("Roughness"), 1.f);
		Glow->SetScalarParameterValue(TEXT("Glow"), 2.2f);
		if (UStaticMeshComponent* Sun = AddMesh(Sphere, FTransform(FRotator::ZeroRotator, MenuSet::SunAt(), FVector(520.f / Sphere->GetBoundingBox().GetSize().X)), Glow))
		{
			Sun->SetCastShadow(false);
			Sun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// The sun's red light from behind the statue, rimming him and Elvis; and the lightning, dark until it strikes.
	const FRotator FromSun = (MenuSet::StatueAt - MenuSet::SunAt()).Rotation();
	if (ADirectionalLight* SunLight = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FromSun, SpawnParams))
	{
		UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent());
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensity(5.f);
		Light->SetLightColor(FLinearColor(1.f, 0.22f, 0.1f));
		Light->SetVolumetricScatteringIntensity(1.2f);
		Light->ForwardShadingPriority = 1; // the main light for fog and translucency, not the lightning
	}
	if (ADirectionalLight* Bolt = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-50.f, 160.f, 0.f), SpawnParams))
	{
		Lightning = Cast<UDirectionalLightComponent>(Bolt->GetLightComponent());
		Lightning->SetMobility(EComponentMobility::Movable);
		Lightning->SetIntensity(0.f);
		Lightning->SetLightColor(FLinearColor(0.75f, 0.72f, 1.f));
		Lightning->SetCastShadows(false);
	}

	// Blood rain, and embers rising off the crown.
	if (ADriftParticles* Rain = World->SpawnActorDeferred<ADriftParticles>(ADriftParticles::StaticClass(), FTransform::Identity))
	{
		Rain->bFollowCamera = true;
		Rain->Count = 3500;
		Rain->Area = FVector(4000.f, 4000.f, 2200.f);
		Rain->Velocity = FVector(180.f, 120.f, -2100.f);
		UGameplayStatics::FinishSpawningActor(Rain, FTransform::Identity);
	}
	if (ADriftParticles* Embers = World->SpawnActorDeferred<ADriftParticles>(ADriftParticles::StaticClass(), FTransform(CrownLocation())))
	{
		Embers->bFollowCamera = false;
		Embers->Count = 55;
		Embers->Area = FVector(160.f, 160.f, 260.f);
		Embers->Velocity = FVector(10.f, 20.f, 55.f);
		Embers->Turbulence = 45.f;
		Embers->ParticleSize = FVector(1.4f, 1.4f, 4.f);
		Embers->Color = FLinearColor(1.f, 0.28f, 0.05f);
		Embers->Glow = 12.f;
		UGameplayStatics::FinishSpawningActor(Embers, FTransform(CrownLocation()));
		CrownEmbers = Embers;
	}

	// The storm's haze, lit red, thin enough to see the far wall and the sun through.
	if (AExponentialHeightFog* FogActor = World->SpawnActor<AExponentialHeightFog>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator, SpawnParams))
	{
		UExponentialHeightFogComponent* Fog = FogActor->GetComponent();
		Fog->SetFogDensity(0.012f);
		Fog->SetFogHeightFalloff(0.25f);
		Fog->SetFogInscatteringColor(FLinearColor(0.035f, 0.008f, 0.006f));
		Fog->SetStartDistance(400.f);
		Fog->SetVolumetricFog(true);
		Fog->SetVolumetricFogScatteringDistribution(0.7f);
		Fog->SetVolumetricFogExtinctionScale(0.8f);
		Fog->SetVolumetricFogDistance(9000.f);
	}
	if (APostProcessVolume* Grade = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
	{
		Grade->bUnbound = true;
		FPostProcessSettings& Settings = Grade->Settings;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 1.4f;
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.85f;
		Settings.bOverride_FilmGrainIntensity = true;
		Settings.FilmGrainIntensity = 0.25f;
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(0.9f, 0.9f, 0.9f, 1.f);
		Settings.bOverride_ColorContrast = true;
		Settings.ColorContrast = FVector4(1.15f, 1.15f, 1.15f, 1.f);
		// Fixed exposure (EV100) for a firelit night, so the flashes and fires don't pump the brightness around.
		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.AutoExposureMinBrightness = 2.6f;
		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.AutoExposureMaxBrightness = 2.6f;
		Settings.bOverride_MotionBlurAmount = true;
		Settings.MotionBlurAmount = 0.f;
	}

	CameraBase = MenuSet::CameraAt;
	CameraLookAt = MenuSet::LookAt;
	Camera = World->SpawnActor<ACameraActor>(CameraBase, (CameraLookAt - CameraBase).Rotation(), SpawnParams);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(45.f);
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}
}

void AMainMenuGameMode::SpawnElvis(const FVector& Location, float Yaw)
{
	// The real player character, standing idle with his spear, cape and all; nobody controls him.
	UClass* CharacterClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	if (!CharacterClass)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Elvis = GetWorld()->SpawnActor<ACharacter>(CharacterClass, Location + FVector(0.f, 0.f, 120.f), FRotator(0.f, Yaw, 0.f), Params);
	if (Elvis)
	{
		const float HalfHeight = Elvis->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Elvis->SetActorLocation(Location + FVector(0.f, 0.f, HalfHeight + 1.f));
		Elvis->AutoPossessAI = EAutoPossessAI::Disabled;
	}
}

void AMainMenuGameMode::SpawnStatue(const FVector& Location, float Yaw, float Scale)
{
	// Vulcan as the bronze statue he is bound in, huge on his plinth, the crown still on his head.
	// Boss Vulcan when he is there (his armour, sword and hair in bronze too; his real Ember Crown, not bronze, glowing),
	// else the god-king.
	USkeletalMesh* Boss = MenuSet::Load<USkeletalMesh>(TEXT("/Game/BossVulcan/SK_BossVulcan_Body.SK_BossVulcan_Body"));
	USkeletalMesh* God = Boss ? Boss : MenuSet::Load<USkeletalMesh>(TEXT("/Game/Vulcan/God/SK_VulcanGod.SK_VulcanGod"));
	UAnimSequence* Pose = MenuSet::Load<UAnimSequence>(Boss ? TEXT("/Game/BossVulcan/Animations/A_BossVulcan_Statue.A_BossVulcan_Statue")
		: TEXT("/Game/Vulcan/God/Animations/A_VulcanGod_Statue.A_VulcanGod_Statue"));
	UMaterialInterface* Skin = Boss ? nullptr : MenuSet::Load<UMaterialInterface>(TEXT("/Game/Vulcan/God/MI_VulcanGod.MI_VulcanGod"));
	if (!God || !Set)
	{
		return;
	}
	Statue = NewObject<USkeletalMeshComponent>(Set);
	Statue->SetSkeletalMesh(God);
	Statue->SetupAttachment(Set->GetRootComponent());
	// The mesh faces +Y; turn it so the statue faces down -X toward the camera (Yaw 180).
	// Boss Vulcan is 1.94 m against the god-king's 1.53 m: scaled down to stand as tall in the frame.
	Statue->SetWorldTransform(FTransform(FRotator(0.f, Yaw - 90.f, 0.f), Location, FVector(Boss ? Scale * 1.53f / 1.94f : Scale)));
	Statue->RegisterComponent();
	if (Pose)
	{
		Statue->PlayAnimation(Pose, true);
	}
	if (Boss)
	{
		auto Bronze = [this](UMeshComponent* Part, bool bStatue)
		{
			for (int32 i = 0; i < Part->GetNumMaterials(); ++i)
			{
				UMaterialInstanceDynamic* Material = Part->CreateAndSetMaterialInstanceDynamic(i);
				if (Material)
				{
					Material->SetScalarParameterValue(TEXT("Statue"), bStatue ? 1.f : 0.f);
					if (!bStatue)
					{
						CrownMaterial = Material; // pulsed by UpdateStorm
					}
				}
			}
		};
		Bronze(Statue, true);
		const TCHAR* Parts[] = { TEXT("Armour"), TEXT("Sword"), TEXT("Crown") };
		for (const TCHAR* Name : Parts)
		{
			if (USkeletalMesh* Asset = MenuSet::Load<USkeletalMesh>(*FString::Printf(TEXT("/Game/BossVulcan/SK_BossVulcan_%s.SK_BossVulcan_%s"), Name, Name)))
			{
				USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(Set);
				Part->SetSkeletalMesh(Asset);
				Part->SetupAttachment(Statue);
				Part->RegisterComponent();
				Part->SetLeaderPoseComponent(Statue);
				Bronze(Part, FCString::Strcmp(Name, TEXT("Crown")) != 0);
			}
		}
		// His hair and beard: grooms in the body's rest-pose space, riding the head and jaw bones.
		const FReferenceSkeleton& Ref = Boss->GetRefSkeleton();
		auto RestPose = [&Ref](FName Bone)
		{
			FTransform Pose = FTransform::Identity;
			for (int32 i = Ref.FindBoneIndex(Bone); i != INDEX_NONE; i = Ref.GetParentIndex(i))
			{
				Pose = Pose * Ref.GetRefBonePose()[i];
			}
			return Pose;
		};
		UMaterialInterface* HairMaterial = MenuSet::Load<UMaterialInterface>(TEXT("/Game/BossVulcan/Hair/M_BV_Hair.M_BV_Hair"));
		const TPair<const TCHAR*, FName> Grooms[] = { { TEXT("Head"), TEXT("head") }, { TEXT("Jaw"), TEXT("jaw") } };
		for (const TPair<const TCHAR*, FName>& G : Grooms)
		{
			if (UGroomAsset* Asset = MenuSet::Load<UGroomAsset>(*FString::Printf(TEXT("/Game/BossVulcan/Hair/GR_BV_Hair_%s.GR_BV_Hair_%s"), G.Key, G.Key)))
			{
				UGroomComponent* Hair = NewObject<UGroomComponent>(Set);
				Hair->SetGroomAsset(Asset);
				Hair->SetupAttachment(Statue, G.Value);
				Hair->SetRelativeTransform(RestPose(G.Value).Inverse());
				Hair->RegisterComponent();
				for (int32 i = 0; HairMaterial && i < Hair->GetNumMaterials(); ++i)
				{
					Hair->SetMaterial(i, HairMaterial);
				}
				Bronze(Hair, true);
			}
		}
	}
	if (Skin)
	{
		UMaterialInstanceDynamic* Bronze = UMaterialInstanceDynamic::Create(Skin, Set);
		Bronze->SetScalarParameterValue(TEXT("Statue"), 1.f);
		for (int32 i = 0; i < Statue->GetNumMaterials(); ++i)
		{
			Statue->SetMaterial(i, Bronze);
		}
	}
	// A two-step plinth for him to stand on.
	if (UStaticMesh* Block = MenuSet::Load<UStaticMesh>(TEXT("/Game/ThirdPerson/Colosseum/World/SM_RomanBlock_A.SM_RomanBlock_A")))
	{
		for (int32 Step = 0; Step < 2; ++Step)
		{
			UStaticMeshComponent* Plinth = NewObject<UStaticMeshComponent>(Set);
			Plinth->SetStaticMesh(Block);
			Plinth->SetupAttachment(Set->GetRootComponent());
			const float Width = Step == 0 ? 9.f : 7.f;
			Plinth->SetWorldTransform(FTransform(FRotator(0.f, Yaw, 0.f), Location + FVector(0.f, 0.f, Step == 0 ? -20.f : 15.f), FVector(Width, Width, 0.6f)));
			Plinth->RegisterComponent();
		}
		Statue->SetWorldLocation(Location + FVector(0.f, 0.f, 38.f));
	}
}

void AMainMenuGameMode::SpawnBrazier(const FVector& Location)
{
	UStaticMesh* Pedestal = MenuSet::Load<UStaticMesh>(TEXT("/Game/ThirdPerson/Colosseum/Meshes/SM_RomanColumn_BrokenB.SM_RomanColumn_BrokenB"));
	UStaticMesh* Cylinder = MenuSet::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = MenuSet::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterialInterface* Shape = MenuSet::Load<UMaterialInterface>(TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"));
	if (!Pedestal || !Cylinder || !Cone || !Shape || !Set)
	{
		return;
	}
	UMaterialInstanceDynamic* Iron = UMaterialInstanceDynamic::Create(Shape, Set);
	Iron->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.045f, 0.04f));
	Iron->SetScalarParameterValue(TEXT("Metallic"), 0.9f);
	Iron->SetScalarParameterValue(TEXT("Roughness"), 0.45f);
	Iron->SetScalarParameterValue(TEXT("Glow"), 0.f);
	UMaterialInstanceDynamic* Coals = UMaterialInstanceDynamic::Create(Shape, Set);
	Coals->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.3f, 0.05f));
	Coals->SetScalarParameterValue(TEXT("Metallic"), 0.f);
	Coals->SetScalarParameterValue(TEXT("Roughness"), 1.f);
	Coals->SetScalarParameterValue(TEXT("Glow"), 4.f);

	// Size is the box the mesh is stretched into; Bottom is how high its base sits; Flip turns it upside down.
	auto Part = [&](UStaticMesh* Mesh, const FVector& Size, float Bottom, UMaterialInterface* Material, bool bFlip = false)
	{
		const FBox Box = Mesh->GetBoundingBox();
		const FVector Scale = Size / Box.GetSize();
		const FRotator Rotation(0.f, 0.f, bFlip ? 180.f : 0.f);
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Set);
		Piece->SetStaticMesh(Mesh);
		Piece->SetupAttachment(Set->GetRootComponent());
		Piece->SetWorldScale3D(Scale);
		Piece->SetWorldRotation(Rotation);
		Piece->SetWorldLocation(Location + FVector(0.f, 0.f, Bottom + 0.5f * Size.Z) - Rotation.RotateVector(Box.GetCenter() * Scale));
		if (Material)
		{
			Piece->SetMaterial(0, Material);
		}
		Piece->SetCastShadow(Material != Coals);
		Piece->RegisterComponent();
	};
	// A broken column for a stand, a wide iron bowl (an upturned cone) on top, glowing coals in it.
	Part(Pedestal, FVector(70.f, 70.f, 100.f), -5.f, nullptr);
	Part(Cone, FVector(150.f, 150.f, 45.f), 92.f, Iron, true);
	Part(Cylinder, FVector(135.f, 135.f, 4.f), 133.f, Coals);
	AFlameFX::Spawn(GetWorld(), Location + FVector(0.f, 0.f, 132.f), 170.f, 110.f);
}

// ---------------------------------------------------------------- music, choices

void AMainMenuGameMode::StartMusic()
{
	for (const TCHAR* Path : MenuSet::Songs)
	{
		if (USoundBase* Sound = MenuSet::Load<USoundBase>(Path))
		{
			Songs.Add(Sound);
		}
	}
	if (Songs.IsEmpty())
	{
		return;
	}
	Music = UGameplayStatics::CreateSound2D(this, Songs[0], 1.f, 1.f, 0.f, nullptr, false, false);
	if (Music)
	{
		Music->OnAudioFinished.AddDynamic(this, &AMainMenuGameMode::HandleSongFinished);
		Music->FadeIn(3.f, 1.f);
	}
}

void AMainMenuGameMode::HandleSongFinished()
{
	// The three songs in turn, round and round.
	if (bLeaving || !Music || Songs.IsEmpty() || IsEngineExitRequested() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	Song = (Song + 1) % Songs.Num();
	Music->SetSound(Songs[Song]);
	Music->Play();
}

void AMainMenuGameMode::StartNewGame()
{
	if (bLeaving)
	{
		return;
	}
	bLeaving = true;
	if (Music)
	{
		Music->FadeOut(9.f, 0.f);
	}
	// The prologue card over black, then the game.
	TWeakObjectPtr<AMainMenuGameMode> WeakThis(this);
	Menu->PlayPrologue([WeakThis]()
	{
		if (AMainMenuGameMode* Self = WeakThis.Get())
		{
			if (APlayerController* Player = Self->GetWorld()->GetFirstPlayerController())
			{
				Player->SetInputMode(FInputModeGameOnly());
				Player->bShowMouseCursor = false;
			}
			UGameplayStatics::OpenLevel(Self, FName(MenuSet::GameMap));
		}
	});
}

void AMainMenuGameMode::Quit()
{
	if (bLeaving)
	{
		return;
	}
	bLeaving = true;
	if (Music)
	{
		Music->FadeOut(0.8f, 0.f);
	}
	TWeakObjectPtr<AMainMenuGameMode> WeakThis(this);
	Menu->FadeOut(0.9f, [WeakThis]()
	{
		if (AMainMenuGameMode* Self = WeakThis.Get())
		{
			UKismetSystemLibrary::QuitGame(Self, Self->GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
		}
	});
}

void AMainMenuGameMode::StartMenuShot()
{
	auto Shot = [](const TCHAR* Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("SpearShots") / (FString(Name) + TEXT(".png")), true, false);
	};
	FTimerManager& Timers = GetWorldTimerManager();
	auto After = [&](float Seconds, TFunction<void()> Action)
	{
		FTimerHandle Handle;
		Timers.SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Action)), Seconds, false);
	};
	After(4.2f, [this]()
	{
		// Where the set pieces land on screen (0..1), for framing.
		APlayerController* Player = GetWorld()->GetFirstPlayerController();
		int32 Width = 0, Height = 0;
		if (!Player)
		{
			return;
		}
		Player->GetViewportSize(Width, Height);
		const TPair<const TCHAR*, FVector> Points[] = {
			{ TEXT("ElvisFeet"), MenuSet::ElvisAt }, { TEXT("ElvisHead"), MenuSet::ElvisAt + FVector(0.f, 0.f, 185.f) },
			{ TEXT("StatueFeet"), MenuSet::StatueAt }, { TEXT("Crown"), CrownLocation() }, { TEXT("Sun"), MenuSet::SunAt() } };
		for (const TPair<const TCHAR*, FVector>& Point : Points)
		{
			FVector2D Screen;
			if (Player->ProjectWorldLocationToScreen(Point.Value, Screen))
			{
				UE_LOG(LogTemp, Display, TEXT("MenuShot %s at %.2f %.2f"), Point.Key, Screen.X / Width, Screen.Y / Height);
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("MenuShot %s off screen"), Point.Key);
			}
		}
	});
	// A lightning strike on cue, so one frame catches it.
	After(5.6f, [this]() { NextFlash = Time; });
	After(4.4f, [Shot]() { Shot(TEXT("menu_title")); });
	After(5.62f, [Shot]() { Shot(TEXT("menu_lightning")); });
	After(6.f, [this]() { if (Menu) { Menu->OpenChoices(); } });
	After(7.2f, [Shot]() { Shot(TEXT("menu_choices")); });
	After(7.6f, [this]() { if (Menu) { Menu->OpenOptions(); } });
	After(8.6f, [Shot]() { Shot(TEXT("menu_options")); });
	After(9.f, [this]() { if (Menu) { Menu->CloseOptions(); } });
	// Then New Game: the prologue card, and the game's -EnvShot films the hand-off and quits.
	After(9.5f, [this]() { FCommandLine::Append(TEXT(" -EnvShot")); StartNewGame(); });
	After(9.5f + 6.6f, [Shot]() { Shot(TEXT("menu_prologue")); });
}
