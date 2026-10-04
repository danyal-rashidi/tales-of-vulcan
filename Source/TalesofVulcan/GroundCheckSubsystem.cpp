#include "GroundCheckSubsystem.h"
#include "FireFX.h"
#include "HipRootMotionComponent.h"
#include "MeleeAttackComponent.h"
#include "SpearGripComponent.h"
#include "VulcanBoss.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/DamageType.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

bool UGroundCheckSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const TCHAR* CommandLine = FCommandLine::Get();
	return (FParse::Param(CommandLine, TEXT("GroundCheck")) || FParse::Param(CommandLine, TEXT("SpearShot"))
		|| FParse::Param(CommandLine, TEXT("EnvShot")) || FParse::Param(CommandLine, TEXT("FireShot"))
		|| FParse::Param(CommandLine, TEXT("BossShot")))
		&& Super::ShouldCreateSubsystem(Outer);
}

void UGroundCheckSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	const TCHAR* CommandLine = FCommandLine::Get();
	FTimerManager& Timers = InWorld.GetTimerManager();
	if (FParse::Param(CommandLine, TEXT("GroundCheck")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::Report, 4.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("EnvShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartEnvShot, 3.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("FireShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartFireShot, 3.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("BossShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartBossShot, 2.f, false);
	}
	else
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartSpearShot, 3.f, false);
	}
}

void UGroundCheckSubsystem::ShootFrom(const FVector& Location, const FVector& LookAt)
{
	UWorld* World = GetWorld();
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	if (!Controller)
	{
		return;
	}
	if (!ShotCamera)
	{
		ShotCamera = World->SpawnActor<ACameraActor>(Location, (LookAt - Location).Rotation());
	}
	ShotCamera->SetActorLocationAndRotation(Location, (LookAt - Location).Rotation());
	Controller->SetViewTarget(ShotCamera);
}

void UGroundCheckSubsystem::StartEnvShot()
{
	// Arena centre (see ArenaDressingSubsystem).
	const FVector Center(-44.f, -5.f, 0.f);

	After(0.2f, [this]() { Shot(TEXT("env_player")); });
	After(0.4f, [this, Center]() { ShootFrom(Center + FVector(0.f, -2600.f, 1400.f), Center); });
	After(0.8f, [this]() { Shot(TEXT("env_overview")); });
	After(1.0f, [this, Center]() { ShootFrom(Center + FVector(2300.f, 900.f, 120.f), Center + FVector(-2500.f, -900.f, 40.f)); });
	After(1.4f, [this]() { Shot(TEXT("env_ground")); });
	After(2.4f, [this]() { Shot(TEXT("env_ground_later")); });
	After(2.6f, [this, Center]() { ShootFrom(Center + FVector(2600.f, -1300.f, 70.f), Center + FVector(3000.f, -1700.f, 30.f)); });
	After(3.0f, [this]() { Shot(TEXT("env_grass_close")); });
	After(3.2f, [this, Center]() { ShootFrom(Center + FVector(-500.f, 0.f, 250.f), Center + FVector(3300.f, 0.f, 300.f)); });
	After(3.6f, [this]() { Shot(TEXT("env_wall")); });
	After(4.0f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartBossShot()
{
	UWorld* World = GetWorld();
	TActorIterator<AVulcanBoss> It(World);
	AVulcanBoss* Boss = It ? *It : nullptr;
	if (!Boss)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}

	TWeakObjectPtr<AVulcanBoss> WeakBoss = Boss;
	Boss->StartFight();

	// A shot every 0.6 s, framed on Vulcan from a fixed angle.
	for (int32 i = 0; i < 55; ++i)
	{
		After(0.3f + i * 0.6f, [this, WeakBoss, i]()
		{
			if (AVulcanBoss* B = WeakBoss.Get())
			{
				const FVector Target = B->GetActorLocation() + FVector(0.f, 0.f, 60.f);
				ShootFrom(Target + FVector(700.f, 450.f, 260.f), Target);
			}
			Shot(FString::Printf(TEXT("boss_%02d"), i));
		});
	}

	// A few hits between attacks to see the flinch, then the death.
	auto Hit = [WeakBoss, World](float Amount)
	{
		if (AVulcanBoss* B = WeakBoss.Get())
		{
			UGameplayStatics::ApplyDamage(B, Amount, UGameplayStatics::GetPlayerController(World, 0),
				UGameplayStatics::GetPlayerPawn(World, 0), UDamageType::StaticClass());
		}
	};
	for (const float Time : { 14.f, 17.f, 20.f, 23.f })
	{
		After(Time, [Hit]() { Hit(5.f); });
	}
	After(28.f, [Hit]() { Hit(100000.f); });
	After(33.5f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartFireShot()
{
	UWorld* World = GetWorld();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Player)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}

	const FVector Feet = Player->GetActorLocation() - FVector(0.f, 0.f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Forward = Player->GetActorForwardVector();
	const FVector Side = Player->GetActorRightVector();

	// Breath from the player's position along its facing, filmed from the side.
	After(0.1f, [this, World, Player, Feet, Forward, Side]()
	{
		AFireFX::Spawn(World, Player->GetActorLocation() + FVector(0.f, 0.f, 60.f), AFireFX::BreathPreset(600.f, 15.f, 2.f), Player->GetRootComponent());
		ShootFrom(Feet + Forward * 300.f + Side * 650.f + FVector(0.f, 0.f, 180.f), Feet + Forward * 300.f + FVector(0.f, 0.f, 60.f));
	});
	After(0.7f, [this]() { Shot(TEXT("fire_breath_a")); });
	After(1.3f, [this]() { Shot(TEXT("fire_breath_b")); });

	// Dive take-off burst, landing burst, then warning embers and a burning patch.
	const FVector Spot = Feet + Forward * 500.f;
	After(2.4f, [this, World, Spot, Side]()
	{
		AFireFX::Spawn(World, Spot, AFireFX::BurstPreset(300.f, 160));
		ShootFrom(Spot + Side * 900.f + FVector(0.f, 0.f, 250.f), Spot + FVector(0.f, 0.f, 80.f));
	});
	After(2.55f, [this]() { Shot(TEXT("fire_burst_a")); });
	After(2.8f, [this]() { Shot(TEXT("fire_burst_b")); });
	After(3.2f, [this]() { Shot(TEXT("fire_burst_c")); });
	After(3.6f, [World, Spot]()
	{
		AFireFX::Spawn(World, Spot + FVector(-400.f, 0.f, 0.f), AFireFX::EmbersPreset(300.f, 3.f));
		AFireFX::Spawn(World, Spot + FVector(400.f, 0.f, 0.f), AFireFX::GroundFirePreset(300.f, 3.f));
	});
	After(4.6f, [this]() { Shot(TEXT("fire_embers_ground")); });
	After(5.2f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::After(float Seconds, TFunction<void()> Action)
{
	FTimerHandle& Handle = ShotTimers.AddDefaulted_GetRef();
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Action)), Seconds, false);
}

void UGroundCheckSubsystem::Shot(const FString& Name)
{
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("SpearShots") / (Name + TEXT(".png")), false, false);
}

void UGroundCheckSubsystem::StartSpearShot()
{
	UWorld* World = GetWorld();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	if (!Player || !Controller)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}

	const TCHAR* CommandLine = FCommandLine::Get();

	// What the grip has to work with.
	TArray<USkeletalMeshComponent*> Meshes;
	Player->GetComponents(Meshes);
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		FString HandBones;
		for (int32 i = 0; i < Mesh->GetNumBones(); ++i)
		{
			const FString Bone = Mesh->GetBoneName(i).ToString();
			if (Bone.Contains(TEXT("Hand")) || Bone.Contains(TEXT("hand")))
			{
				HandBones += Bone + TEXT(" ");
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[SpearShot] %s hand bones: %s"), *Mesh->GetName(), *HandBones);
	}
	TArray<UStaticMeshComponent*> Statics;
	Player->GetComponents(Statics);
	for (const UStaticMeshComponent* Static : Statics)
	{
		if (Static->GetStaticMesh())
		{
			const FBox Box = Static->GetStaticMesh()->GetBoundingBox();
			UE_LOG(LogTemp, Warning, TEXT("[SpearShot] %s mesh %s local box min %s max %s scale %s socket %s"),
				*Static->GetName(), *Static->GetStaticMesh()->GetName(), *Box.Min.ToString(), *Box.Max.ToString(),
				*Static->GetComponentScale().ToString(), *Static->GetAttachSocketName().ToString());
		}
	}

	// Preview the intended setup: hip root motion, a spear grip and the two-handed attack montages.
	if (!Player->FindComponentByClass<UHipRootMotionComponent>() && !FParse::Param(CommandLine, TEXT("NoHipMotion")))
	{
		NewObject<UHipRootMotionComponent>(Player, TEXT("HipRootMotionPreview"))->RegisterComponent();
	}
	if (!Player->FindComponentByClass<USpearGripComponent>())
	{
		USpearGripComponent* Grip = NewObject<USpearGripComponent>(Player, TEXT("SpearGripPreview"));
		FParse::Value(CommandLine, TEXT("GripFront="), Grip->FrontHandPosition);
		FParse::Value(CommandLine, TEXT("GripRoll="), Grip->Roll);
		Grip->bFlipSpear = FParse::Param(CommandLine, TEXT("GripFlip"));
		if (FParse::Param(CommandLine, TEXT("GripSwapHands")))
		{
			Swap(Grip->FrontHandBone, Grip->BackHandBone);
			Swap(Grip->FrontKnuckleBone, Grip->BackKnuckleBone);
		}
		Grip->RegisterComponent();
	}

	TArray<UMeleeAttackComponent*> Attacks;
	Player->GetComponents(Attacks);
	UMeleeAttackComponent* Light = nullptr;
	UMeleeAttackComponent* Heavy = nullptr;
	for (UMeleeAttackComponent* Attack : Attacks)
	{
		if (Attack->GetName().Contains(TEXT("Light")))
		{
			Light = Attack;
			Attack->AttackMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Player/Animations/RTG_Great_Sword_Slash_Montage.RTG_Great_Sword_Slash_Montage"));
		}
		else if (Attack->GetName().Contains(TEXT("Heavy")))
		{
			Heavy = Attack;
			Attack->AttackMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Player/Animations/RTG_Great_Sword_High_Spin_Attack_Montage.RTG_Great_Sword_High_Spin_Attack_Montage"));
		}
	}

	// Optionally move to open ground first (-ShotAtX= -ShotAtY=), facing +X.
	float AtX = 0.f, AtY = 0.f;
	if (FParse::Value(CommandLine, TEXT("ShotAtX="), AtX) && FParse::Value(CommandLine, TEXT("ShotAtY="), AtY))
	{
		Player->SetActorLocationAndRotation(FVector(AtX, AtY, Player->GetActorLocation().Z + 100.f), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Controller->SetControlRotation(FRotator::ZeroRotator);
	}

	// Film from the side.
	float Yaw = 90.f;
	float Distance = 260.f;
	FParse::Value(CommandLine, TEXT("ShotYaw="), Yaw);
	FParse::Value(CommandLine, TEXT("ShotDist="), Distance);
	const FVector Target = Player->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	const FVector Offset = Player->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * Distance + FVector(0.f, 0.f, 20.f);
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(Target + Offset, (-Offset).Rotation());
	Camera->AttachToActor(Player, FAttachmentTransformRules::KeepWorldTransform);
	Controller->SetViewTarget(Camera);

	After(0.5f, [this]() { Shot(TEXT("idle")); });
	After(1.0f, [Light]() { if (Light) { Light->TryAttack(); } });
	for (int32 i = 1; i <= 12; ++i)
	{
		After(1.0f + i * 0.12f, [this, i]() { Shot(FString::Printf(TEXT("light_%02d"), i)); });
	}
	After(3.2f, [Heavy]() { if (Heavy) { Heavy->TryAttack(); } });
	for (int32 i = 1; i <= 14; ++i)
	{
		After(3.2f + i * 0.15f, [this, i]() { Shot(FString::Printf(TEXT("heavy_%02d"), i)); });
	}
	After(6.0f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::Report()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Character = *It;
		const FVector Location = Character->GetActorLocation();
		const float CapsuleBottom = Location.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();

		UE_LOG(LogTemp, Warning, TEXT("[GroundCheck] %s at %s  capsule bottom z=%.1f  movement mode=%d"),
			*Character->GetName(), *Location.ToString(), CapsuleBottom, Move ? (int32)Move->MovementMode.GetValue() : -1);

		if (Move)
		{
			const FFindFloorResult& Floor = Move->CurrentFloor;
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck]   standing on: actor=%s component=%s floor z=%.1f gap=%.1f walkable=%d"),
				*GetNameSafe(Floor.HitResult.GetActor()), *GetNameSafe(Floor.HitResult.GetComponent()),
				Floor.HitResult.ImpactPoint.Z, Floor.FloorDist, Floor.bWalkableFloor ? 1 : 0);
		}

		// Every collision surface straight below, top to bottom.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundCheck), true, Character);
		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, Location, Location - FVector(0.f, 0.f, 1500.f),
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), Params);
		for (const FHitResult& Hit : Hits)
		{
			const UPrimitiveComponent* Prim = Hit.GetComponent();
			const AActor* HitActor = Hit.GetActor();
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck]   below: z=%.1f actor=%s (%s) component=%s visible=%d hiddenInGame=%d profile=%s"),
				Hit.ImpactPoint.Z, *GetNameSafe(HitActor), HitActor ? *HitActor->GetActorNameOrLabel() : TEXT("-"),
				*GetNameSafe(Prim), Prim && Prim->IsVisible() ? 1 : 0, Prim && Prim->bHiddenInGame ? 1 : 0,
				Prim ? *Prim->GetCollisionProfileName().ToString() : TEXT("-"));
		}

		// Lowest visible point of each visible part (skin, otter shapes, weapon...).
		TArray<UPrimitiveComponent*> Parts;
		Character->GetComponents(Parts);
		for (const UPrimitiveComponent* Part : Parts)
		{
			if (Part == Character->GetCapsuleComponent() || !Part->IsVisible() || Part->bHiddenInGame)
			{
				continue;
			}
			const float Lowest = Part->Bounds.Origin.Z - Part->Bounds.BoxExtent.Z;
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck]   part %s (%s) lowest z=%.1f  (%.1f above capsule bottom)"),
				*Part->GetName(), *Part->GetClass()->GetName(), Lowest, Lowest - CapsuleBottom);
		}

		// Feet of every skeletal mesh (bounds can be loose, bones are exact).
		TArray<USkeletalMeshComponent*> Meshes;
		Character->GetComponents(Meshes);
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			float LowestBoneZ = TNumericLimits<float>::Max();
			FName LowestBone;
			for (int32 i = 0; i < Mesh->GetNumBones(); ++i)
			{
				const FName Bone = Mesh->GetBoneName(i);
				const float Z = Mesh->GetBoneLocation(Bone).Z;
				if (Z < LowestBoneZ)
				{
					LowestBoneZ = Z;
					LowestBone = Bone;
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck]   skeleton %s visible=%d lowest bone %s z=%.1f  (%.1f above capsule bottom)"),
				*Mesh->GetName(), Mesh->IsVisible() && !Mesh->bHiddenInGame ? 1 : 0, *LowestBone.ToString(), LowestBoneZ, LowestBoneZ - CapsuleBottom);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[GroundCheck] done"));
	FPlatformMisc::RequestExit(false);
}
