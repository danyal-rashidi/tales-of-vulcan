#include "MainMenuGameMode.h"
#include "DriftParticles.h"
#include "FlameFX.h"
#include "GroundCheckSubsystem.h"
#include "MainMenuWidget.h"
#include "ElvisAnimInstance.h"
#include "PlayerHUDWidget.h"
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
#include "Misc/CommandLine.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace MenuSet
{
	const TCHAR* ArenaMap = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");
	const TCHAR* Songs[] = { TEXT("/Game/Audio/S_MenuMusic_01.S_MenuMusic_01"), TEXT("/Game/Audio/S_MenuMusic_02.S_MenuMusic_02"),
		TEXT("/Game/Audio/S_MenuMusic_03.S_MenuMusic_03") };

	// The stage, in cm: the camera looks down +X; Elvis stands left of centre, the title sits in the dark on the right.
	const FVector CameraAt(-900.f, 40.f, 140.f);
	const FVector LookAt(300.f, -30.f, 170.f);
	const FVector ElvisAt(0.f, -90.f, 0.f);
	const FVector BrazierLeft(380.f, -440.f, 0.f);
	const FVector BrazierRight(700.f, 430.f, 0.f);
	const FVector Bonfire(1250.f, -540.f, 0.f);
	const FVector StatueAt(3400.f, 520.f, 0.f);
	const FVector CapeWind(-60.f, 260.f, 30.f); // a breeze from the left of the screen lifting Elvis's cape

	template <typename T>
	T* Load(const TCHAR* Path)
	{
		return LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
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

	// Test runs (-EnvShot, -OtterShot, ...) want the arena, not the menu.
	if (UGroundCheckSubsystem::IsTestRun())
	{
		UGameplayStatics::OpenLevel(this, FName(MenuSet::ArenaMap));
		return;
	}

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

		// A breeze in Elvis's cape (his anim instance exists once he has ticked).
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
			18.f * FMath::Sin(Time * 0.11f),
			45.f * FMath::Sin(Time * 0.07f + 1.f) + 12.f * FMath::Sin(Time * 0.19f),
			14.f * FMath::Sin(Time * 0.13f + 2.f));
		const FVector Location = CameraBase + Drift;
		const FVector Target = CameraLookAt + FVector(0.f, 25.f * FMath::Sin(Time * 0.05f), 8.f * FMath::Sin(Time * 0.09f));
		Camera->SetActorLocationAndRotation(Location, (Target - Location).Rotation());
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

	// Sand underfoot, as far as the fog lets you see.
	UStaticMesh* Cylinder = MenuSet::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Dunes = MenuSet::Load<UMaterialInterface>(TEXT("/Game/ThirdPerson/Colosseum/M_Dunes.M_Dunes"));
	if (Cylinder && Dunes)
	{
		UMaterialInstanceDynamic* Sand = UMaterialInstanceDynamic::Create(Dunes, Set);
		Sand->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.55f, 0.47f, 0.38f));
		for (const TCHAR* Map : { TEXT("D"), TEXT("N"), TEXT("R") })
		{
			if (UTexture* Texture = MenuSet::Load<UTexture>(*FString::Printf(TEXT("/Game/Environment/Textures/T_DesertSand_%s.T_DesertSand_%s"), Map, Map)))
			{
				Sand->SetTextureParameterValue(*FString::Printf(TEXT("Sand_%s"), Map), Texture);
			}
		}
		const FBox Box = Cylinder->GetBoundingBox();
		AddMesh(Cylinder, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100.f), FVector(30000.f / Box.GetSize().X, 30000.f / Box.GetSize().Y, 200.f / Box.GetSize().Z)), Sand);
	}

	// Ruins: an arcade and a triumphal arch far back in the fog, columns standing, fallen and broken.
	const TCHAR* World_ = TEXT("/Game/ThirdPerson/Colosseum/World/");
	const TCHAR* Meshes = TEXT("/Game/ThirdPerson/Colosseum/Meshes/");
	auto Mesh = [](const TCHAR* Folder, const TCHAR* Name) { return MenuSet::Load<UStaticMesh>(*FString::Printf(TEXT("%s%s.%s"), Folder, Name, Name)); };
	AddMesh(Mesh(World_, TEXT("SM_ArcadeRuin")), FTransform(FRotator(0.f, 25.f, 0.f), FVector(3600.f, -2600.f, -20.f)));
	AddMesh(Mesh(World_, TEXT("SM_TriumphalArch")), FTransform(FRotator(0.f, -100.f, 0.f), FVector(6200.f, 1900.f, -30.f)));
	AddMesh(Mesh(Meshes, TEXT("SM_RomanColumn")), FTransform(FRotator::ZeroRotator, FVector(950.f, -950.f, 0.f), FVector(1.3f)));
	AddMesh(Mesh(Meshes, TEXT("SM_RomanColumn")), FTransform(FRotator::ZeroRotator, FVector(1700.f, 1050.f, 0.f), FVector(1.3f)));
	AddMesh(Mesh(Meshes, TEXT("SM_RomanColumn_BrokenA")), FTransform(FRotator(0.f, 40.f, 0.f), FVector(650.f, 700.f, 0.f), FVector(1.2f)));
	AddMesh(Mesh(Meshes, TEXT("SM_RomanColumn_BrokenB")), FTransform(FRotator(0.f, 10.f, 0.f), FVector(380.f, -1050.f, 0.f), FVector(1.2f)));
	// Fallen pieces lying on their sides (pitch -90 lays a column along its yaw), resting on the sand.
	struct FFallen { const TCHAR* Name; FVector At; float Yaw; float Scale; };
	const FFallen Fallen[] = {
		{ TEXT("SM_RomanColumn"), FVector(2600.f, -1500.f, 0.f), 70.f, 1.25f },
		{ TEXT("SM_RomanColumn_BrokenA"), FVector(2100.f, -700.f, 0.f), -35.f, 1.2f },
		{ TEXT("SM_RomanColumn_BrokenB"), FVector(1000.f, -900.f, 0.f), 30.f, 1.1f },
		{ TEXT("SM_RomanColumn_BrokenB"), FVector(2300.f, 900.f, 0.f), 100.f, 1.f },
		{ TEXT("SM_RomanColumn_BrokenA"), FVector(1900.f, 1500.f, 0.f), -70.f, 1.1f } };
	for (const FFallen& Piece : Fallen)
	{
		AddMesh(Mesh(Meshes, Piece.Name), FTransform(FRotator(-90.f, Piece.Yaw, 0.f), Piece.At + FVector(0.f, 0.f, 52.f * Piece.Scale), FVector(Piece.Scale)));
	}

	FRandomStream Random(1977);
	auto Clear = [](const FVector& P)
	{
		// Keep the space around Elvis, the fires and the camera's view of them open.
		return FVector::Dist2D(P, MenuSet::ElvisAt) > 260.f && FVector::Dist2D(P, MenuSet::BrazierLeft) > 200.f
			&& FVector::Dist2D(P, MenuSet::BrazierRight) > 200.f && FVector::Dist2D(P, MenuSet::Bonfire) > 260.f && P.X > -200.f;
	};

	// Ground cover and a few palms standing dark against the fog (the Rome assets).
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
	const TCHAR* Ground = TEXT("/Game/Rome/Ground/");
	UStaticMesh* Grass[3] = { Mesh(Ground, TEXT("SM_Esparto_01")), Mesh(Ground, TEXT("SM_Esparto_02")), Mesh(Ground, TEXT("SM_Esparto_03")) };
	UStaticMesh* Shrubs[3] = { Mesh(Ground, TEXT("SM_DeadShrub_01")), Mesh(Ground, TEXT("SM_DeadShrub_02")), Mesh(Ground, TEXT("SM_DesertShrub_01")) };
	UStaticMesh* Pebbles[3] = { Mesh(Ground, TEXT("SM_Pebbles_01")), Mesh(Ground, TEXT("SM_Pebbles_02")), Mesh(Ground, TEXT("SM_Pebbles_03")) };
	auto Upright = [&](const FVector& P, float Scale) { return FTransform(FRotator(Random.FRandRange(-4.f, 4.f), Random.FRandRange(0.f, 360.f), 0.f), P, FVector(Scale)); };
	for (int32 i = 0; i < 160; ++i)
	{
		const FVector P(Random.FRandRange(-500.f, 3500.f), Random.FRandRange(-2600.f, 2600.f), -3.f);
		if (Clear(P))
		{
			Instance(Grass[Random.RandRange(0, 2)], Upright(P, Random.FRandRange(0.5f, 1.1f)));
		}
	}
	for (int32 i = 0; i < 24; ++i)
	{
		const FVector P(Random.FRandRange(0.f, 3500.f), Random.FRandRange(-2600.f, 2600.f), -4.f);
		if (Clear(P))
		{
			Instance(Shrubs[Random.RandRange(0, 2)], Upright(P, Random.FRandRange(0.9f, 1.6f)));
		}
	}
	for (int32 i = 0; i < 120; ++i)
	{
		const FVector P(Random.FRandRange(-600.f, 3000.f), Random.FRandRange(-2400.f, 2400.f), -2.f);
		if (FVector::Dist2D(P, MenuSet::ElvisAt) > 120.f)
		{
			Instance(Pebbles[Random.RandRange(0, 2)], Upright(P, Random.FRandRange(0.7f, 1.4f)));
		}
	}
	const TCHAR* Trees = TEXT("/Game/Rome/Trees/");
	Instance(Mesh(Trees, TEXT("SM_DatePalm_03")), FTransform(FRotator(0.f, 30.f, 0.f), FVector(4300.f, -900.f, -60.f), FVector(1.2f)));
	Instance(Mesh(Trees, TEXT("SM_DatePalm_02")), FTransform(FRotator(0.f, 200.f, 0.f), FVector(5000.f, 700.f, -60.f), FVector(1.3f)));
	Instance(Mesh(Trees, TEXT("SM_DatePalm_01")), FTransform(FRotator(0.f, 120.f, 0.f), FVector(3100.f, 2700.f, -60.f), FVector(1.1f)));

	// Fire: two braziers framing Elvis and a bonfire behind him that backlights him, all real flames.
	SpawnBrazier(MenuSet::BrazierLeft);
	SpawnBrazier(MenuSet::BrazierRight);
	SpawnBonfire(MenuSet::Bonfire);

	SpawnStatue(MenuSet::StatueAt, 195.f, 4.5f);
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
	};
	Fill(FVector(-420.f, -480.f, 160.f), 45.f, 1400.f, FLinearColor(1.f, 0.5f, 0.2f));   // a fire off-screen, lighting Elvis's front
	Fill(MenuSet::StatueAt + FVector(-450.f, -80.f, 120.f), 200.f, 2400.f, FLinearColor(1.f, 0.45f, 0.18f)); // firelight up the statue
	SpawnElvis(MenuSet::ElvisAt, 165.f);

	// Embers hanging in the air.
	if (ADriftParticles* Embers = World->SpawnActorDeferred<ADriftParticles>(ADriftParticles::StaticClass(), FTransform(FVector(900.f, 0.f, 400.f))))
	{
		Embers->bFollowCamera = false;
		Embers->Count = 700;
		Embers->Area = FVector(3000.f, 3600.f, 1000.f);
		Embers->Velocity = FVector(35.f, 20.f, 75.f);
		Embers->Turbulence = 70.f;
		Embers->ParticleSize = FVector(1.6f, 1.6f, 5.f);
		Embers->Color = FLinearColor(1.f, 0.32f, 0.06f);
		Embers->Glow = 8.f;
		UGameplayStatics::FinishSpawningActor(Embers, FTransform(FVector(900.f, 0.f, 400.f)));
	}

	// Night: a cold moon behind the scene rimming everything, thick fog the fires glow through.
	if (ADirectionalLight* Moon = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-22.f, 165.f, 0.f), SpawnParams))
	{
		UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Moon->GetLightComponent());
		Moon->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Light->SetIntensity(4.f);
		Light->SetLightColor(FLinearColor(0.55f, 0.65f, 1.f));
		Light->SetVolumetricScatteringIntensity(1.5f);
	}
	if (AExponentialHeightFog* FogActor = World->SpawnActor<AExponentialHeightFog>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator, SpawnParams))
	{
		UExponentialHeightFogComponent* Fog = FogActor->GetComponent();
		Fog->SetFogDensity(0.02f);
		Fog->SetFogHeightFalloff(0.35f);
		Fog->SetFogInscatteringColor(FLinearColor(0.012f, 0.014f, 0.02f));
		Fog->SetStartDistance(200.f);
		Fog->SetVolumetricFog(true);
		Fog->SetVolumetricFogScatteringDistribution(0.6f);
		Fog->SetVolumetricFogExtinctionScale(1.2f);
		Fog->SetVolumetricFogDistance(7000.f);
	}
	if (APostProcessVolume* Grade = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
	{
		Grade->bUnbound = true;
		FPostProcessSettings& Settings = Grade->Settings;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 1.2f;
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.8f;
		Settings.bOverride_FilmGrainIntensity = true;
		Settings.FilmGrainIntensity = 0.25f;
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(0.85f, 0.85f, 0.85f, 1.f);
		Settings.bOverride_ColorContrast = true;
		Settings.ColorContrast = FVector4(1.15f, 1.15f, 1.15f, 1.f);
		// Fixed exposure (EV100) for a firelit night, so the fires don't pump the brightness around.
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
		Camera->GetCameraComponent()->SetFieldOfView(40.f);
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
	// Vulcan as the bronze statue he starts the fight as, huge, half lost in the fog.
	USkeletalMesh* Otter = MenuSet::Load<USkeletalMesh>(TEXT("/Game/Vulcan/Otter/SK_VulcanOtter.SK_VulcanOtter"));
	UAnimSequence* Pose = MenuSet::Load<UAnimSequence>(TEXT("/Game/Vulcan/Otter/Animations/A_VulcanOtter_Statue.A_VulcanOtter_Statue"));
	UMaterialInterface* Skin = MenuSet::Load<UMaterialInterface>(TEXT("/Game/Vulcan/Otter/M_VulcanOtter.M_VulcanOtter"));
	if (!Otter || !Set)
	{
		return;
	}
	USkeletalMeshComponent* Statue = NewObject<USkeletalMeshComponent>(Set);
	Statue->SetSkeletalMesh(Otter);
	Statue->SetupAttachment(Set->GetRootComponent());
	// The mesh faces +Y; turn it so the statue faces down -X toward the camera (Yaw ~180).
	Statue->SetWorldTransform(FTransform(FRotator(0.f, Yaw - 90.f, 0.f), Location, FVector(Scale)));
	Statue->RegisterComponent();
	if (Pose)
	{
		Statue->PlayAnimation(Pose, true);
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
	// A plinth for him to stand on.
	if (UStaticMesh* Block = MenuSet::Load<UStaticMesh>(TEXT("/Game/ThirdPerson/Colosseum/World/SM_RomanBlock_A.SM_RomanBlock_A")))
	{
		UStaticMeshComponent* Plinth = NewObject<UStaticMeshComponent>(Set);
		Plinth->SetStaticMesh(Block);
		Plinth->SetupAttachment(Set->GetRootComponent());
		Plinth->SetWorldTransform(FTransform(FRotator(0.f, Yaw, 0.f), Location - FVector(0.f, 0.f, 20.f), FVector(7.f, 7.f, 0.6f)));
		Plinth->RegisterComponent();
		Statue->SetWorldLocation(Location + FVector(0.f, 0.f, 42.f));
	}
}

void AMainMenuGameMode::SpawnBonfire(const FVector& Location)
{
	// Charred logs leaning together over glowing coals, in a ring of stones.
	UStaticMesh* Cylinder = MenuSet::Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Rock = MenuSet::Load<UStaticMesh>(TEXT("/Game/KiteDemo/Environments/Rocks/River_Rock_01/SM_River_Rock_01.SM_River_Rock_01"));
	UMaterialInterface* Shape = MenuSet::Load<UMaterialInterface>(TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"));
	if (!Cylinder || !Shape || !Set)
	{
		return;
	}
	UMaterialInstanceDynamic* Char = UMaterialInstanceDynamic::Create(Shape, Set);
	Char->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.025f, 0.018f, 0.012f));
	Char->SetScalarParameterValue(TEXT("Metallic"), 0.f);
	Char->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
	Char->SetScalarParameterValue(TEXT("Glow"), 0.f);
	UMaterialInstanceDynamic* Coals = UMaterialInstanceDynamic::Create(Shape, Set);
	Coals->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.28f, 0.04f));
	Coals->SetScalarParameterValue(TEXT("Metallic"), 0.f);
	Coals->SetScalarParameterValue(TEXT("Roughness"), 1.f);
	Coals->SetScalarParameterValue(TEXT("Glow"), 4.f);

	const FVector CylinderSize = Cylinder->GetBoundingBox().GetSize();
	auto Add = [&](UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Set);
		Piece->SetStaticMesh(Mesh);
		Piece->SetupAttachment(Set->GetRootComponent());
		Piece->SetWorldTransform(Transform);
		if (Material)
		{
			Piece->SetMaterial(0, Material);
		}
		Piece->SetCastShadow(Material != Coals);
		Piece->RegisterComponent();
	};
	Add(Cylinder, FTransform(FRotator::ZeroRotator, Location + FVector(0.f, 0.f, 2.f), FVector(160.f, 160.f, 6.f) / CylinderSize), Coals);
	FRandomStream Random(77);
	for (int32 i = 0; i < 6; ++i)
	{
		// Each log leans in from the edge toward the middle, like a tent of sticks.
		const float Angle = UE_TWO_PI * i / 6.f + Random.FRandRange(-0.2f, 0.2f);
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		const FVector Axis = (-Out * 0.75f + FVector::UpVector * 0.66f).GetSafeNormal();
		Add(Cylinder, FTransform(FRotationMatrix::MakeFromZ(Axis).Rotator(), Location + Out * 42.f + FVector(0.f, 0.f, 42.f),
			FVector(20.f, 20.f, Random.FRandRange(140.f, 175.f)) / CylinderSize), Char);
	}
	if (Rock)
	{
		for (int32 i = 0; i < 11; ++i)
		{
			const float Angle = UE_TWO_PI * i / 11.f + Random.FRandRange(-0.1f, 0.1f);
			Add(Rock, FTransform(FRotator(Random.FRandRange(-15.f, 15.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-15.f, 15.f)),
				Location + FVector(FMath::Cos(Angle) * 150.f, FMath::Sin(Angle) * 150.f, -6.f), FVector(Random.FRandRange(1.2f, 1.7f))), nullptr);
		}
	}
	AFlameFX::Spawn(GetWorld(), Location, 330.f, 220.f);
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
		Music->FadeOut(1.4f, 0.f);
	}
	TWeakObjectPtr<AMainMenuGameMode> WeakThis(this);
	Menu->FadeOut(1.5f, [WeakThis]()
	{
		if (AMainMenuGameMode* Self = WeakThis.Get())
		{
			if (APlayerController* Player = Self->GetWorld()->GetFirstPlayerController())
			{
				Player->SetInputMode(FInputModeGameOnly());
				Player->bShowMouseCursor = false;
			}
			UGameplayStatics::OpenLevel(Self, FName(MenuSet::ArenaMap));
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
	// Title, then the choices, two frames apart to check the flames move.
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
	After(3.8f, [this]()
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
			{ TEXT("BrazierLeftFlame"), MenuSet::BrazierLeft + FVector(0.f, 0.f, 200.f) }, { TEXT("BrazierRightFlame"), MenuSet::BrazierRight + FVector(0.f, 0.f, 200.f) },
			{ TEXT("BonfireTop"), MenuSet::Bonfire + FVector(0.f, 0.f, 300.f) }, { TEXT("StatueHead"), MenuSet::StatueAt + FVector(0.f, 0.f, 950.f) } };
		for (const TPair<const TCHAR*, FVector>& Point : Points)
		{
			FVector2D Screen;
			if (Player->ProjectWorldLocationToScreen(Point.Value, Screen))
			{
				UE_LOG(LogTemp, Display, TEXT("MenuShot %s at %.2f %.2f"), Point.Key, Screen.X / Width, Screen.Y / Height);
			}
		}
	});
	// Record what the menu sounds like, to check the songs actually play.
	After(0.5f, [this]() { UAudioMixerBlueprintLibrary::StartRecordingOutput(GetWorld(), 7.f); });
	// Jump to the end of the first song, so the recording also catches the playlist moving on to the second.
	After(1.f, [this]() { if (Music && Songs.Num() > 1) { Music->Play(FMath::Max(Songs[0]->GetDuration() - 3.f, 0.f)); } });
	After(7.2f, [this]() { UAudioMixerBlueprintLibrary::StopRecordingOutput(GetWorld(), EAudioRecordingExportType::WavFile, TEXT("menu_audio"), FPaths::ProjectSavedDir() / TEXT("SpearShots")); });
	After(4.f, [Shot]() { Shot(TEXT("menu_title")); });
	After(5.f, [this]() { if (Menu) { Menu->OpenChoices(); } });
	After(6.5f, [Shot]() { Shot(TEXT("menu_choices")); });
	After(6.8f, [Shot]() { Shot(TEXT("menu_choices_b")); });
	// Then press New Game, and let the arena's -EnvShot film it and quit: checks the hand-off to the game.
	After(7.5f, [this]() { FCommandLine::Append(TEXT(" -EnvShot")); StartNewGame(); });
}
