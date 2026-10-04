#include "VulcanBoss.h"
#include "HealthComponent.h"
#include "VulcanProjectile.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "DriftParticles.h"
#include "EarthquakeCameraShake.h"
#include "FireFX.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace OtterBody
{
	enum EPart : int32
	{
		Torso, BellyPatch, Head, Muzzle, Nose,
		EyeL, EyeR, ShineL, ShineR, BrowDotL, BrowDotR,
		EarL, EarR, InnerEarL, InnerEarR,
		WhiskerL0, WhiskerL1, WhiskerL2, WhiskerR0, WhiskerR1, WhiskerR2,
		// Glasses: each rim is a ring of 10 beads, so you can see the eye through it
		RimL0, RimL1, RimL2, RimL3, RimL4, RimL5, RimL6, RimL7, RimL8, RimL9,
		RimR0, RimR1, RimR2, RimR3, RimR4, RimR5, RimR6, RimR7, RimR8, RimR9,
		Bridge,
		UpperArmL, LowerArmL, HandL, UpperArmR, LowerArmR, HandR,
		ThighL, CalfL, FootL, ThighR, CalfR, FootR,
		Tail0, Tail1, Tail2,
		// Obsidian spikes (cones): back ridge, head "volcano peaks", tail
		SpikeBack0, SpikeBack1, SpikeBack2, SpikeBack3,
		SpikeHead0, SpikeHead1, SpikeHead2,
		SpikeTail0, SpikeTail1,
		Count
	};

	enum EColorSlot : int32 { Fur, Lava, Obsidian, Eye, Lens, MuzzleTone, SlotCount };

	const TCHAR* const Names[Count] =
	{
		TEXT("Torso"), TEXT("BellyPatch"), TEXT("Head"), TEXT("Muzzle"), TEXT("Nose"),
		TEXT("EyeL"), TEXT("EyeR"), TEXT("ShineL"), TEXT("ShineR"), TEXT("BrowDotL"), TEXT("BrowDotR"),
		TEXT("EarL"), TEXT("EarR"), TEXT("InnerEarL"), TEXT("InnerEarR"),
		TEXT("WhiskerL0"), TEXT("WhiskerL1"), TEXT("WhiskerL2"), TEXT("WhiskerR0"), TEXT("WhiskerR1"), TEXT("WhiskerR2"),
		TEXT("RimL0"), TEXT("RimL1"), TEXT("RimL2"), TEXT("RimL3"), TEXT("RimL4"),
		TEXT("RimL5"), TEXT("RimL6"), TEXT("RimL7"), TEXT("RimL8"), TEXT("RimL9"),
		TEXT("RimR0"), TEXT("RimR1"), TEXT("RimR2"), TEXT("RimR3"), TEXT("RimR4"),
		TEXT("RimR5"), TEXT("RimR6"), TEXT("RimR7"), TEXT("RimR8"), TEXT("RimR9"),
		TEXT("Bridge"),
		TEXT("UpperArmL"), TEXT("LowerArmL"), TEXT("HandL"), TEXT("UpperArmR"), TEXT("LowerArmR"), TEXT("HandR"),
		TEXT("ThighL"), TEXT("CalfL"), TEXT("FootL"), TEXT("ThighR"), TEXT("CalfR"), TEXT("FootR"),
		TEXT("Tail0"), TEXT("Tail1"), TEXT("Tail2"),
		TEXT("SpikeBack0"), TEXT("SpikeBack1"), TEXT("SpikeBack2"), TEXT("SpikeBack3"),
		TEXT("SpikeHead0"), TEXT("SpikeHead1"), TEXT("SpikeHead2"),
		TEXT("SpikeTail0"), TEXT("SpikeTail1")
	};

	const EColorSlot Colors[Count] =
	{
		Fur, Lava, Fur, MuzzleTone, Obsidian,
		Eye, Eye, Lens, Lens, MuzzleTone, MuzzleTone,
		Fur, Fur, MuzzleTone, MuzzleTone,
		Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian,
		Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian,
		Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian, Obsidian,
		Obsidian,
		Fur, Fur, Fur, Fur, Fur, Fur,
		Fur, Fur, Fur, Fur, Fur, Fur,
		Fur, Fur, Fur,
		Obsidian, Obsidian, Obsidian, Obsidian,
		Obsidian, Obsidian, Obsidian,
		Obsidian, Obsidian
	};

	bool IsSpike(int32 Part) { return Part >= SpikeBack0 && Part < Count; }
	bool IsGlasses(int32 Part) { return Part >= RimL0 && Part <= Bridge; }

	constexpr int32 RimBeads = 10;
}

AVulcanBoss::AVulcanBoss()
{
	PrimaryActorTick.bCanEverTick = true;
	// Run after animation so the otter shapes follow this frame's pose.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	// Stand-in body until the otter exists: the template mannequin. Override in BP_Vulcan.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> StandInMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (StandInMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(StandInMesh.Object);
	}
	static ConstructorHelpers::FClassFinder<UAnimInstance> StandInAnim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (StandInAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(StandInAnim.Class);
	}

	// Mixamo animations retargeted to the mannequin; BuildMontagesFromAnimations turns them into montages.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Roar(TEXT("/Game/RTG_Mutant_Roaring.RTG_Mutant_Roaring"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Jump(TEXT("/Game/RTG_Mutant_Jumping.RTG_Mutant_Jumping"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Land(TEXT("/Game/Player/Animations/RTG_Hard_Landing.RTG_Hard_Landing"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> HitReact(TEXT("/Game/RTG_Zombie_Reaction_Hit.RTG_Zombie_Reaction_Hit"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Death(TEXT("/Game/RTG_Standing_React_Death_Backward.RTG_Standing_React_Death_Backward"));
	RoarAnimation = Roar.Object;
	JumpAnimation = Jump.Object;
	LandAnimation = Land.Object;
	HitReactAnimation = HitReact.Object;
	DeathAnimation = Death.Object;
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	// Squash the skeleton: shorter legs and torso give the otter chunky, chibi proportions.
	GetMesh()->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.7f));

	// Boss-sized, and easy to tell apart from the player mannequin.
	GetCapsuleComponent()->SetRelativeScale3D(FVector(1.6f));

	// Cartoon otter made of stretched spheres + cone spikes; positioned on the mannequin's bones every frame.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BlobMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SpikeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	for (int32 i = 0; i < OtterBody::Count; ++i)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(FName(FString(TEXT("Otter_")) + OtterBody::Names[i]));
		Part->SetupAttachment(GetMesh());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);

		const auto& ShapeMesh = OtterBody::IsSpike(i) ? SpikeMesh : BlobMesh;
		if (ShapeMesh.Succeeded())
		{
			Part->SetStaticMesh(ShapeMesh.Object);
		}
		OtterParts.Add(Part);
	}

	StatueMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatueMesh"));
	StatueMesh->SetupAttachment(GetCapsuleComponent());
	StatueMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatueMesh->SetCanEverAffectNavigation(false);
	StatueMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f)); // feet at the bottom of the capsule

	CoreGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Otter_CoreGlow"));
	CoreGlow->SetupAttachment(GetMesh());
	CoreGlow->SetIntensityUnits(ELightUnits::Candelas);
	CoreGlow->SetIntensity(CoreGlowIntensity);
	CoreGlow->SetLightColor(FLinearColor(1.f, 0.35f, 0.05f));
	CoreGlow->SetAttenuationRadius(500.f);
	CoreGlow->SetCastShadows(false);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 500.f;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);

	ProjectileClass = AVulcanProjectile::StaticClass();
}

void AVulcanBoss::BeginPlay()
{
	Super::BeginPlay();

	BuildMontagesFromAnimations();

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// The mannequin is hidden behind the otter shapes, and a hidden skeletal mesh stops refreshing its
	// bones by default, which froze the otter in one pose. Keep animating and updating bones regardless.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	HealthComponent->OnHealthChanged.AddDynamic(this, &AVulcanBoss::HandleHealthChanged);
	HealthComponent->OnDeath.AddDynamic(this, &AVulcanBoss::HandleDeath);

	RebindOtterParts();

	MeshRestLocation = GetMesh()->GetRelativeLocation();
	StatueRestLocation = StatueMesh->GetRelativeLocation();
	MeshRestRotation = GetMesh()->GetRelativeRotation();
	StatueRestRotation = StatueMesh->GetRelativeRotation();

	if (bStatueIntro)
	{
		// Statue until the player comes close (see Think). Let the idle pose settle, then freeze it.
		IntroState = EIntroState::Statue;
		HealthComponent->bInvulnerable = true;
		ApplyOtterLook();
		GetWorldTimerManager().SetTimer(IntroTimer, this, &AVulcanBoss::FreezeStatuePose, 0.2f, false);
	}
	else if (bStartFightOnBeginPlay)
	{
		StartFight();
	}

	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AVulcanBoss::Think, FMath::Max(ThinkInterval, 0.05f), true);
}

void AVulcanBoss::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Show the statue look in the editor too, so the sculpture model can be lined up.
	IntroState = bStatueIntro ? EIntroState::Statue : EIntroState::Done;

	RebindOtterParts();
	ApplyOtterLook();
	UpdateOtterBody();
}

void AVulcanBoss::RebindOtterParts()
{
	if (OtterParts.Num() == OtterBody::Count && !OtterParts.Contains(nullptr))
	{
		return;
	}

	TArray<UStaticMeshComponent*> Components;
	GetComponents(Components);

	OtterParts.Reset();
	OtterParts.SetNum(OtterBody::Count);
	for (int32 i = 0; i < OtterBody::Count; ++i)
	{
		const FName PartName(FString(TEXT("Otter_")) + OtterBody::Names[i]);
		for (UStaticMeshComponent* Component : Components)
		{
			if (Component && Component->GetFName() == PartName)
			{
				OtterParts[i] = Component;
				break;
			}
		}
	}
}

// ============================================================ Otter body

bool AVulcanBoss::HasStatueModel() const
{
	return StatueMesh && StatueMesh->GetStaticMesh() != nullptr;
}

void AVulcanBoss::FreezeStatuePose()
{
	if (IntroState == EIntroState::Statue)
	{
		GetMesh()->bPauseAnims = true;
	}
}

void AVulcanBoss::AwakenFromStatue()
{
	if (IntroState != EIntroState::Statue)
	{
		return;
	}

	CaptureSky();

	if (bLightningStrike)
	{
		StrikeStatue();
	}
	else
	{
		BeginShaking();
	}
}

void AVulcanBoss::StrikeStatue()
{
	IntroState = EIntroState::Struck;

	const FVector Target = GetActorLocation() + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.8f);
	BuildBolt(Target);
	OnStatueStruck(Target);
	SetQuakeStrength(1.6f); // thunder jolt

	const float StrikeTime = FMath::Max(LightningStrikeTime, 0.05f);
	BoltEndTime = GetWorld()->GetTimeSeconds() + StrikeTime;
	GetWorldTimerManager().SetTimer(BoltTimer, this, &AVulcanBoss::FlickerBolt, 0.05f, true);
	GetWorldTimerManager().SetTimer(IntroTimer, this, &AVulcanBoss::BeginShaking, StrikeTime, false);
}

void AVulcanBoss::BeginShaking()
{
	IntroState = EIntroState::Awakening;
	OnStatueAwakening();

	if (bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, StatueAwakenTime, FColor::Orange, TEXT("The statue begins to crack..."));
	}

	GetWorldTimerManager().SetTimer(IntroTimer, this, &AVulcanBoss::FinishAwakening, FMath::Max(StatueAwakenTime, 0.05f), false);
}

void AVulcanBoss::BuildBolt(const FVector& Target)
{
	ClearBolt();

	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Cylinder)
	{
		return;
	}

	UMaterialInstanceDynamic* BoltMaterial = BaseMaterial ? UMaterialInstanceDynamic::Create(BaseMaterial, this) : nullptr;
	if (BoltMaterial)
	{
		BoltMaterial->SetVectorParameterValue(TEXT("Color"), LightningColor);
		BoltMaterial->SetScalarParameterValue(TEXT("Glow"), 80.f);
		BoltMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		BoltMaterial->SetScalarParameterValue(TEXT("Roughness"), 1.f);
	}

	// Engine cylinder is 100 tall and 100 wide with its pivot in the middle.
	auto AddSegment = [&](const FVector& A, const FVector& B, float Thickness)
	{
		const FVector Dir = B - A;
		const float Length = Dir.Size();
		if (Length < KINDA_SMALL_NUMBER)
		{
			return;
		}
		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
		Segment->SetStaticMesh(Cylinder);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		Segment->SetCanEverAffectNavigation(false);
		if (BoltMaterial)
		{
			Segment->SetMaterial(0, BoltMaterial);
		}
		Segment->RegisterComponent();
		Segment->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(Dir / Length).Rotator(), (A + B) * 0.5f,
			FVector(Thickness, Thickness, Length + Thickness * 0.5f) / 100.f));
		BoltParts.Add(Segment);
	};

	// Jagged main bolt from high in the sky down to the statue.
	const FVector SkyPoint = Target + FVector(FMath::FRandRange(-700.f, 700.f), FMath::FRandRange(-700.f, 700.f), 6000.f);
	constexpr int32 Steps = 18;
	TArray<FVector> Points;
	for (int32 i = 0; i <= Steps; ++i)
	{
		const float T = static_cast<float>(i) / Steps;
		FVector Point = FMath::Lerp(SkyPoint, Target, T);
		if (i > 0 && i < Steps)
		{
			Point += FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 260.f * (1.f - 0.7f * T);
		}
		Points.Add(Point);
	}
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		AddSegment(Points[i - 1], Points[i], 18.f);
	}

	// A few forks splitting off the main bolt.
	for (int32 Fork = 0; Fork < 3; ++Fork)
	{
		const int32 From = FMath::RandRange(2, Steps - 6);
		FVector Prev = Points[From];
		FVector Dir = ((Points[From + 1] - Points[From]).GetSafeNormal() + FMath::VRand() * 0.8f).GetSafeNormal();
		Dir.Z = -FMath::Abs(Dir.Z) - 0.3f;
		for (int32 k = 0; k < 5; ++k)
		{
			const FVector Next = Prev + (Dir + FMath::VRand() * 0.35f).GetSafeNormal() * FMath::FRandRange(180.f, 320.f);
			AddSegment(Prev, Next, 8.f - k);
			Prev = Next;
		}
	}

	BoltFlash = NewObject<UPointLightComponent>(this);
	BoltFlash->SetIntensityUnits(ELightUnits::Candelas);
	BoltFlash->SetLightColor(LightningColor);
	BoltFlash->SetAttenuationRadius(9000.f);
	BoltFlash->SetCastShadows(true);
	BoltFlash->RegisterComponent();
	BoltFlash->SetWorldLocation(Target + FVector(0.f, 0.f, 400.f));
	BoltFlash->SetIntensity(60000.f);
}

void AVulcanBoss::FlickerBolt()
{
	if (GetWorld()->GetTimeSeconds() >= BoltEndTime)
	{
		ClearBolt();
		return;
	}

	// Real lightning strobes a few times before it fades.
	const bool bOn = FMath::FRand() > 0.3f;
	for (UStaticMeshComponent* Segment : BoltParts)
	{
		if (Segment)
		{
			Segment->SetVisibility(bOn);
		}
	}
	if (BoltFlash)
	{
		BoltFlash->SetIntensity(bOn ? 60000.f * FMath::FRandRange(0.5f, 1.f) : 0.f);
	}
}

void AVulcanBoss::ClearBolt()
{
	GetWorldTimerManager().ClearTimer(BoltTimer);
	for (UStaticMeshComponent* Segment : BoltParts)
	{
		if (Segment)
		{
			Segment->DestroyComponent();
		}
	}
	BoltParts.Reset();
	if (BoltFlash)
	{
		BoltFlash->DestroyComponent();
		BoltFlash = nullptr;
	}
}

// ============================================================ Storm sky

void AVulcanBoss::CaptureSky()
{
	if (!bStormSky || bSkyCaptured)
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (UDirectionalLightComponent* Light = It->FindComponentByClass<UDirectionalLightComponent>())
		{
			// Prefer the light that drives the sky (the sun).
			if (!Sun.IsValid() || Light->IsUsedAsAtmosphereSunLight())
			{
				Sun = Light;
			}
		}
		if (!Atmosphere.IsValid()) { Atmosphere = It->FindComponentByClass<USkyAtmosphereComponent>(); }
		if (!Clouds.IsValid())     { Clouds = It->FindComponentByClass<UVolumetricCloudComponent>(); }
		if (!Fog.IsValid())        { Fog = It->FindComponentByClass<UExponentialHeightFogComponent>(); }
	}

	if (Sun.IsValid())
	{
		SunStartColor = Sun->GetLightColor();
		SunStartIntensity = Sun->Intensity;
		SunStartRotation = Sun->GetComponentRotation();
	}

	// Where the sun ends up. A directional light shines along its forward vector, so the sun
	// itself sits in the opposite direction: face the light away from the spot it should appear.
	SunTargetRotation = FRotator(StormSunPitch, SunStartRotation.Yaw, 0.f);
	if (bSunBehindVulcan)
	{
		if (const APawn* Player = GetPlayer())
		{
			const FVector PlayerToVulcan = (GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
			if (!PlayerToVulcan.IsNearlyZero())
			{
				SunTargetRotation.Yaw = PlayerToVulcan.Rotation().Yaw + 180.f + SunSideOffset;
			}
		}
	}
	if (Atmosphere.IsValid())
	{
		SkyStartTint = Atmosphere->SkyLuminanceFactor;
	}
	if (Fog.IsValid())
	{
		FogStartColor = Fog->FogInscatteringLuminance;
		FogStartDensity = Fog->FogDensity;
	}
	UMaterialInterface* CloudBase = Clouds.IsValid() ? Clouds->Material.LoadSynchronous() : nullptr;
	if (CloudBase)
	{
		CloudMaterial = UMaterialInstanceDynamic::Create(CloudBase, this);
		Clouds->SetMaterial(CloudMaterial);
		CloudMaterial->GetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), CloudStartCoverage);
		CloudMaterial->GetScalarParameterValue(TEXT("Cloud_GlobalDensity"), CloudStartDensity);
		CloudMaterial->GetVectorParameterValue(TEXT("Cloud_AlbedoColor"), CloudStartAlbedo);
		CloudMaterial->GetVectorParameterValue(TEXT("Storm_AlbedoColor"), CloudStartStormAlbedo);
	}

	bSkyCaptured = true;
}

void AVulcanBoss::ApplyStorm(float Alpha)
{
	if (!bStormSky || !bSkyCaptured)
	{
		return;
	}

	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	StormAlpha = A;

	// Blood rain starts once the sky has mostly turned red, full by the time Vulcan wakes.
	for (TActorIterator<ADriftParticles> It(GetWorld()); It; ++It)
	{
		if (It->bWaitForStorm)
		{
			It->Intensity = FMath::Clamp((A - 0.65f) / 0.35f, 0.f, 1.f);
		}
	}

	if (Sun.IsValid())
	{
		Sun->SetLightColor(FMath::Lerp(SunStartColor, BloodSunColor, A));
		Sun->SetIntensity(FMath::Lerp(SunStartIntensity, BloodSunIntensity, A));
		Sun->SetWorldRotation(FQuat::Slerp(SunStartRotation.Quaternion(), SunTargetRotation.Quaternion(), A));
	}
	if (Atmosphere.IsValid())
	{
		Atmosphere->SetSkyLuminanceFactor(FMath::Lerp(SkyStartTint, StormSkyTint, A));
	}
	if (Fog.IsValid())
	{
		Fog->SetFogInscatteringColor(FMath::Lerp(FogStartColor, StormFogColor, A));
		Fog->SetFogDensity(FMath::Lerp(FogStartDensity, StormFogDensity, A));
	}
	if (CloudMaterial)
	{
		// Clouds keep their alpha channel (it holds a material setting, not opacity).
		FLinearColor Albedo = FMath::Lerp(CloudStartAlbedo, StormCloudColor, A);
		Albedo.A = CloudStartAlbedo.A;
		FLinearColor StormAlbedo = FMath::Lerp(CloudStartStormAlbedo, StormCloudColor, A);
		StormAlbedo.A = CloudStartStormAlbedo.A;

		CloudMaterial->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), FMath::Lerp(CloudStartCoverage, StormCloudCoverage, A));
		CloudMaterial->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), FMath::Lerp(CloudStartDensity, StormCloudDensity, A));
		CloudMaterial->SetVectorParameterValue(TEXT("Cloud_AlbedoColor"), Albedo);
		CloudMaterial->SetVectorParameterValue(TEXT("Storm_AlbedoColor"), StormAlbedo);
	}
}

void AVulcanBoss::SetQuakeStrength(float Scale)
{
	if (!bEarthquake)
	{
		return;
	}
	if (!Quake.IsValid())
	{
		if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			Quake = Camera->StartCameraShake(UEarthquakeCameraShake::StaticClass(), 1.f);
		}
	}
	if (Quake.IsValid())
	{
		Quake->ShakeScale = Scale * EarthquakeStrength;
	}
}

void AVulcanBoss::StopQuake()
{
	if (Quake.IsValid())
	{
		if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			Camera->StopCameraShake(Quake.Get(), false); // blend out
		}
	}
	Quake = nullptr;
}

void AVulcanBoss::UpdateBloodSun()
{
	if (!bStormSky || BloodSunSize <= 0.f)
	{
		return;
	}

	APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Camera)
	{
		return;
	}

	if (!BloodSunDisk)
	{
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sphere || !BaseMaterial)
		{
			return;
		}
		BloodSunMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		BloodSunMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		BloodSunMaterial->SetScalarParameterValue(TEXT("Roughness"), 1.f);

		BloodSunDisk = NewObject<UStaticMeshComponent>(this);
		BloodSunDisk->SetStaticMesh(Sphere);
		BloodSunDisk->SetMaterial(0, BloodSunMaterial);
		BloodSunDisk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BloodSunDisk->SetCastShadow(false);
		BloodSunDisk->SetCanEverAffectNavigation(false);
		BloodSunDisk->SetAbsolute(true, true, true);
		BloodSunDisk->RegisterComponent();
	}

	// The sun sits opposite the direction the light shines.
	const FVector SunDirection = Sun.IsValid() ? -Sun->GetForwardVector() : -SunTargetRotation.Vector();
	constexpr float Distance = 40000.f; // far beyond the colosseum, in front of the clouds
	const float Diameter = 2.f * Distance * FMath::Tan(FMath::DegreesToRadians(BloodSunSize * 0.5f));

	BloodSunDisk->SetWorldLocation(Camera->GetCameraLocation() + SunDirection * Distance);
	BloodSunDisk->SetWorldScale3D(FVector(Diameter / 100.f));
	BloodSunMaterial->SetVectorParameterValue(TEXT("Color"), BloodSunColor);
	BloodSunMaterial->SetScalarParameterValue(TEXT("Glow"), BloodSunGlow * StormAlpha * StormAlpha);
	BloodSunDisk->SetVisibility(StormAlpha > 0.02f);
}

void AVulcanBoss::FinishAwakening()
{
	IntroState = EIntroState::Done;
	ApplyStorm(1.f);

	GetMesh()->bPauseAnims = false;
	GetMesh()->SetRelativeLocationAndRotation(MeshRestLocation, MeshRestRotation);
	StatueMesh->SetRelativeLocationAndRotation(StatueRestLocation, StatueRestRotation);
	StopQuake();
	HealthComponent->bInvulnerable = false;

	ApplyOtterLook();
	OnStatueTransformed();

	StartFight();
	NextAttackTime = GetWorld()->GetTimeSeconds() + 1.f; // a beat to react before the first attack

	// Roar as the fight begins, standing still for it.
	const float RoarTime = PlayMontageScaled(RoarMontage);
	if (RoarTime > 0.f)
	{
		GetCharacterMovement()->DisableMovement();
		NextAttackTime = GetWorld()->GetTimeSeconds() + RoarTime;
		GetWorldTimerManager().SetTimer(RoarTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!bDead)
			{
				GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
		}), RoarTime, false);
	}
}

void AVulcanBoss::ApplyOtterLook()
{
	const bool bStatue = IntroState != EIntroState::Done;
	const bool bStatueModel = bStatue && HasStatueModel();
	const bool bShowOtter = bUseOtterBody && !bStatueModel;

	StatueMesh->SetVisibility(bStatueModel);
	GetMesh()->SetVisibility(!bUseOtterBody && !bStatueModel, false);

	// M_VulcanShape (made by Tools/make_vulcan_material.py) adds Metallic/Roughness/Glow.
	// Fall back to the engine's plain shape material if it hasn't been created.
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vulcan/M_VulcanShape.M_VulcanShape"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	// When Vulcan dies the lava cools to black rock. As a statue, everything is bronze.
	const FLinearColor CooledLava = FLinearColor(FColor(30, 26, 26));
	const FLinearColor DarkBronze = BronzeColor * 0.55f;
	const FLinearColor SlotColors[OtterBody::SlotCount] =
	{
		bStatue ? BronzeColor : FurColor,
		bStatue ? BronzeColor : (bDead ? CooledLava : LavaColor),
		bStatue ? DarkBronze : ObsidianColor,
		bStatue ? DarkBronze : EyeColor,
		bStatue ? BronzeColor : LensColor,
		bStatue ? BronzeColor : MuzzleColor
	};

	// Shine per slot: metallic fur, glowing matte lava, glassy obsidian and eyes, glowing highlights.
	// A statue is all polished bronze.
	const float SlotMetallic[OtterBody::SlotCount] = { BodyMetallic, 0.f, 0.7f, 0.3f, 0.f, BodyMetallic * 0.7f };
	const float SlotRoughness[OtterBody::SlotCount] = { BodyRoughness, 0.5f, 0.08f, 0.05f, 0.3f, FMath::Min(BodyRoughness + 0.1f, 1.f) };
	const float SlotGlow[OtterBody::SlotCount] = { 0.f, bDead ? 0.f : LavaGlow, 0.f, 0.f, 0.6f, 0.f };

	OtterMaterials.SetNum(OtterBody::SlotCount);
	for (int32 Slot = 0; Slot < OtterBody::SlotCount; ++Slot)
	{
		if (BaseMaterial && (!OtterMaterials[Slot] || OtterMaterials[Slot]->Parent != BaseMaterial))
		{
			OtterMaterials[Slot] = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		}
		if (UMaterialInstanceDynamic* Material = OtterMaterials[Slot])
		{
			Material->SetVectorParameterValue(TEXT("Color"), SlotColors[Slot]);
			Material->SetScalarParameterValue(TEXT("Metallic"), bStatue ? 1.f : SlotMetallic[Slot]);
			Material->SetScalarParameterValue(TEXT("Roughness"), bStatue ? 0.35f : SlotRoughness[Slot]);
			Material->SetScalarParameterValue(TEXT("Glow"), bStatue ? 0.f : SlotGlow[Slot]);
		}
	}

	for (int32 i = 0; i < OtterParts.Num() && i < OtterBody::Count; ++i)
	{
		UStaticMeshComponent* Part = OtterParts[i];
		if (!Part)
		{
			continue;
		}

		Part->SetVisibility(bShowOtter && (bShowGlasses || !OtterBody::IsGlasses(i)) && (bShowSpikes || !OtterBody::IsSpike(i)));
		Part->SetMaterial(0, OtterMaterials[OtterBody::Colors[i]]);
	}

	if (CoreGlow)
	{
		CoreGlow->SetVisibility(bShowOtter && !bDead && !bStatue);
	}
}

void AVulcanBoss::UpdateOtterBody()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!bUseOtterBody || !Body || OtterParts.Num() != OtterBody::Count || OtterParts.Contains(nullptr))
	{
		return;
	}
	if (IntroState != EIntroState::Done && HasStatueModel())
	{
		return; // the sculpture is showing instead
	}

	using namespace OtterBody;

	// Thickness follows the horizontal scale (the skeleton is squashed vertically on purpose).
	const float S = Body->GetComponentScale().X;
	const FVector Up = FVector::UpVector;
	const FVector Fwd = GetActorForwardVector();
	const FVector Right = GetActorRightVector();

	auto Bone = [Body](const TCHAR* Name) { return Body->GetSocketLocation(FName(Name)); };

	// Engine sphere is 100 units across, so scale = size / 100. Sizes are in cm before boss scaling.
	auto Blob = [this, S](int32 Part, const FVector& Center, const FMatrix& Axes, const FVector& SizeCm)
	{
		OtterParts[Part]->SetWorldTransform(FTransform(Axes.Rotator(), Center, SizeCm * S / 100.f));
	};

	// Stretched sphere running from A to B, like a sausage.
	auto Limb = [this, S, Fwd, Up](int32 Part, const FVector& A, const FVector& B, float RadiusCm)
	{
		const FVector Dir = B - A;
		const float Length = Dir.Size();
		const FVector Along = Length > KINDA_SMALL_NUMBER ? Dir / Length : Up;
		const FVector Secondary = FMath::Abs(FVector::DotProduct(Along, Fwd)) > 0.95f ? Up : Fwd;
		const float Diameter = RadiusCm * 2.f * S;
		const FVector Scale = FVector(Diameter, Diameter, Length + Diameter * 0.8f) / 100.f;
		OtterParts[Part]->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZX(Along, Secondary).Rotator(), (A + B) * 0.5f, Scale));
	};

	// Cone with its tip pointing along Direction. Engine cone is 100 tall and 100 wide, pivot at its center.
	auto Spike = [this, S, Fwd, Up](int32 Part, const FVector& Base, const FVector& Direction, float LengthCm, float WidthCm)
	{
		const FVector Along = Direction.GetSafeNormal();
		const FVector Secondary = FMath::Abs(FVector::DotProduct(Along, Fwd)) > 0.95f ? Up : Fwd;
		const FVector Center = Base + Along * LengthCm * 0.5f * S;
		OtterParts[Part]->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZX(Along, Secondary).Rotator(), Center, FVector(WidthCm, WidthCm, LengthCm) * S / 100.f));
	};

	// ---- Torso: pear-shaped, molten lava belly in front
	const FVector Pelvis = Bone(TEXT("pelvis"));
	const FVector Spine = Bone(TEXT("spine_03"));
	const FVector NeckBone = Bone(TEXT("neck_01"));
	const FVector HeadBone = Bone(TEXT("head"));

	// One smooth body from below the hips up into the head (torso, chest and neck in one piece).
	const FVector TorsoAxis = (NeckBone - Pelvis).GetSafeNormal();
	const FMatrix TorsoAxes = FRotationMatrix::MakeFromZX(TorsoAxis.IsNearlyZero() ? Up : TorsoAxis, Fwd);
	const float TorsoSpan = FVector::Dist(Pelvis, NeckBone);
	const FVector TorsoCenter = FMath::Lerp(Pelvis, NeckBone, 0.45f);
	const float TorsoDepthCm = 48.f;
	const float TorsoHalfLength = (TorsoSpan + 45.f * S) * 0.5f;

	OtterParts[Torso]->SetWorldTransform(FTransform(TorsoAxes.Rotator(), TorsoCenter,
		FVector(TorsoDepthCm * S, 56.f * S, TorsoHalfLength * 2.f) / 100.f));
	Blob(BellyPatch, FMath::Lerp(Pelvis, NeckBone, 0.3f) + TorsoAxes.GetScaledAxis(EAxis::X) * 15.f * S, TorsoAxes, FVector(26.f, 42.f, 56.f));

	// How far the back surface is from the spine at a point along the torso (it's an ellipsoid).
	auto TorsoBackDepth = [&](const FVector& Point)
	{
		const float AlongAxis = FVector::DotProduct(Point - TorsoCenter, TorsoAxes.GetScaledAxis(EAxis::Z));
		const float Fraction = FMath::Clamp(AlongAxis / TorsoHalfLength, -1.f, 1.f);
		return TorsoDepthCm * 0.5f * S * FMath::Sqrt(1.f - Fraction * Fraction);
	};

	// ---- Head: round, snout, lava mouth, round glasses with calm dot eyes behind them
	const FMatrix HeadAxes = FRotationMatrix::MakeFromZX(HeadBone - NeckBone, Fwd);
	const FVector HF = HeadAxes.GetScaledAxis(EAxis::X);
	const FVector HR = HeadAxes.GetScaledAxis(EAxis::Y);
	const FVector HU = HeadAxes.GetScaledAxis(EAxis::Z);
	// Big chibi head (radii 30 deep, 34 wide, 29 tall). Every face feature is placed on its surface.
	// Head-local sizes below are multiplied by HeadScale (K), so the whole head grows as one piece.
	const float K = FMath::Max(HeadScale, 0.01f);
	const FVector HeadRadii(30.f, 34.f, 29.f);
	const FVector HeadCenter = HeadBone + (HU * 22.f + HF * 2.f) * S * K;

	// Distance from head center to the face surface at a given sideways/up offset (cm).
	auto FaceDepth = [&HeadRadii](float SideCm, float UpCm)
	{
		const float Inside = 1.f - FMath::Square(SideCm / HeadRadii.Y) - FMath::Square(UpCm / HeadRadii.Z);
		return HeadRadii.X * FMath::Sqrt(FMath::Max(Inside, 0.f));
	};
	auto OnFace = [&](float SideCm, float UpCm, float OutCm)
	{
		return HeadCenter + (HR * SideCm + HU * UpCm + HF * (FaceDepth(SideCm, UpCm) + OutCm)) * S * K;
	};
	auto HeadBlob = [&](int32 Part, const FVector& Center, const FMatrix& Axes, const FVector& SizeCm)
	{
		Blob(Part, Center, Axes, SizeCm * K);
	};

	HeadBlob(Head, HeadCenter, HeadAxes, HeadRadii * 2.f);
	// Muzzle: a wide cream bulge over the lower face and cheeks, like the reference.
	HeadBlob(Muzzle, OnFace(0.f, -12.f, -14.f), HeadAxes, FVector(32.f, 54.f, 30.f));
	HeadBlob(Nose, OnFace(0.f, -4.f, 2.f), HeadAxes, FVector(6.f, 9.f, 6.f));
	HeadBlob(Bridge, OnFace(0.f, 4.f, 0.9f), HeadAxes, FVector(1.6f, 6.f, 1.6f));

	const float EyeSide = 12.f;
	const float EyeUp = 3.f;
	const float RimRadius = 9.f;

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Sign = Side == 0 ? -1.f : 1.f; // left side is -Right

		// Big glossy eyes with a white catchlight.
		const FVector EyeCenter = OnFace(EyeSide * Sign, EyeUp, 0.4f);
		HeadBlob(Side == 0 ? EyeL : EyeR, EyeCenter, HeadAxes, FVector(3.f, 9.f, 11.f));
		HeadBlob(Side == 0 ? ShineL : ShineR, EyeCenter + (HF * 1.4f + HU * 3.f - HR * 2.f) * S * K, HeadAxes, FVector(1.f, 2.8f, 3.2f));

		// Round glasses: a ring of beads hugging the face around each eye (see-through).
		for (int32 b = 0; b < RimBeads; ++b)
		{
			const float Angle = 2.f * PI * b / RimBeads;
			const float BeadSide = EyeSide * Sign + RimRadius * FMath::Cos(Angle);
			const float BeadUp = EyeUp + RimRadius * FMath::Sin(Angle);
			const FVector Tangent = (-HR * FMath::Sin(Angle) + HU * FMath::Cos(Angle)).GetSafeNormal();
			const FMatrix BeadAxes = FRotationMatrix::MakeFromZX(Tangent, HF);
			const float BeadLength = 2.f * PI * RimRadius / RimBeads + 1.5f; // overlap neighbours into a smooth ring
			HeadBlob((Side == 0 ? RimL0 : RimR0) + b, OnFace(BeadSide, BeadUp, 0.9f), BeadAxes, FVector(1.8f, 1.8f, BeadLength));
		}

		// Little eyebrow dots.
		HeadBlob(Side == 0 ? BrowDotL : BrowDotR, OnFace(10.f * Sign, 15.f, 0.2f), HeadAxes, FVector(1.5f, 4.5f, 2.8f));

		// Round ears with a light inside, up on the top corners of the head.
		const FVector EarCenter = HeadCenter + (HR * 23.f * Sign + HU * 19.f - HF * 3.f) * S * K;
		HeadBlob(Side == 0 ? EarL : EarR, EarCenter, HeadAxes, FVector(8.f, 15.f, 15.f));
		HeadBlob(Side == 0 ? InnerEarL : InnerEarR, EarCenter + HF * 3.5f * S * K, HeadAxes, FVector(2.f, 9.f, 9.f));

		// Three whiskers fanning out from each cheek.
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector Root = OnFace(15.f * Sign, -9.f - k * 2.5f, -0.5f);
			const FVector Tip = Root + (HR * 17.f * Sign - HF * 2.f + HU * (4.f - k * 4.f)) * S * K;
			Limb((Side == 0 ? WhiskerL0 : WhiskerR0) + k, Root, Tip, 0.35f * K);
		}
	}

	// ---- Arms (stubby)
	const FVector UpperArm[2] = { Bone(TEXT("upperarm_l")), Bone(TEXT("upperarm_r")) };
	const FVector LowerArm[2] = { Bone(TEXT("lowerarm_l")), Bone(TEXT("lowerarm_r")) };
	const FVector Hand[2] = { Bone(TEXT("hand_l")), Bone(TEXT("hand_r")) };

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FVector ForearmDir = (Hand[Side] - LowerArm[Side]).GetSafeNormal();
		const FVector PawCenter = Hand[Side] + ForearmDir * 3.f * S;
		const FMatrix PawAxes = FRotationMatrix::MakeFromZX(ForearmDir.IsNearlyZero() ? Up : ForearmDir, Fwd);

		Limb(Side == 0 ? UpperArmL : UpperArmR, UpperArm[Side], LowerArm[Side], 9.f);
		Limb(Side == 0 ? LowerArmL : LowerArmR, LowerArm[Side], Hand[Side], 8.f);
		Blob(Side == 0 ? HandL : HandR, PawCenter, PawAxes, FVector(14.f, 14.f, 15.f));
	}

	// ---- Legs (short and thick), flat feet
	const FVector Thigh[2] = { Bone(TEXT("thigh_l")), Bone(TEXT("thigh_r")) };
	const FVector Calf[2] = { Bone(TEXT("calf_l")), Bone(TEXT("calf_r")) };
	const FVector Foot[2] = { Bone(TEXT("foot_l")), Bone(TEXT("foot_r")) };
	const FVector Ball[2] = { Bone(TEXT("ball_l")), Bone(TEXT("ball_r")) };

	for (int32 Side = 0; Side < 2; ++Side)
	{
		Limb(Side == 0 ? ThighL : ThighR, Thigh[Side], Calf[Side], 14.f);
		Limb(Side == 0 ? CalfL : CalfR, Calf[Side], Foot[Side], 11.f);

		FVector FootDir = Ball[Side] - Foot[Side];
		FootDir.Z = 0.f;
		const FMatrix FootAxes = FRotationMatrix::MakeFromXZ(FootDir.IsNearlyZero() ? Fwd : FootDir, Up);
		Blob(Side == 0 ? FootL : FootR, FMath::Lerp(Foot[Side], Ball[Side], 0.6f), FootAxes, FVector(26.f, 16.f, 11.f));
	}

	// ---- Tail: drapes to the ground behind and curls to the side. Lashes during Tail Lash.
	const float GroundZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const bool bLashing = CurrentAttack == EVulcanAttack::TailLash;
	const float SwayDegrees = bLashing ? FMath::Sin(OtterTime * 14.f) * 50.f : FMath::Sin(OtterTime * 2.5f) * 10.f;
	const FQuat Sway(Up, FMath::DegreesToRadians(SwayDegrees));
	const FVector TailRoot = Pelvis - (Fwd * 18.f + Up * 4.f) * S;

	auto TailPoint = [&](float BackCm, float SideCm, float Z)
	{
		FVector Point = TailRoot + Sway.RotateVector(-Fwd * BackCm * S + Right * SideCm * S);
		Point.Z = Z;
		return Point;
	};

	const FVector T1 = TailPoint(26.f, 0.f, FMath::Lerp(TailRoot.Z, GroundZ, 0.55f));
	const FVector T2 = TailPoint(48.f, 6.f, GroundZ + 6.f * S);
	const FVector T3 = TailPoint(56.f, 26.f, GroundZ + 4.f * S);

	Limb(Tail0, TailRoot, T1, 12.f);
	Limb(Tail1, T1, T2, 9.5f);
	Limb(Tail2, T2, T3, 7.f);

	// ---- Obsidian spikes: a ridge down the back, three volcano peaks on the head, two on the tail
	const FVector BackLean = (-Fwd * 0.75f + Up * 0.65f);
	const float BackT[4] = { 0.1f, 0.4f, 0.7f, 0.95f };
	const float BackLength[4] = { 17.f, 24.f, 26.f, 19.f };
	const float BackWidth[4] = { 11.f, 14.f, 15.f, 12.f };
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector OnSpine = FMath::Lerp(Pelvis, NeckBone, BackT[k]);
		const FVector Base = OnSpine - TorsoAxes.GetScaledAxis(EAxis::X) * (TorsoBackDepth(OnSpine) - 2.f * S);
		Spike(SpikeBack0 + k, Base, BackLean, BackLength[k], BackWidth[k]);
	}

	const float PeakSide[3] = { -9.f, 0.f, 9.f };
	const float PeakLength[3] = { 13.f, 19.f, 13.f };
	const float PeakWidth[3] = { 9.f, 12.f, 9.f };
	for (int32 k = 0; k < 3; ++k)
	{
		const FVector Base = HeadCenter + (HU * 25.f - HF * 3.f + HR * PeakSide[k]) * S * K;
		Spike(SpikeHead0 + k, Base, HU + HR * (PeakSide[k] * 0.05f) - HF * 0.3f, PeakLength[k] * K, PeakWidth[k] * K);
	}

	const FVector TailLean = Up - Sway.RotateVector(Fwd) * 0.4f;
	Spike(SpikeTail0, FMath::Lerp(TailRoot, T1, 0.6f) + Up * 9.f * S, TailLean, 14.f, 10.f);
	Spike(SpikeTail1, FMath::Lerp(T1, T2, 0.5f) + Up * 7.f * S, TailLean, 11.f, 8.f);

	// ---- Lava glow, flickering, out in front so it lights the floor rather than blowing out the body.
	if (CoreGlow)
	{
		const float Flicker = 0.8f + 0.15f * FMath::Sin(OtterTime * 9.f) + 0.08f * FMath::Sin(OtterTime * 23.f);
		CoreGlow->SetWorldLocation(FMath::Lerp(Pelvis, Spine, 0.5f) + Fwd * 90.f * S);
		CoreGlow->SetIntensity(CoreGlowIntensity * (bPhaseTwo ? 2.f : 1.f) * Flicker);
	}
}

void AVulcanBoss::StartFight()
{
	if (IntroState == EIntroState::Statue)
	{
		AwakenFromStatue(); // the fight starts once the transformation finishes
		return;
	}
	if (bFightActive || bDead || IntroState != EIntroState::Done)
	{
		return;
	}

	bFightActive = true;
	OnFightStarted();
}

// ============================================================ Decision making

void AVulcanBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IntroState == EIntroState::Done)
	{
		OtterTime += DeltaSeconds; // statues don't sway
	}
	else if (IntroState == EIntroState::Awakening)
	{
		// Shake harder and harder until it breaks free.
		const float Progress = 1.f - GetWorldTimerManager().GetTimerRemaining(IntroTimer) / FMath::Max(StatueAwakenTime, 0.05f);
		// Earthquake: slow rolling lurches plus a fine tremor, building to the moment it breaks free.
		const float Strength = FMath::Clamp(Progress, 0.25f, 1.f);
		const float T = GetWorld()->GetTimeSeconds();
		const FVector Lurch(FMath::PerlinNoise1D(T * 6.f), FMath::PerlinNoise1D(T * 7.f + 17.f), 0.4f * FMath::PerlinNoise1D(T * 9.f + 31.f));
		const FVector Jitter = (Lurch * 2.f + FMath::VRand() * 0.25f) * AwakenShake * Strength;
		const FRotator Wobble(FMath::PerlinNoise1D(T * 5.f + 3.f) * 2.f, FMath::PerlinNoise1D(T * 4.f + 11.f), FMath::PerlinNoise1D(T * 6.f + 7.f) * 2.f);
		const FRotator Rock = Wobble * (AwakenWobble * Strength);
		GetMesh()->SetRelativeLocationAndRotation(MeshRestLocation + Jitter, MeshRestRotation + Rock);
		StatueMesh->SetRelativeLocationAndRotation(StatueRestLocation + Jitter, StatueRestRotation + Rock);
		SetQuakeStrength(FMath::Lerp(0.6f, 1.8f, Progress));

		// The storm rolls in while it shakes, fully dark by the time it comes alive.
		ApplyStorm(FMath::SmoothStep(0.f, 1.f, Progress));
	}

	if (StormAlpha > 0.f)
	{
		UpdateBloodSun();
	}

	UpdateOtterBody();

	if (!bDirectChase || bDead || CurrentAttack != EVulcanAttack::None)
	{
		return;
	}

	if (APawn* Player = GetPlayer())
	{
		FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.f;
		AddMovementInput(ToPlayer.GetSafeNormal());
	}
}

void AVulcanBoss::Think()
{
	bDirectChase = false;

	// Statue intro: wake up when the player comes close.
	if (IntroState == EIntroState::Statue)
	{
		const APawn* Player = GetPlayer();
		if (Player && FVector::Dist2D(GetActorLocation(), Player->GetActorLocation()) <= IntroTriggerRange)
		{
			AwakenFromStatue();
		}
		return;
	}

	if (bDead || !bFightActive || CurrentAttack != EVulcanAttack::None)
	{
		return;
	}

	APawn* Player = GetPlayer();
	AAIController* AI = Cast<AAIController>(GetController());
	if (!Player)
	{
		return;
	}

	// Stop once the player is dead.
	if (const UHealthComponent* PlayerHealth = Player->FindComponentByClass<UHealthComponent>())
	{
		if (PlayerHealth->IsDead())
		{
			if (AI) { AI->StopMovement(); }
			return;
		}
	}

	const float Distance = FVector::Dist2D(GetActorLocation(), Player->GetActorLocation());
	if (Distance > AggroRange)
	{
		if (AI) { AI->StopMovement(); }
		return;
	}

	if (GetWorld()->GetTimeSeconds() >= NextAttackTime && TryStartAttack(Distance))
	{
		return;
	}

	if (Distance > MeleeRange * 0.8f)
	{
		// Use the nav mesh if the level has one; otherwise walk straight at the player.
		const bool bPathing = AI && AI->MoveToActor(Player, MeleeRange * 0.6f) != EPathFollowingRequestResult::Failed;
		bDirectChase = !bPathing;
	}
}

bool AVulcanBoss::TryStartAttack(float DistanceToPlayer)
{
	struct FOption
	{
		EVulcanAttack Attack;
		float Weight;
	};

	TArray<FOption> Options;
	auto AddOption = [&Options](bool bEnabled, EVulcanAttack Attack, float Weight)
	{
		if (bEnabled)
		{
			Options.Add(FOption{ Attack, Weight });
		}
	};

	const float EffectiveBreathRange = BreathRange + (bPhaseTwo ? PhaseTwoBreathExtraRange : 0.f);
	float ChaseWeight = 0.f;

	if (DistanceToPlayer <= MeleeRange)
	{
		AddOption(bEnableTailLash, EVulcanAttack::TailLash, 6.f);
		AddOption(bEnableBreath, EVulcanAttack::MoltenBreath, 3.f);
	}
	else if (DistanceToPlayer <= EffectiveBreathRange * 0.9f)
	{
		AddOption(bEnableBreath, EVulcanAttack::MoltenBreath, 4.f);
		AddOption(bEnableSpit, EVulcanAttack::ObsidianSpit, 2.f);
		ChaseWeight = 3.f;
	}
	else
	{
		AddOption(bEnableSpit, EVulcanAttack::ObsidianSpit, 4.f);
		AddOption(bEnableDive, EVulcanAttack::MagmaDive, 3.f);
		ChaseWeight = 3.f;
	}

	float TotalWeight = ChaseWeight;
	for (const FOption& Option : Options)
	{
		TotalWeight += Option.Weight;
	}

	EVulcanAttack Chosen = EVulcanAttack::None;
	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const FOption& Option : Options)
	{
		if (Roll < Option.Weight)
		{
			Chosen = Option.Attack;
			break;
		}
		Roll -= Option.Weight;
	}

	if (Chosen == EVulcanAttack::None)
	{
		// Rolled "chase": walk toward the player for a bit before rolling again.
		NextAttackTime = GetWorld()->GetTimeSeconds() + 1.0f;
		return false;
	}

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}

	CurrentAttack = Chosen;
	FacePlayer();

	if (bShowDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange,
			FString::Printf(TEXT("Vulcan: %s"), *UEnum::GetDisplayValueAsText(Chosen).ToString()));
	}

	switch (Chosen)
	{
	case EVulcanAttack::TailLash:     StartTailLash(); break;
	case EVulcanAttack::ObsidianSpit: StartSpit();     break;
	case EVulcanAttack::MoltenBreath: StartBreath();   break;
	case EVulcanAttack::MagmaDive:    StartDive();     break;
	default: break;
	}

	return true;
}

void AVulcanBoss::FinishAttack()
{
	CurrentAttack = EVulcanAttack::None;
	NextAttackTime = GetWorld()->GetTimeSeconds() + AttackCooldown / GetSpeedScale();
}

// ============================================================ Tail Lash

void AVulcanBoss::StartTailLash()
{
	PlayMontageScaled(TailLashMontage);
	Schedule(AttackTimer, &AVulcanBoss::TailLashHit, TailLashHitDelay / GetSpeedScale());
}

void AVulcanBoss::TailLashHit()
{
	const float HalfArc = TailLashArcDegrees * 0.5f;

	if (bShowDebug)
	{
		const float HalfArcRad = FMath::DegreesToRadians(HalfArc);
		DrawDebugCone(GetWorld(), GetActorLocation(), GetActorForwardVector(), TailLashRange, HalfArcRad, FMath::DegreesToRadians(10.f), 16, FColor::Orange, false, 0.5f);
	}

	if (IsPlayerInCone(TailLashRange, HalfArc))
	{
		DamagePlayer(TailLashDamage);
	}

	Schedule(AttackTimer, &AVulcanBoss::FinishAttack, (TailLashDuration - TailLashHitDelay) / GetSpeedScale());
}

// ============================================================ Obsidian Spit

void AVulcanBoss::StartSpit()
{
	PlayMontageScaled(SpitMontage);
	Schedule(AttackTimer, &AVulcanBoss::SpitFire, SpitFireDelay / GetSpeedScale());
}

void AVulcanBoss::SpitFire()
{
	APawn* Player = GetPlayer();
	if (Player && ProjectileClass)
	{
		FacePlayer();

		const FVector Muzzle = GetActorTransform().TransformPosition(SpitMuzzleOffset);
		const FRotator BaseRotation = (Player->GetActorLocation() - Muzzle).Rotation();
		const int32 Count = FMath::Max(1, bPhaseTwo ? PhaseTwoSpitShardCount : SpitShardCount);

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		for (int32 i = 0; i < Count; ++i)
		{
			const float YawOffset = Count > 1
				? FMath::Lerp(-SpitSpreadDegrees, SpitSpreadDegrees, static_cast<float>(i) / (Count - 1))
				: 0.f;

			const FRotator ShotRotation = BaseRotation + FRotator(0.f, YawOffset, 0.f);
			if (AVulcanProjectile* Shard = GetWorld()->SpawnActor<AVulcanProjectile>(ProjectileClass, Muzzle, ShotRotation, Params))
			{
				if (bPhaseTwo)
				{
					Shard->Damage *= PhaseTwoDamageMultiplier;
				}
			}
		}
	}

	Schedule(AttackTimer, &AVulcanBoss::FinishAttack, (SpitDuration - SpitFireDelay) / GetSpeedScale());
}

// ============================================================ Molten Breath (flamethrower)

void AVulcanBoss::StartBreath()
{
	PlayMontageScaled(BreathMontage);
	OnBreathWindup();
	Schedule(AttackTimer, &AVulcanBoss::BeginBreathing, BreathWindup / GetSpeedScale());
}

void AVulcanBoss::BeginBreathing()
{
	BreathTimeRemaining = BreathDuration + (bPhaseTwo ? PhaseTwoBreathExtraDuration : 0.f);
	OnBreathStarted();

	// Visible flames from the mouth along Vulcan's facing (the damage cone in BreathTick is unchanged).
	USceneComponent* Mouth = GetMesh();
	if (OtterParts.IsValidIndex(OtterBody::Muzzle) && OtterParts[OtterBody::Muzzle])
	{
		Mouth = OtterParts[OtterBody::Muzzle];
	}
	AFireFX::Spawn(GetWorld(), Mouth->GetComponentLocation(),
		AFireFX::BreathPreset(BreathRange + (bPhaseTwo ? PhaseTwoBreathExtraRange : 0.f),
			BreathHalfAngle + (bPhaseTwo ? PhaseTwoBreathExtraHalfAngle : 0.f), BreathTimeRemaining),
		Mouth);
	GetWorldTimerManager().SetTimer(BreathTimer, this, &AVulcanBoss::BreathTick, FMath::Max(BreathTickInterval, 0.02f), true);
}

void AVulcanBoss::BreathTick()
{
	const float Interval = FMath::Max(BreathTickInterval, 0.02f);

	// Slowly sweep the flames toward the player.
	if (APawn* Player = GetPlayer())
	{
		const float DesiredYaw = (Player->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
		const float NewYaw = FMath::FixedTurn(GetActorRotation().Yaw, DesiredYaw, BreathTurnRate * GetSpeedScale() * Interval);
		SetActorRotation(FRotator(0.f, NewYaw, 0.f));
	}

	const float Range = BreathRange + (bPhaseTwo ? PhaseTwoBreathExtraRange : 0.f);
	const float HalfAngle = BreathHalfAngle + (bPhaseTwo ? PhaseTwoBreathExtraHalfAngle : 0.f);

	if (bShowDebug)
	{
		const float HalfAngleRad = FMath::DegreesToRadians(HalfAngle);
		DrawDebugCone(GetWorld(), GetActorLocation(), GetActorForwardVector(), Range, HalfAngleRad, HalfAngleRad, 12, FColor::Red, false, Interval);
	}

	if (IsPlayerInCone(Range, HalfAngle))
	{
		DamagePlayer(BreathDamagePerTick);
	}

	BreathTimeRemaining -= Interval;
	if (BreathTimeRemaining <= 0.f)
	{
		EndBreath();
	}
}

void AVulcanBoss::EndBreath()
{
	GetWorldTimerManager().ClearTimer(BreathTimer);
	OnBreathEnded();

	if (BreathMontage)
	{
		StopAnimMontage(BreathMontage);
	}

	Schedule(AttackTimer, &AVulcanBoss::FinishAttack, 0.4f / GetSpeedScale());
}

// ============================================================ Magma Dive

void AVulcanBoss::StartDive()
{
	PlayMontageScaled(DiveMontage);
	AFireFX::Spawn(GetWorld(), GetFeetLocation(), AFireFX::BurstPreset(180.f, 70));
	Schedule(AttackTimer, &AVulcanBoss::DiveSubmerge, DiveSubmergeTime / GetSpeedScale());
}

void AVulcanBoss::DiveSubmerge()
{
	OnDiveSubmerged(GetFeetLocation());
	AFireFX::Spawn(GetWorld(), GetFeetLocation(), AFireFX::BurstPreset(260.f, 110));

	// Disable movement before collision, otherwise Vulcan falls through the floor.
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);

	Schedule(AttackTimer, &AVulcanBoss::DiveShowWarning, DiveHiddenTime / GetSpeedScale());
}

void AVulcanBoss::DiveShowWarning()
{
	FVector Ground = GetFeetLocation();
	if (APawn* Player = GetPlayer())
	{
		Ground = Player->GetActorLocation();
		Ground.Z -= Player->GetSimpleCollisionHalfHeight();
	}

	DiveTarget = Ground + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	OnDiveWarning(Ground, DiveRadius);

	const float WarningTime = DiveWarningTime / GetSpeedScale();
	AFireFX::Spawn(GetWorld(), Ground, AFireFX::EmbersPreset(DiveRadius, WarningTime));
	if (bShowDebug)
	{
		DrawDebugCircle(GetWorld(), Ground + FVector(0.f, 0.f, 5.f), DiveRadius, 32, FColor::Orange, false, WarningTime, 0, 4.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
	}

	Schedule(AttackTimer, &AVulcanBoss::DiveEmerge, WarningTime);
}

void AVulcanBoss::DiveEmerge()
{
	SetActorLocation(DiveTarget, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	FacePlayer();
	PlayMontageScaled(EmergeMontage);

	const FVector Ground = GetFeetLocation();
	OnDiveEmerged(Ground);
	AFireFX::Spawn(GetWorld(), Ground, AFireFX::BurstPreset(DiveRadius, 160));

	if (APawn* Player = GetPlayer())
	{
		if (FVector::Dist2D(Player->GetActorLocation(), Ground) <= DiveRadius)
		{
			DamagePlayer(DiveDamage);
		}
	}

	if (bPhaseTwo && BurnPatchDuration > 0.f)
	{
		BurnPatchLocation = Ground;
		BurnPatchTimeRemaining = BurnPatchDuration;
		OnBurnPatchStarted(Ground, DiveRadius, BurnPatchDuration);
		AFireFX::Spawn(GetWorld(), Ground, AFireFX::GroundFirePreset(DiveRadius, BurnPatchDuration));
		GetWorldTimerManager().SetTimer(BurnTimer, this, &AVulcanBoss::BurnPatchTick, 0.5f, true);
	}

	Schedule(AttackTimer, &AVulcanBoss::FinishAttack, DiveRecoverTime / GetSpeedScale());
}

void AVulcanBoss::BurnPatchTick()
{
	if (bShowDebug)
	{
		DrawDebugCircle(GetWorld(), BurnPatchLocation + FVector(0.f, 0.f, 5.f), DiveRadius, 32, FColor::Red, false, 0.5f, 0, 3.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
	}

	if (APawn* Player = GetPlayer())
	{
		const FVector PlayerLocation = Player->GetActorLocation();
		const bool bOnPatch = FVector::Dist2D(PlayerLocation, BurnPatchLocation) <= DiveRadius
			&& FMath::Abs(PlayerLocation.Z - BurnPatchLocation.Z) < 200.f;
		if (bOnPatch)
		{
			DamagePlayer(BurnPatchDamagePerTick);
		}
	}

	BurnPatchTimeRemaining -= 0.5f;
	if (BurnPatchTimeRemaining <= 0.f)
	{
		GetWorldTimerManager().ClearTimer(BurnTimer);
	}
}

// ============================================================ Health / phases

void AVulcanBoss::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	// Getting hit wakes Vulcan up even if the arena trigger was skipped.
	if (!bFightActive && NewHealth < MaxHealth)
	{
		StartFight();
	}

	// Flinch when hit between attacks (never mid-attack or mid-roar, so those always finish).
	const bool bTookDamage = NewHealth < (LastHealth < 0.f ? MaxHealth : LastHealth);
	LastHealth = NewHealth;
	if (bTookDamage && HitReactMontage && bFightActive && !bDead && NewHealth > 0.f && !IsHidden()
		&& IntroState == EIntroState::Done && CurrentAttack == EVulcanAttack::None
		&& !GetWorldTimerManager().IsTimerActive(RoarTimer) && GetWorld()->GetTimeSeconds() >= NextHitReactTime)
	{
		NextHitReactTime = GetWorld()->GetTimeSeconds() + HitReactCooldown;
		PlayMontageScaled(HitReactMontage);
	}

	if (!bPhaseTwo && NewHealth > 0.f && MaxHealth > 0.f && NewHealth / MaxHealth <= PhaseTwoHealthPercent)
	{
		bPhaseTwo = true;
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * PhaseTwoSpeedMultiplier;
		OnPhaseTwoStarted();
	}
}

void AVulcanBoss::HandleDeath(AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;

	if (CurrentAttack == EVulcanAttack::MoltenBreath)
	{
		OnBreathEnded();
	}
	CurrentAttack = EVulcanAttack::None;

	GetWorldTimerManager().ClearAllTimersForObject(this);

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}

	// In case Vulcan dies while submerged.
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	StopAnimMontage();
	PlayMontageScaled(DeathMontage);
	GetCharacterMovement()->DisableMovement();

	// Lava cools to black, eyes and core glow go out.
	ApplyOtterLook();

	OnVulcanDefeated();
}

// ============================================================ Helpers

APawn* AVulcanBoss::GetPlayer() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void AVulcanBoss::FacePlayer()
{
	if (APawn* Player = GetPlayer())
	{
		const float Yaw = (Player->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
		SetActorRotation(FRotator(0.f, Yaw, 0.f));
	}
}

bool AVulcanBoss::IsPlayerInCone(float Range, float HalfAngleDegrees) const
{
	const APawn* Player = GetPlayer();
	if (!Player)
	{
		return false;
	}

	FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.f;

	// Measure to the edge of the player's capsule, not its center.
	const float Distance = ToPlayer.Size() - Player->GetSimpleCollisionRadius();
	if (Distance > Range)
	{
		return false;
	}
	if (ToPlayer.IsNearlyZero())
	{
		return true;
	}

	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.f;

	const float CosAngle = FVector::DotProduct(Forward.GetSafeNormal(), ToPlayer.GetSafeNormal());
	return CosAngle >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
}

void AVulcanBoss::DamagePlayer(float BaseAmount)
{
	if (APawn* Player = GetPlayer())
	{
		const float Amount = BaseAmount * (bPhaseTwo ? PhaseTwoDamageMultiplier : 1.f);
		UGameplayStatics::ApplyDamage(Player, Amount, GetController(), this, UDamageType::StaticClass());
	}
}

void AVulcanBoss::BuildMontagesFromAnimations()
{
	auto MakeMontage = [](UAnimSequenceBase* Animation, bool bHoldLastFrame = false) -> UAnimMontage*
	{
		UAnimMontage* Montage = Animation ? UAnimMontage::CreateSlotAnimationAsDynamicMontage(Animation, TEXT("DefaultSlot"), 0.2f, 0.3f) : nullptr;
		if (Montage && bHoldLastFrame)
		{
			Montage->bEnableAutoBlendOut = false;
		}
		return Montage;
	};

	if (!RoarMontage)      { RoarMontage = MakeMontage(RoarAnimation); }
	if (!BreathMontage)    { BreathMontage = MakeMontage(RoarAnimation); }
	if (!DiveMontage)      { DiveMontage = MakeMontage(JumpAnimation); }
	if (!EmergeMontage)    { EmergeMontage = MakeMontage(LandAnimation); }
	if (!HitReactMontage)  { HitReactMontage = MakeMontage(HitReactAnimation); }
	if (!DeathMontage)     { DeathMontage = MakeMontage(DeathAnimation, true); }
}

float AVulcanBoss::PlayMontageScaled(UAnimMontage* Montage)
{
	return Montage ? PlayAnimMontage(Montage, GetSpeedScale()) : 0.f;
}

FVector AVulcanBoss::GetFeetLocation() const
{
	return GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void AVulcanBoss::Schedule(FTimerHandle& Handle, void (AVulcanBoss::*Callback)(), float DelaySeconds)
{
	// SetTimer with a rate of 0 cancels instead of firing, so keep it positive.
	GetWorldTimerManager().SetTimer(Handle, this, Callback, FMath::Max(DelaySeconds, 0.01f), false);
}
