#include "GroundCheckSubsystem.h"
#include "AIController.h"
#include "DodgeComponent.h"
#include "FireFX.h"
#include "HealthComponent.h"
#include "HipRootMotionComponent.h"
#include "PlungeAttackComponent.h"
#include "LockOnComponent.h"
#include "MeleeAttackComponent.h"
#include "SpearGripComponent.h"
#include "VulcanBoss.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/DamageType.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "AssetCompilingManager.h"
#include "RomeExitGate.h"
#include "PlayerEquipComponent.h"
#include "Engine/StaticMeshActor.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

bool UGroundCheckSubsystem::IsTestRun()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	return FParse::Param(CommandLine, TEXT("GroundCheck")) || FParse::Param(CommandLine, TEXT("SpearShot"))
		|| FParse::Param(CommandLine, TEXT("EnvShot")) || FParse::Param(CommandLine, TEXT("FireShot"))
		|| FParse::Param(CommandLine, TEXT("BossShot")) || FParse::Param(CommandLine, TEXT("BossProbe")) || FParse::Param(CommandLine, TEXT("LockShot"))
		|| FParse::Param(CommandLine, TEXT("ElvisShot")) || FParse::Param(CommandLine, TEXT("OtterShot")) || FParse::Param(CommandLine, TEXT("RomeShot")) || FParse::Param(CommandLine, TEXT("DragonShot")) || FParse::Param(CommandLine, TEXT("MapShot")) || FParse::Param(CommandLine, TEXT("ExitShot")) || FParse::Param(CommandLine, TEXT("MoveShot")) || FParse::Param(CommandLine, TEXT("PropShot")) || FParse::Param(CommandLine, TEXT("StepShot")) || FParse::Param(CommandLine, TEXT("LegShot")) || FParse::Param(CommandLine, TEXT("HouseShot"));
}

bool UGroundCheckSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsTestRun() && Super::ShouldCreateSubsystem(Outer);
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
	else if (FParse::Param(CommandLine, TEXT("LockShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartLockShot, 2.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("BossProbe")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartBossProbe, 2.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("PropShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartPropShot, 5.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("MoveShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartMoveShot, 3.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("HouseShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartHouseShot, 3.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("LegShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartLegShot, 3.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("StepShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartStepShot, 3.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("ExitShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartExitShot, 2.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("MapShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartMapShot, 4.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("DragonShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartDragonShot, 2.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("RomeShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartRomeShot, 2.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("OtterShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartOtterShot, 1.f, false);
	}
	else if (FParse::Param(CommandLine, TEXT("ElvisShot")))
	{
		Timers.SetTimer(ReportTimer, this, &UGroundCheckSubsystem::StartElvisShot, 2.f, false);
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
		// The camera jumps between shots; motion blur would smear every first frame.
		ShotCamera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		ShotCamera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.f;
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
	After(3.8f, [this]() { ShootFrom(FVector(560.f, -640.f, 300.f), FVector(-120.f, 60.f, 60.f)); });
	After(4.2f, [this]() { Shot(TEXT("env_stairs")); });
	After(4.4f, [this]() { ShootFrom(FVector(-800.f, 760.f, 220.f), FVector(-120.f, 60.f, 60.f)); });
	After(4.8f, [this]() { Shot(TEXT("env_stairs2")); });
	After(5.2f, []() { FPlatformMisc::RequestExit(false); });
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

void UGroundCheckSubsystem::StartBossProbe()
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

	// Twice a second for 25 s: is the skeleton animating, what plays, how fast is Vulcan moving?
	for (int32 i = 0; i < 50; ++i)
	{
		After(0.5f + i * 0.5f, [WeakBoss, i]()
		{
			AVulcanBoss* B = WeakBoss.Get();
			USkeletalMeshComponent* Mesh = B ? B->GetMesh() : nullptr;
			if (!Mesh)
			{
				return;
			}
			const UAnimInstance* Anim = Mesh->GetAnimInstance();
			const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
			const FTransform ToActor = B->GetActorTransform();
			auto Local = [&](const TCHAR* Bone) { return ToActor.InverseTransformPosition(Mesh->GetSocketLocation(Bone)); };

			int32 VisibleParts = 0;
			TArray<UStaticMeshComponent*> Parts;
			B->GetComponents(Parts);
			for (const UStaticMeshComponent* Part : Parts)
			{
				VisibleParts += Part->IsVisible() && Part->GetName().StartsWith(TEXT("Otter_")) ? 1 : 0;
			}

			UE_LOG(LogTemp, Warning, TEXT("[BossProbe] t=%4.1f speed=%5.0f paused=%d anim=%s montage=%s mesh visible=%d otter parts visible=%d foot_l=%s foot_r=%s hand_r=%s"),
				i * 0.5f + 0.5f, B->GetVelocity().Size2D(), Mesh->bPauseAnims ? 1 : 0,
				Anim ? *Anim->GetClass()->GetName() : TEXT("none"), Montage ? *Montage->GetName() : TEXT("-"),
				Mesh->IsVisible() ? 1 : 0, VisibleParts,
				*Local(TEXT("foot_l")).ToCompactString(), *Local(TEXT("foot_r")).ToCompactString(), *Local(TEXT("hand_r")).ToCompactString());
		});
	}
	After(26.f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartLockShot()
{
	UWorld* World = GetWorld();
	TActorIterator<AVulcanBoss> It(World);
	AVulcanBoss* Boss = It ? *It : nullptr;
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	if (!Boss || !Player || !Controller)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}
	Boss->StartFight();

	// Stand in the open 12 m in front of Vulcan.
	After(5.5f, [Player, Boss]()
	{
		const FVector Away = (Player->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
		Player->SetActorLocation(Boss->GetActorLocation() + Away * 1200.f + FVector(0.f, 0.f, 50.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	// After the wake-up roar: face roughly toward Vulcan, lock on, and watch through the player's camera.
	After(6.f, [Player, Controller, Boss]()
	{
		Controller->SetControlRotation((Boss->GetActorLocation() - Player->GetActorLocation()).Rotation() + FRotator(0.f, 25.f, 0.f));
	});
	After(6.3f, [Player]()
	{
		ULockOnComponent* LockOn = Player->FindComponentByClass<ULockOnComponent>();
		UE_LOG(LogTemp, Warning, TEXT("[LockShot] lock-on component %s, locked: %d"), LockOn ? TEXT("found") : TEXT("missing"), LockOn && LockOn->ToggleLock() ? 1 : 0);
	});
	// Roll, jump and plunge (dust), hit Vulcan (boss bar trail), then kill him (victory banner).
	if (UHealthComponent* PlayerHealth = Player->FindComponentByClass<UHealthComponent>())
	{
		PlayerHealth->bInvulnerable = true;
	}
	After(8.f, [Player]() { if (UDodgeComponent* Dodge = Player->FindComponentByClass<UDodgeComponent>()) { Dodge->TryDodge(); } });
	After(9.5f, [Player]() { Player->Jump(); });
	After(9.8f, [Player]() { if (UPlungeAttackComponent* Plunge = Player->FindComponentByClass<UPlungeAttackComponent>()) { Plunge->TryPlunge(); } });
	auto Hit = [World, Player, Boss](float Amount)
	{
		UGameplayStatics::ApplyDamage(Boss, Amount, UGameplayStatics::GetPlayerController(World, 0), Player, UDamageType::StaticClass());
	};
	After(11.f, [Hit]() { Hit(150.f); });
	After(13.f, [Hit]() { Hit(100000.f); });
	for (int32 i = 0; i < 26; ++i)
	{
		After(6.4f + i * 0.5f, [this, i]() { Shot(FString::Printf(TEXT("lock_%02d"), i)); });
	}

	// Everything heard during the run, to Saved/SpearShots/lock_audio.wav (the wav is written in the background).
	UAudioMixerBlueprintLibrary::StartRecordingOutput(World, 20.f);
	After(19.5f, [World]()
	{
		UAudioMixerBlueprintLibrary::StopRecordingOutput(World, EAudioRecordingExportType::WavFile, TEXT("lock_audio"), FPaths::ProjectSavedDir() / TEXT("SpearShots"));
	});
	After(21.f, []() { FPlatformMisc::RequestExit(false); });
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

	// Dust once the fire is out: plunge slam (left) and a footstep puff (right), seen from above the player.
	After(6.8f, [this, World, Feet, Spot, Forward, Side]()
	{
		AFireFX::Spawn(World, Spot - Side * 250.f, AFireFX::DustPreset(200.f, 40));
		AFireFX::Spawn(World, Spot + Side * 250.f, AFireFX::DustPreset(50.f, 5));
		ShootFrom(Feet - Forward * 200.f + FVector(0.f, 0.f, 300.f), Spot + FVector(0.f, 0.f, 60.f));
	});
	After(7.0f, [this]() { Shot(TEXT("dust_a")); });
	After(7.4f, [this]() { Shot(TEXT("dust_b")); });
	After(7.9f, [this]() { Shot(TEXT("dust_c")); });
	After(8.4f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::After(float Seconds, TFunction<void()> Action)
{
	FTimerHandle& Handle = ShotTimers.AddDefaulted_GetRef();
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, MoveTemp(Action)), Seconds, false);
}

void UGroundCheckSubsystem::Shot(const FString& Name)
{
	// Lock-on shots include the UI so the target dot shows.
	const bool bUI = Name.StartsWith(TEXT("lock")) || FParse::Param(FCommandLine::Get(), TEXT("ShotUI"));
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("SpearShots") / (Name + TEXT(".png")), bUI, false);
}

void UGroundCheckSubsystem::StartExitShot()
{
	// The whole way through L_Rome: the player starts in the town, walks into the colosseum's entrance and comes
	// out inside the arena; Vulcan is killed, the north gate's portcullis rises, the player steps into it and
	// comes out on the plaza outside, where the storm clears.
	UWorld* World = GetWorld();
	TActorIterator<AVulcanBoss> BossIt(World);
	AVulcanBoss* Boss = BossIt ? *BossIt : nullptr;
	ARomeExitGate* Gate = nullptr;
	ARomeExitGate* Entrance = nullptr;
	for (TActorIterator<ARomeExitGate> It(World); It; ++It)
	{
		(It->bEntrance ? Entrance : Gate) = *It;
	}
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	if (!Boss || !Gate || !Entrance || !Player || !Controller)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExitShot] boss %d gate %d entrance %d player %d"), Boss != nullptr, Gate != nullptr, Entrance != nullptr, Player != nullptr);
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (UHealthComponent* PlayerHealth = Player->FindComponentByClass<UHealthComponent>())
	{
		PlayerHealth->bInvulnerable = true;
	}
	UE_LOG(LogTemp, Display, TEXT("[ExitShot] player starts at %s"), *Player->GetActorLocation().ToString());
	After(1.5f, [this]() { Shot(TEXT("exit_0_start")); });
	// Walk up to the entrance, then into it.
	const FVector In = Entrance->GetActorForwardVector();
	After(2.0f, [Player, Controller, Entrance, In]()
	{
		Player->SetActorLocation(Entrance->GetActorLocation() - In * 700.f + FVector(0.f, 0.f, 120.f), false, nullptr, ETeleportType::TeleportPhysics);
		Controller->SetControlRotation(In.Rotation());
	});
	After(3.5f, [this]() { Shot(TEXT("exit_1_entrance")); });
	After(3.7f, [Player, Entrance, In]()
	{
		Player->SetActorLocation(Entrance->GetActorLocation() + In * 25.f + FVector(0.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	After(6.5f, [this, Player]()
	{
		UE_LOG(LogTemp, Display, TEXT("[ExitShot] inside at %s"), *Player->GetActorLocation().ToString());
		Shot(TEXT("exit_2_inside"));
	});

	const float T0 = 7.f;
	After(T0, [Boss]() { Boss->StartFight(); });
	const FVector Into = Gate->GetActorForwardVector();
	const FVector Front = Gate->GetActorLocation() - Into * 900.f + FVector(0.f, 0.f, 220.f);
	const FVector Look = Gate->GetActorLocation() + FVector(0.f, 0.f, 220.f);
	After(T0 + 5.5f, [this, Front, Look]() { ShootFrom(Front, Look); });
	After(T0 + 6.0f, [this]() { Shot(TEXT("exit_3_shut")); });
	After(T0 + 6.2f, [World, Boss, Player]() { UGameplayStatics::ApplyDamage(Boss, 100000.f, UGameplayStatics::GetPlayerController(World, 0), Player, UDamageType::StaticClass()); });
	After(T0 + 11.0f, [this]() { Shot(TEXT("exit_4_rising")); });
	After(T0 + 14.5f, [this]() { Shot(TEXT("exit_5_open")); });
	// Walk out: put the player just in front of the gate, then into the passage.
	After(T0 + 14.7f, [Player, Gate, Into]()
	{
		Player->SetActorLocation(Gate->GetActorLocation() - Into * 200.f + FVector(0.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	After(T0 + 15.0f, [Player, Gate, Into]()
	{
		Player->SetActorLocation(Gate->GetActorLocation() + Into * 25.f + FVector(0.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	After(T0 + 17.5f, [Controller, Player]() { Controller->SetViewTarget(Player); });
	After(T0 + 18.5f, [this]() { Shot(TEXT("exit_6_outside")); });
	// The storm clears over SkyClearDelay + SkyClearSeconds after his death.
	const float Cleared = T0 + 6.2f + Boss->SkyClearDelay + Boss->SkyClearSeconds + 1.f;
	After(Cleared, [this, Player]()
	{
		const FVector At = Player->GetActorLocation();
		UE_LOG(LogTemp, Display, TEXT("[ExitShot] player now at %s"), *At.ToString());
		ShootFrom(At + FVector(-900.f, 1400.f, 700.f), At + FVector(0.f, -1500.f, 600.f));
	});
	After(Cleared + 0.5f, [this]() { Shot(TEXT("exit_7_clear")); });
	After(Cleared + 1.0f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartHouseShot()
{
	// Walking into the town's houses in the Rome map, through the player's own camera: a portico house (B05, C04, A07
	// have a colonnade in front) and a shop house (A04, B03, B07, C01, C02 have open shopfronts). The player starts
	// 4 m out in front of the house's middle and walks straight at it; the log says how far in they got.
	UWorld* World = GetWorld();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Player)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}
	auto Find = [World, Player](std::initializer_list<const TCHAR*> Names) -> AStaticMeshActor*
	{
		AStaticMeshActor* Best = nullptr;
		float BestDist = 1e12f;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			const UStaticMesh* Mesh = It->GetStaticMeshComponent()->GetStaticMesh();
			const FString Name = Mesh ? Mesh->GetName() : FString();
			for (const TCHAR* N : Names)
			{
				const float D = FVector::DistSquared(It->GetActorLocation(), Player->GetActorLocation());
				if (Name == FString(TEXT("SM_RomeHouse_")) + N && D < BestDist)
				{
					Best = *It; BestDist = D;
				}
			}
		}
		return Best;
	};
	struct FVisit { const TCHAR* Name; AStaticMeshActor* House; };
	const FVisit Visits[] = { { TEXT("portico"), Find({ TEXT("B05"), TEXT("C04"), TEXT("A07") }) }, { TEXT("shop"), Find({ TEXT("A04"), TEXT("B03"), TEXT("B07"), TEXT("C01") }) } };
	World->GetTimerManager().SetTimer(MoveTimer, FTimerDelegate::CreateWeakLambda(this, [this, Player]()
	{
		if (!MoveInput.IsNearlyZero())
		{
			Player->AddMovementInput(MoveInput.GetSafeNormal(), MoveInput.Size());
		}
	}), 0.005f, true);
	float At = 0.2f;
	for (const FVisit& V : Visits)
	{
		if (!V.House)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HouseShot] no %s house found"), V.Name);
			continue;
		}
		// The houses' fronts face local +Y; the pivot is the middle of the street front at ground level.
		const FTransform Frame = V.House->GetActorTransform();
		const FVector Out = Frame.GetRotation().RotateVector(FVector(0.f, 1.f, 0.f));
		const FVector Front = Frame.GetLocation();
		const FString Name = V.Name;
		const FString Mesh = V.House->GetStaticMeshComponent()->GetStaticMesh()->GetName();
		After(At, [this, Player, Front, Out, Name, Mesh]()
		{
			MoveInput = FVector::ZeroVector;
			Player->GetCharacterMovement()->StopMovementImmediately();
			Player->TeleportTo(Front + Out * 400.f + FVector(0.f, 0.f, 120.f), (-Out).Rotation());
			if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
			{
				PC->SetControlRotation((-Out).Rotation() + FRotator(-10.f, 0.f, 0.f));
				PC->SetViewTarget(Player);
			}
			UE_LOG(LogTemp, Display, TEXT("[HouseShot] %s house %s"), *Name, *Mesh);
		});
		After(At + 1.0f, [this, Out]() { MoveInput = -Out; });
		After(At + 2.4f, [this]() { Shot(TEXT("house_walk_in_1")); });
		After(At + 3.6f, [this, Player, Front, Out, Name]()
		{
			const float Ahead = FVector::DotProduct(Player->GetActorLocation() - Front, Out);
			UE_LOG(LogTemp, Display, TEXT("[HouseShot] %s: stopped %.0f cm out from the house front (negative = inside the facade line)"), *Name, Ahead);
			Shot(TEXT("house_") + Name);
		});
		At += 4.f;
	}
	After(At, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartLegShot()
{
	// Elvis's legs in play, after every retarget and adjustment: standing, running straight, running round a curve and
	// walking. Each phase logs how high each of his feet gets at its lowest (it should touch, ~0-8 cm), the hidden
	// Quinn's feet for comparison, and how far each knee bows sideways off the hip-to-ankle line; plus knee-height shots.
	UWorld* World = GetWorld();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	USkeletalMeshComponent* Elvis = nullptr;
	if (Player)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		Player->GetComponents(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetSkeletalMeshAsset() && Mesh->GetSkeletalMeshAsset()->GetName() == TEXT("SK_Elvis"))
			{
				Elvis = Mesh;
			}
		}
	}
	if (!Player || !Elvis)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LegShot] player %d elvis %d"), Player != nullptr, Elvis != nullptr);
		FPlatformMisc::RequestExit(false);
		return;
	}
	struct FLegStats { float LowL = 1e9f, LowR = 1e9f, QLowL = 1e9f, QLowR = 1e9f, BowL0 = 1e9f, BowL1 = -1e9f, BowR0 = 1e9f, BowR1 = -1e9f; float SideL = 0.f, SideR = 0.f, QSideL = 0.f, QSideR = 0.f; float HipYaw = 0.f, ChestYaw = 0.f, HipRoll = 0.f, SpineLean = 0.f; float ToeL0 = 1e9f, ToeL1 = -1e9f, ToeR0 = 1e9f, ToeR1 = -1e9f, QToeL0 = 1e9f, QToeL1 = -1e9f, QToeR0 = 1e9f, QToeR1 = -1e9f, KneeL0 = 1e9f, KneeL1 = -1e9f, KneeR0 = 1e9f, KneeR1 = -1e9f; float PToeL = 0.f, PToeR = 0.f, PQToeL = 0.f, PQToeR = 0.f; int32 PL = 0, PR = 0; TArray<float> TwL, TwR; float CurlL = 0.f, CurlR = 0.f, WristL = 0.f, WristR = 0.f; int32 NL = 0, NR = 0; int32 N = 0; };
	TSharedRef<FLegStats> Stats = MakeShared<FLegStats>();
	USkeletalMeshComponent* Quinn = Player->GetMesh();
	World->GetTimerManager().SetTimer(MoveTimer, FTimerDelegate::CreateWeakLambda(this, [this, Player, Elvis, Quinn, Stats]()
	{
		if (!MoveInput.IsNearlyZero())
		{
			Player->AddMovementInput(MoveInput.GetSafeNormal(), MoveInput.Size());
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LegShot), false, Player);
		const FVector At = Player->GetActorLocation();
		if (!GetWorld()->LineTraceSingleByChannel(Hit, At, At - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
		{
			return;
		}
		const float Ground = Hit.ImpactPoint.Z;
		auto Low = [Ground](USkeletalMeshComponent* Mesh, FName Foot, FName Toe) { return FMath::Min(Mesh->GetBoneLocation(Foot).Z, Mesh->GetBoneLocation(Toe).Z) - Ground; };
		const FVector Right = Player->GetActorRightVector();
		auto Bow = [Elvis, Right](const TCHAR* Side)
		{
			const FVector Hip = Elvis->GetBoneLocation(FName(FString(Side) + TEXT("UpLeg")));
			const FVector Knee = Elvis->GetBoneLocation(FName(FString(Side) + TEXT("Leg")));
			const FVector Ankle = Elvis->GetBoneLocation(FName(FString(Side) + TEXT("Foot")));
			return FVector::DotProduct(Knee - (Hip + Ankle) * 0.5f, Right);
		};
		FLegStats& S = *Stats;
		S.LowL = FMath::Min(S.LowL, Low(Elvis, TEXT("LeftFoot"), TEXT("LeftToeBase")));
		S.LowR = FMath::Min(S.LowR, Low(Elvis, TEXT("RightFoot"), TEXT("RightToeBase")));
		S.QLowL = FMath::Min(S.QLowL, Low(Quinn, TEXT("foot_l"), TEXT("ball_l")));
		S.QLowR = FMath::Min(S.QLowR, Low(Quinn, TEXT("foot_r"), TEXT("ball_r")));
		const float BL = Bow(TEXT("Left")), BR = Bow(TEXT("Right"));
		// Where each foot lands sideways from its own hip (+ = to his right), averaged over the moments it's down:
		// a planted foot should sit about under its hip (a few cm in); a leg slanting across shows as a big inward offset.
		auto Side = [Right](USkeletalMeshComponent* Mesh, FName Hip, FName Foot) { return FVector::DotProduct(Mesh->GetBoneLocation(Foot) - Mesh->GetBoneLocation(Hip), Right); };
		if (Elvis->GetBoneLocation(TEXT("LeftFoot")).Z - Ground < 16.f) { S.SideL += Side(Elvis, TEXT("LeftUpLeg"), TEXT("LeftFoot")); S.QSideL += Side(Quinn, TEXT("thigh_l"), TEXT("foot_l")); ++S.NL; }
		if (Elvis->GetBoneLocation(TEXT("RightFoot")).Z - Ground < 16.f) { S.SideR += Side(Elvis, TEXT("RightUpLeg"), TEXT("RightFoot")); S.QSideR += Side(Quinn, TEXT("thigh_r"), TEXT("foot_r")); ++S.NR; }
		S.BowL0 = FMath::Min(S.BowL0, BL); S.BowL1 = FMath::Max(S.BowL1, BL); S.BowR0 = FMath::Min(S.BowR0, BR); S.BowR1 = FMath::Max(S.BowR1, BR);
		// Which way his body faces relative to the way he's going (+ = turned to his right): the hip line and the shoulders.
		const FVector Fwd = Player->GetActorForwardVector();
		auto Facing = [&Fwd, &Right](const FVector& LeftSide, const FVector& RightSide) { const FVector V = LeftSide - RightSide; return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(V, Fwd), FVector::DotProduct(V, -Right))); };
		S.HipYaw += Facing(Elvis->GetBoneLocation(TEXT("LeftUpLeg")), Elvis->GetBoneLocation(TEXT("RightUpLeg")));
		S.ChestYaw += Facing(Elvis->GetBoneLocation(TEXT("LeftArm")), Elvis->GetBoneLocation(TEXT("RightArm")));
		// Which way each foot points (+ = toe turned to his right), and which way each knee points (the knee's sideways
		// offset from the hip-ankle line, signed so + = knee points out), on Elvis and on the hidden Quinn.
		{
			auto Toe = [&Fwd, &Right](USkeletalMeshComponent* Mesh, FName Foot, FName Ball)
			{
				const FVector D = Mesh->GetBoneLocation(Ball) - Mesh->GetBoneLocation(Foot);
				return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(D, Right), FVector::DotProduct(D, Fwd)));
			};
			const float TL = Toe(Elvis, TEXT("LeftFoot"), TEXT("LeftToeBase")), TR = Toe(Elvis, TEXT("RightFoot"), TEXT("RightToeBase"));
			const float QL = Toe(Quinn, TEXT("foot_l"), TEXT("ball_l")), QR = Toe(Quinn, TEXT("foot_r"), TEXT("ball_r"));
			S.ToeL0 = FMath::Min(S.ToeL0, TL); S.ToeL1 = FMath::Max(S.ToeL1, TL); S.ToeR0 = FMath::Min(S.ToeR0, TR); S.ToeR1 = FMath::Max(S.ToeR1, TR);
			S.QToeL0 = FMath::Min(S.QToeL0, QL); S.QToeL1 = FMath::Max(S.QToeL1, QL); S.QToeR0 = FMath::Min(S.QToeR0, QR); S.QToeR1 = FMath::Max(S.QToeR1, QR);
			const float KL = -BL, KR = BR;     // out = away from the middle
			// through the whole stride: each foot's twist about its own shin (+ = toe out), skipping toe-straight-down moments
			auto Twist = [&Fwd](USkeletalMeshComponent* Mesh, FName Calf, FName Foot, FName Ball, float OutSign, TArray<float>& Into)
			{
				const FVector Knee = Mesh->GetBoneLocation(Calf), Ankle = Mesh->GetBoneLocation(Foot), Toe = Mesh->GetBoneLocation(Ball);
				const FVector Shin = (Knee - Ankle).GetSafeNormal();
				const FVector F = FVector::VectorPlaneProject(Toe - Ankle, Shin), R = FVector::VectorPlaneProject(Fwd, Shin);
				if (F.Size() < 4.f || R.Size() < 0.3f) { return; }
				// + about the shin (up) turns the toe toward his right
				const float A = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(R.GetSafeNormal(), F.GetSafeNormal()), Shin), FVector::DotProduct(R.GetSafeNormal(), F.GetSafeNormal())));
				Into.Add(A * OutSign);
			};
			Twist(Elvis, TEXT("LeftLeg"), TEXT("LeftFoot"), TEXT("LeftToeBase"), -1.f, S.TwL);
			Twist(Elvis, TEXT("RightLeg"), TEXT("RightFoot"), TEXT("RightToeBase"), 1.f, S.TwR);
			// hands: finger curl (hand to the middle finger's last joint) and wrist bend (forearm vs hand), degrees
			auto Angle = [](const FVector& A, const FVector& B) { return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal()), -1.f, 1.f))); };
			for (int32 Hand = 0; Hand < 2; ++Hand)
			{
				const FString P = Hand ? TEXT("Right") : TEXT("Left");
				const FVector Elbow = Elvis->GetBoneLocation(*(P + TEXT("ForeArm"))), Wrist = Elvis->GetBoneLocation(*(P + TEXT("Hand")));
				const FVector M1 = Elvis->GetBoneLocation(*(P + TEXT("HandMiddle1"))), M2 = Elvis->GetBoneLocation(*(P + TEXT("HandMiddle2"))), M3 = Elvis->GetBoneLocation(*(P + TEXT("HandMiddle3")));
				(Hand ? S.CurlR : S.CurlL) += Angle(M1 - Wrist, M3 - M2);
				(Hand ? S.WristR : S.WristL) += Angle(Wrist - Elbow, M1 - Wrist);
			}
			// while planted: average direction, + = toe out
			if (Elvis->GetBoneLocation(TEXT("LeftFoot")).Z - Ground < 16.f) { S.PToeL += -TL; S.PQToeL += -QL; ++S.PL; }
			if (Elvis->GetBoneLocation(TEXT("RightFoot")).Z - Ground < 16.f) { S.PToeR += TR; S.PQToeR += QR; ++S.PR; }
			S.KneeL0 = FMath::Min(S.KneeL0, KL); S.KneeL1 = FMath::Max(S.KneeL1, KL); S.KneeR0 = FMath::Min(S.KneeR0, KR); S.KneeR1 = FMath::Max(S.KneeR1, KR);
		}
		// Sideways tilt (+ = to his right): one hip higher than the other, and the neck leaning off the hips.
		{
			const FVector HL = Elvis->GetBoneLocation(TEXT("LeftUpLeg")), HR = Elvis->GetBoneLocation(TEXT("RightUpLeg"));
			S.HipRoll += FMath::RadiansToDegrees(FMath::Atan2(HL.Z - HR.Z, FVector::Dist2D(HL, HR)));
			const FVector Hips = Elvis->GetBoneLocation(TEXT("Hips")), Neck = Elvis->GetBoneLocation(TEXT("Neck"));
			S.SpineLean += FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Neck - Hips, Right), Neck.Z - Hips.Z));
		}
		++S.N;
	}), 0.02f, true);
	auto Report = [Stats, Player](const TCHAR* Phase)
	{
		const FLegStats& S = *Stats;
		UE_LOG(LogTemp, Display, TEXT("[LegShot] %-10s speed %4.0f  Elvis lowest foot L %5.1f R %5.1f cm | Quinn L %5.1f R %5.1f | knee bow (+ = to his right) L %+5.1f..%+5.1f R %+5.1f..%+5.1f  (%d samples)"),
			Phase, Player->GetVelocity().Size2D(), S.LowL, S.LowR, S.QLowL, S.QLowR, S.BowL0, S.BowL1, S.BowR0, S.BowR1, S.N);
		UE_LOG(LogTemp, Display, TEXT("[LegShot] %-10s planted foot sideways from its hip (+ = to his right): Elvis L %+5.1f R %+5.1f | Quinn L %+5.1f R %+5.1f"),
			Phase, S.NL ? S.SideL / S.NL : 0.f, S.NR ? S.SideR / S.NR : 0.f, S.NL ? S.QSideL / S.NL : 0.f, S.NR ? S.QSideR / S.NR : 0.f);
		auto Spread = [](TArray<float> A) { if (A.Num() < 3) { return FString(TEXT("-")); } A.Sort(); float Sum = 0.f; for (float V : A) { Sum += V; } return FString::Printf(TEXT("mean %+4.0f (10%%-90%%: %+4.0f..%+4.0f)"), Sum / A.Num(), A[A.Num() / 10], A[A.Num() * 9 / 10]); };
		UE_LOG(LogTemp, Display, TEXT("[LegShot] %-10s hands: finger curl L %4.0f R %4.0f deg, wrist bend L %4.0f R %4.0f deg"), Phase,
			S.N ? S.CurlL / S.N : 0.f, S.N ? S.CurlR / S.N : 0.f, S.N ? S.WristL / S.N : 0.f, S.N ? S.WristR / S.N : 0.f);
		UE_LOG(LogTemp, Display, TEXT("[LegShot] %-10s ankle twist through the stride (+ = toe out): left %s | right %s"), Phase, *Spread(S.TwL), *Spread(S.TwR));
		UE_LOG(LogTemp, Display, TEXT("[LegShot] %-10s planted feet point (+ = toe out): Elvis L %+5.1f R %+5.1f deg | Quinn L %+5.1f R %+5.1f deg"),
			Phase, S.PL ? S.PToeL / S.PL : 0.f, S.PR ? S.PToeR / S.PR : 0.f, S.PL ? S.PQToeL / S.PL : 0.f, S.PR ? S.PQToeR / S.PR : 0.f);
		UE_LOG(LogTemp, Display, TEXT("[LegShot] %-10s feet point (+ = toe to his right): Elvis L %+4.0f..%+4.0f R %+4.0f..%+4.0f deg | Quinn L %+4.0f..%+4.0f R %+4.0f..%+4.0f deg | Elvis knees out (+) / in (-): L %+4.1f..%+4.1f R %+4.1f..%+4.1f cm"),
			Phase, S.ToeL0, S.ToeL1, S.ToeR0, S.ToeR1, S.QToeL0, S.QToeL1, S.QToeR0, S.QToeR1, S.KneeL0, S.KneeL1, S.KneeR0, S.KneeR1);
		UE_LOG(LogTemp, Display, TEXT("[LegShot] %-10s body facing vs travel (+ = turned to his right): hips %+5.1f deg, shoulders %+5.1f deg | tilt (+ = to his right): hips %+5.1f deg, spine %+5.1f deg"),
			Phase, S.N ? S.HipYaw / S.N : 0.f, S.N ? S.ChestYaw / S.N : 0.f, S.N ? S.HipRoll / S.N : 0.f, S.N ? S.SpineLean / S.N : 0.f);
		*Stats = FLegStats();
	};
	const FVector Ahead = Player->GetActorForwardVector().GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
	auto Knee = [this, Player](FVector Offset) { const FVector P = Player->GetActorLocation(); ShootFrom(P + Offset, P - FVector(0.f, 0.f, 40.f)); };
	After(0.1f, [Stats]() { *Stats = FLegStats(); });
	After(1.6f, [Knee, Ahead]() { Knee(Ahead * 260.f - FVector(0.f, 0.f, 50.f)); });
	After(1.9f, [this]() { Shot(TEXT("legs_0_idle_front")); });
	After(2.0f, [Knee, Side]() { Knee(Side * 260.f - FVector(0.f, 0.f, 50.f)); });
	After(2.3f, [this]() { Shot(TEXT("legs_1_idle_side")); });
	After(2.5f, [Report]() { Report(TEXT("idle")); });
	After(2.6f, [this, Ahead]() { MoveInput = Ahead; });
	After(4.0f, [Report]() { Report(TEXT("run start")); });
	After(5.4f, [Knee, Side]() { Knee(Side * 330.f - FVector(0.f, 0.f, 40.f)); });
	After(5.5f, [this]() { Shot(TEXT("legs_2_run_side")); });
	After(5.6f, [Report]() { Report(TEXT("run")); });
	After(4.8f, [this, Elvis, Player]() { const FVector H = Elvis->GetBoneLocation(TEXT("RightHand")); ShootFrom(H + Player->GetActorRightVector() * 70.f + Player->GetActorForwardVector() * 20.f + FVector(0.f, 0.f, 10.f), H); });
	After(4.85f, [this]() { Shot(TEXT("legs_hand_right")); });
	// Legs without the cape in the way (its bones hidden, which hides the skin weighted to them): from behind at hip
	// height through several strides, and side on.
	After(3.0f, [Elvis]() { for (const TCHAR* Bone : { TEXT("Cape_L_01"), TEXT("Cape_M_01"), TEXT("Cape_R_01") }) { Elvis->HideBoneByName(FName(Bone), EPhysBodyOp::PBO_None); } });
	for (int32 i = 0; i < 8; ++i)
	{
		After(3.1f + i * 0.08f, [this, Player, i]()
		{
			const FVector P = Player->GetActorLocation();
			ShootFrom(P - Player->GetActorForwardVector() * 330.f + FVector(0.f, 0.f, -10.f), P - FVector(0.f, 0.f, 40.f));
		});
		After(3.15f + i * 0.08f, [this, i]() { Shot(FString::Printf(TEXT("legs_nocape_%d"), i)); });
	}
	After(3.9f, [Elvis]() { for (const TCHAR* Bone : { TEXT("Cape_L_01"), TEXT("Cape_M_01"), TEXT("Cape_R_01") }) { Elvis->UnHideBoneByName(FName(Bone)); } });
	for (int32 i = 0; i < 8; ++i)    // the game's own follow camera, as the player sees him: running straight, then steering left
	{
		After(3.6f + i * 0.09f, [this, Player, i]() { if (APlayerController* PC = Cast<APlayerController>(Player->GetController())) { PC->SetViewTarget(Player); } Shot(FString::Printf(TEXT("legs_cam_straight_%d"), i)); });
		After(7.2f + i * 0.09f, [this, Player, i]() { if (APlayerController* PC = Cast<APlayerController>(Player->GetController())) { PC->SetViewTarget(Player); } Shot(FString::Printf(TEXT("legs_cam_left_%d"), i)); });
	}
	// From behind and a little above, like the player's camera, mid-run: Elvis, then the hidden Quinn in the same pose
	// (to tell a bent skeleton from a bent skin).
	After(4.2f, [this, Player]() { const FVector P = Player->GetActorLocation(); ShootFrom(P - Player->GetActorForwardVector() * 280.f + FVector(0.f, 0.f, 60.f), P - FVector(0.f, 0.f, 30.f)); });
	After(4.35f, [this]() { Shot(TEXT("legs_5_run_behind_elvis")); });
	After(4.4f, [Elvis, Player]() { Elvis->SetVisibility(false, true); Player->GetMesh()->SetHiddenInGame(false); Player->GetMesh()->SetVisibility(true); });
	After(4.45f, [this, Player]() { const FVector P = Player->GetActorLocation(); ShootFrom(P - Player->GetActorForwardVector() * 280.f + FVector(0.f, 0.f, 60.f), P - FVector(0.f, 0.f, 30.f)); });
	After(4.6f, [this]() { Shot(TEXT("legs_6_run_behind_quinn")); });
	After(4.7f, [Elvis, Player]() { Elvis->SetVisibility(true, true); Player->GetMesh()->SetHiddenInGame(true); });
	for (int32 i = 0; i < 60; ++i)
	{
		After(5.7f + i * 0.04f, [this, Ahead, i]() { MoveInput = Ahead.RotateAngleAxis(-i * 3.f, FVector::UpVector); });   // curving left
	}
	After(7.0f, [this, Knee, Player]() { Knee(-Player->GetActorRightVector() * 330.f + Player->GetActorForwardVector() * 80.f - FVector(0.f, 0.f, 40.f)); });
	After(7.1f, [this]() { Shot(TEXT("legs_3_curve_left")); });
	After(8.1f, [Report]() { Report(TEXT("curve left")); });
	After(8.2f, [this, Player]() { MoveInput = Player->GetActorForwardVector() * 0.3f; });
	After(9.6f, [this, Knee, Player]() { Knee(Player->GetActorRightVector() * 300.f - FVector(0.f, 0.f, 40.f)); });
	After(9.7f, [this]() { Shot(TEXT("legs_4_walk_side")); });
	After(9.8f, [Report]() { Report(TEXT("walk")); });
	After(10.0f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartStepShot()
{
	// Footsteps by surface (DustComponent logs each step during test runs): the player is dropped onto open sand,
	// the forum's paving, the basilica's stone floor, the theatre's wooden stage, a big sandstone rock and the gravel
	// road north of the town in the Rome map (L_Rome), and walks, then runs, a few seconds on each.
	UWorld* World = GetWorld();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Player)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}
	const FVector C(-44.f, -5.f, 0.f);
	struct FSpot { const TCHAR* Name; FVector2D At; float Z; FVector2D Walk; };
	const FSpot Spots[] = {
		{ TEXT("sand"), FVector2D(60.f, 60.f), 3.f, FVector2D(1.f, 0.f) },
		{ TEXT("paving"), FVector2D(-20.f, 300.f), 4.f, FVector2D(0.f, 1.f) },
		{ TEXT("basilica floor"), FVector2D(-10.f, 414.f), 5.f, FVector2D(1.f, 0.f) },
		{ TEXT("theatre stage"), FVector2D(220.f, 318.f), 5.5f, FVector2D(0.f, 1.f) },
		{ TEXT("rock"), FVector2D(1072.6f, 1473.7f), 98.f, FVector2D(0.2f, 1.f) },
		{ TEXT("gravel road"), FVector2D(1.f, 600.f), 10.f, FVector2D(0.f, 1.f) },
		{ TEXT("basilica door"), FVector2D(0.f, 386.f), 4.f, FVector2D(0.f, 1.f) } };      // in from the forum, through the middle door
	World->GetTimerManager().SetTimer(MoveTimer, FTimerDelegate::CreateWeakLambda(this, [this, Player]()
	{
		if (!MoveInput.IsNearlyZero())
		{
			Player->AddMovementInput(MoveInput.GetSafeNormal(), MoveInput.Size());
		}
	}), 0.005f, true);
	float At = 0.2f;
	for (const FSpot& Spot : Spots)
	{
		const FVector Where = C + FVector(Spot.At.X, Spot.At.Y, Spot.Z) * 100.f;
		const FVector Dir(Spot.Walk.X, Spot.Walk.Y, 0.f);
		const FString Name = Spot.Name;
		After(At, [this, Player, Where, Name]()
		{
			MoveInput = FVector::ZeroVector;
			Player->GetCharacterMovement()->StopMovementImmediately();
			Player->TeleportTo(Where, Player->GetActorRotation());
			UE_LOG(LogTemp, Display, TEXT("[StepShot] on %s"), *Name);
		});
		After(At + 2.5f, [this, Dir]() { MoveInput = Dir * 0.3f; });           // walk
		After(At + 4.5f, [this, Dir]() { MoveInput = Dir; });                  // run
		After(At + 6.f, [this, Player, Name]()
		{
			ShootFrom(Player->GetActorLocation() + FVector(-300.f, -300.f, 250.f), Player->GetActorLocation());
		});
		After(At + 6.3f, [this, Name]() { Shot(TEXT("steps_") + Name.Replace(TEXT(" "), TEXT("_"))); });
		After(At + 6.4f, [Player, C, Name]() { const FVector P = (Player->GetActorLocation() - C) / 100.f; UE_LOG(LogTemp, Display, TEXT("[StepShot] %s: ended at (%.1f, %.1f) m"), *Name, P.X, P.Y); });
		At += 6.5f;
	}
	After(At, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartMoveShot()
{
	UWorld* World = GetWorld();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	UPlayerEquipComponent* Equip = Player ? Player->FindComponentByClass<UPlayerEquipComponent>() : nullptr;
	if (!Player || !Equip)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveShot] player %d equip %d"), Player != nullptr, Equip != nullptr);
		FPlatformMisc::RequestExit(false);
		return;
	}
	const FVector Ahead = Player->GetActorForwardVector();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Ahead);
	MoveInput = FVector::ZeroVector;
	FollowOffset = -Ahead * 260.f + Side * 220.f + FVector(0.f, 0.f, 40.f);
	// Every frame: push the stick, and keep the camera riding alongside.
	World->GetTimerManager().SetTimer(MoveTimer, FTimerDelegate::CreateWeakLambda(this, [this, Player]()
	{
		if (!MoveInput.IsNearlyZero())
		{
			Player->AddMovementInput(MoveInput.GetSafeNormal(), MoveInput.Size());
		}
		const FVector At = Player->GetActorLocation();
		ShootFrom(At + FollowOffset, At + FVector(0.f, 0.f, 10.f));
	}), 0.005f, true);
	auto Log = [Player](const TCHAR* What)
	{
		UE_LOG(LogTemp, Display, TEXT("[MoveShot] %s: speed %.0f"), What, Player->GetVelocity().Size2D());
	};
	After(0.8f, [this]() { Shot(TEXT("move_0_idle")); });
	After(1.0f, [this, Ahead, Side]() { FollowOffset = Side * 330.f + FVector(0.f, 0.f, 30.f); MoveInput = Ahead * 0.22f; });
	After(2.6f, [this, Log]() { Log(TEXT("walk")); Shot(TEXT("move_1_walk")); });
	After(2.8f, [this, Ahead]() { MoveInput = Ahead; });
	After(4.2f, [this, Log]() { Log(TEXT("jog")); Shot(TEXT("move_2_jog")); });
	After(4.3f, [Equip]() { Equip->SetSprintHeld(true); });
	After(5.8f, [this, Log]() { Log(TEXT("sprint")); Shot(TEXT("move_3_sprint")); });
	After(6.0f, [this, Equip]() { Equip->SetSprintHeld(false); MoveInput = FVector::ZeroVector; });
	After(7.2f, [Equip]() { Equip->Draw(); });
	After(7.55f, [this]() { Shot(TEXT("move_4_drawing")); });
	After(8.6f, [this]() { Shot(TEXT("move_5_armed")); });
	After(8.8f, [this, Ahead]() { MoveInput = Ahead; });
	After(10.0f, [this, Log]() { Log(TEXT("armed jog")); Shot(TEXT("move_6_armed_jog")); });
	After(10.1f, [this, Player]() { MoveInput = FVector::ZeroVector; Player->Jump(); });
	After(10.4f, [this]() { Shot(TEXT("move_7_jump")); });
	// A curve while jogging: the lean.
	for (int32 i = 0; i < 40; ++i)
	{
		After(11.2f + i * 0.04f, [this, Ahead, i]() { MoveInput = Ahead.RotateAngleAxis(i * 3.f, FVector::UpVector); });
	}
	After(12.6f, [this, Log]() { Log(TEXT("turning")); Shot(TEXT("move_8_lean")); });
	After(12.8f, [this]() { MoveInput = FVector::ZeroVector; });
	After(13.6f, [Equip]() { Equip->Sheathe(); });
	After(14.05f, [this]() { Shot(TEXT("move_9_sheathing")); });
	After(15.0f, [this, Player]() { FollowOffset = -Player->GetActorForwardVector() * 190.f + FVector(0.f, 0.f, 50.f); });
	After(15.3f, [this]() { Shot(TEXT("move_10_back")); });
	After(15.8f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartPropShot()
{
	// One instance of each kind of street prop, filmed close from a three-quarter view.
	TArray<FTransform> Spots;
	TArray<FString> Names;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		if (It->GetWorld() != GetWorld() || !It->GetStaticMesh() || It->GetInstanceCount() < 3)
		{
			continue;
		}
		const FString Name = It->GetStaticMesh()->GetName();
		if (Name.StartsWith(TEXT("SM_RomePlanter")) || Name.StartsWith(TEXT("SM_RomeBasket")) || Name.StartsWith(TEXT("SM_RomeAmphorae")) || Name.StartsWith(TEXT("SM_RomePot")))
		{
			FTransform T;
			It->GetInstanceTransform(2, T, true);
			Spots.Add(T);
			Names.Add(Name);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[PropShot] %d kinds"), Spots.Num());
	for (int32 i = 0; i < FMath::Min(Spots.Num(), 6); ++i)
	{
		const FVector At = Spots[i].GetLocation() + FVector(0.f, 0.f, 35.f);
		After(0.5f + i * 1.5f, [this, At]() { ShootFrom(At + FVector(110.f, -90.f, 60.f), At); });
		After(1.4f + i * 1.5f, [this, Name = Names[i]]() { Shot(TEXT("prop_") + Name); });
	}
	After(1.0f + FMath::Min(Spots.Num(), 6) * 1.5f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartMapShot()
{
	// The Rome map (L_Rome) from the air and from the ground: over the colosseum looking north to the town and
	// temple hill, east to the oasis, south down the desert road, and standing outside the colosseum's north gate.
	const FVector C(-44.f, -5.f, 0.f);
	auto M = [&C](float X, float Y, float Z) { return C + FVector(X, Y, Z) * 100.f; };   // metres from the colosseum centre
	// The temple, for the shots of its steps and its room (it faces +X; its room's floor is 3 m up).
	FTransform Temple = FTransform(M(-396.f, 430.f, 14.f));
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		if (It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName() == TEXT("SM_RomeTemple"))
		{
			Temple = It->GetActorTransform();
		}
	}
	// A point Up metres above the ground at X, Y (metres from the colosseum centre), for street-level views.
	auto G = [this, &M](float X, float Y, float Up)
	{
		const FVector Top = M(X, Y, 300.f);
		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.f, 0.f, 60000.f), ECC_Visibility);
		return (bHit ? Hit.ImpactPoint : M(X, Y, 0.f)) + FVector(0.f, 0.f, Up * 100.f);
	};
	for (const FVector2D& P : { FVector2D(0.f, 288.f), FVector2D(20.f, 400.f), FVector2D(20.f, 450.f), FVector2D(-100.f, 500.f), FVector2D(100.f, 520.f),
		FVector2D(-200.f, 450.f), FVector2D(560.f, 170.f), FVector2D(170.f, 560.f) })
	{
		const FVector At = G(P.X, P.Y, 0.f);
		FHitResult Hit;
		TArray<FHitResult> Hits;
		GetWorld()->LineTraceMultiByChannel(Hits, M(P.X, P.Y, 300.f), M(P.X, P.Y, -600.f), ECC_Visibility);
		FString All;
		for (const FHitResult& H : Hits) { All += FString::Printf(TEXT("%s@%.2f "), H.GetActor() ? *H.GetActor()->GetActorLabel() : TEXT("?"), H.ImpactPoint.Z / 100.f); }
		GetWorld()->LineTraceSingleByChannel(Hit, M(P.X, P.Y, 300.f), M(P.X, P.Y, -600.f), ECC_Visibility);
		UE_LOG(LogTemp, Display, TEXT("[MapShot] ground at (%.0f, %.0f) m: %.2f m (%s) all: %s"), P.X, P.Y, At.Z / 100.f, Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("nothing"), *All);
	}
	// What covers a buried spot in the town: every mesh whose bounds hold it (collision or not).
	{
		const FVector Probe = M(20.f, 450.f, 6.f);
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->IsRegistered() && It->Bounds.GetBox().IsInside(Probe) && !It->IsA<ULandscapeHeightfieldCollisionComponent>())
			{
				const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(*It);
				UE_LOG(LogTemp, Display, TEXT("[MapShot] covering: %s / %s mesh %s visible %d collision %d at %s"), It->GetOwner() ? *It->GetOwner()->GetActorLabel() : TEXT("?"),
					*It->GetName(), SM && SM->GetStaticMesh() ? *SM->GetStaticMesh()->GetName() : TEXT("-"), It->IsVisible(), (int32)It->GetCollisionEnabled(), *It->GetComponentLocation().ToString());
			}
		}
	}
	struct FView { const TCHAR* Name; FVector Eye; FVector Look; };
	const FView Views[] = {
		{ TEXT("map_town_air"), M(-170.f, 60.f, 110.f), M(0.f, 330.f, 0.f) },
		{ TEXT("map_forum_top"), M(0.f, 300.f, 260.f), M(0.f, 330.f, 0.f) },
		{ TEXT("map_cardo"), G(0.f, 70.f, 1.8f), G(0.f, 300.f, 3.f) },
		{ TEXT("map_forum"), G(-38.f, 288.f, 1.8f), G(20.f, 360.f, 2.f) },
		{ TEXT("map_street"), G(56.f, 186.f, 1.8f), G(56.f, 300.f, 2.f) },
		{ TEXT("map_decumanus"), G(-150.f, 330.f, 1.8f), G(0.f, 330.f, 3.f) },
		{ TEXT("map_roofs"), M(-60.f, 160.f, 30.f), M(20.f, 330.f, 0.f) },
		{ TEXT("map_temple"), Temple.TransformPosition(FVector(5200.f, 1400.f, 900.f)), Temple.TransformPosition(FVector(0.f, 0.f, 700.f)) },
		{ TEXT("map_temple_front"), Temple.TransformPosition(FVector(4600.f, -900.f, 450.f)), Temple.TransformPosition(FVector(1300.f, 0.f, 800.f)) },
		{ TEXT("map_temple_inside"), Temple.TransformPosition(FVector(550.f, 250.f, 480.f)), Temple.TransformPosition(FVector(-1300.f, -100.f, 500.f)) },
		{ TEXT("map_north"), C + FVector(0.f, -22000.f, 16000.f), C + FVector(0.f, 30000.f, 0.f) },
		{ TEXT("map_gate"), C + FVector(1200.f, 9000.f, 250.f), C + FVector(0.f, 3500.f, 700.f) },
		{ TEXT("map_oasis"), C + FVector(40000.f, 6000.f, 1800.f), C + FVector(56000.f, 17000.f, -300.f) },
		// The landmarks (build_rome_landmarks.py): the basilica on the forum, the baths, the theatre, the town walls.
		{ TEXT("map_landmarks_air"), M(330.f, 170.f, 90.f), M(40.f, 360.f, 0.f) },
		{ TEXT("map_basilica"), G(-6.f, 352.f, 1.8f), M(0.f, 405.f, 10.f) },
		{ TEXT("map_basilica_inside"), M(-38.f, 414.5f, 5.3f), M(30.f, 414.5f, 8.f) },
		{ TEXT("map_baths"), G(-66.f, 336.f, 1.8f), M(-81.f, 302.f, 11.f) },
		{ TEXT("map_baths_air"), M(-35.f, 255.f, 45.f), M(-81.f, 299.f, 5.f) },
		{ TEXT("map_theatre"), M(150.f, 268.f, 38.f), M(212.f, 330.f, 0.f) },
		{ TEXT("map_theatre_stage"), M(220.f, 333.f, 5.2f), M(188.f, 330.f, 7.f) },
		{ TEXT("map_wall_gate"), G(14.f, 532.f, 1.8f), M(0.f, 490.f, 7.f) },
		// The land beyond the town (rterrain.py): the northern range and its pass, mesas and buttes, the canyon, the
		// dune sea. Heights come from the heightmap, since far tiles may not have collision loaded around the camera.
		{ TEXT("map_range"), M(0.f, 300.f, 110.f), M(0.f, 1150.f, 90.f) },
		{ TEXT("map_range_gate"), M(6.f, 525.f, 4.9f), M(-120.f, 1100.f, 70.f) },
		{ TEXT("map_pass"), M(4.f, 950.f, 34.4f), M(0.f, 1400.f, 45.f) },
		{ TEXT("map_mesas"), M(560.f, 300.f, 3.4f), M(980.f, 470.f, 40.f) },
		{ TEXT("map_mesas_air"), M(250.f, -150.f, 110.f), M(800.f, -500.f, 20.f) },
		{ TEXT("map_canyon"), M(-520.f, -449.f, -0.8f), M(-780.f, -590.f, 3.f) },
		{ TEXT("map_canyon_air"), M(-420.f, -250.f, 120.f), M(-850.f, -650.f, 0.f) },
		{ TEXT("map_dunes"), M(0.f, -500.f, 43.3f), M(150.f, -1200.f, 50.f) },
		{ TEXT("map_from_temple"), M(-300.f, 500.f, 32.f), M(-820.f, 1000.f, 100.f) },
		{ TEXT("map_rocks"), M(770.f, 431.f, 17.f), M(789.f, 450.f, 13.f) } };          // fallen blocks at a mesa's foot (build_rome_rocks.py)
	float At = 0.2f;
	for (const FView& View : Views)
	{
		const FVector Eye = View.Eye, Look = View.Look;
		const FString Name = View.Name;
		After(At, [this, Eye, Look]() { ShootFrom(Eye, Look); });
		// Give streaming a moment to bring in the landscape around each view.
		After(At + 2.5f, [this, Name]() { Shot(Name); });
		At += 3.f;
	}
	After(At, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartDragonShot()
{
	// The dragon beast (/Game/Dragon), standing in front of the player so their sizes compare, playing each of his
	// retargeted animations in turn: idle, walk, jog, the swipes and attacks, then death.
	UWorld* World = GetWorld();
	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Dragon/SK_DragonBeast.SK_DragonBeast"));
	if (!Player || !Mesh)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}
	// Textures and shaders may still be building on a first run; film only once they're done.
	FAssetCompilingManager::Get().FinishAllCompilation();
	const FVector Forward = Player->GetActorForwardVector().GetSafeNormal2D();
	FVector Base = Player->GetActorLocation() + Forward * 600.f;
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Base + FVector(0.f, 0.f, 300.f), Base - FVector(0.f, 0.f, 600.f), ECC_Visibility))
	{
		Base = Hit.ImpactPoint;
	}
	ASkeletalMeshActor* Dragon = World->SpawnActor<ASkeletalMeshActor>(Base, FRotator(0.f, (-Forward).Rotation().Yaw, 0.f));
	if (!Dragon)
	{
		return;
	}
	USkeletalMeshComponent* Skin = Dragon->GetSkeletalMeshComponent();
	Skin->SetMobility(EComponentMobility::Movable);
	Skin->SetSkeletalMesh(Mesh);
	Skin->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// The mesh faces +Y in Unreal (made facing -Y in Blender), like the mannequin; turn it to face down the actor's +X.
	Skin->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	const FVector Facing = -Forward;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Facing);
	const FVector Chest = Base + FVector(0.f, 0.f, 130.f);

	// A soft key light for the test, so his scales and textures show (the arena is dark here).
	if (APointLight* Key = World->SpawnActor<APointLight>(Chest + Facing * 300.f + Side * 200.f + FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator))
	{
		Key->PointLightComponent->SetMobility(EComponentMobility::Movable);
		Key->PointLightComponent->SetIntensityUnits(ELightUnits::Candelas);
		Key->PointLightComponent->SetIntensity(120.f);
		Key->PointLightComponent->SetAttenuationRadius(1500.f);
		Key->PointLightComponent->SetLightColor(FLinearColor(1.f, 0.85f, 0.7f));
	}

	// Size check first: side-on to him and the player together.
	const FVector Between = (Base + Player->GetActorLocation()) * 0.5f;
	After(0.2f, [this, Between, Side]() { ShootFrom(Between + Side * 950.f + FVector(0.f, 0.f, 150.f), Between + FVector(0.f, 0.f, 110.f)); });
	After(0.8f, [this]() { Shot(TEXT("dragon_00_compare")); });

	enum class EView : uint8 { Front, Side, Head };
	struct FClip { const TCHAR* Name; float ShotAfter; EView View; };
	const FClip Clips[] = { { TEXT("Idle"), 0.8f, EView::Front }, { TEXT("Idle"), 0.5f, EView::Head }, { TEXT("Walk"), 0.7f, EView::Side },
		{ TEXT("Jog"), 0.6f, EView::Front }, { TEXT("Swipe"), 0.75f, EView::Front }, { TEXT("HeavySwipe"), 0.7f, EView::Front },
		{ TEXT("SpinAttack"), 0.9f, EView::Front }, { TEXT("JumpAttack"), 1.0f, EView::Side }, { TEXT("Death"), 2.0f, EView::Front } };
	float At = 1.0f;
	int32 Index = 1;
	for (const FClip& Clip : Clips)
	{
		UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Dragon/Animations/A_DragonBeast_%s.A_DragonBeast_%s"), Clip.Name, Clip.Name));
		if (!Anim)
		{
			continue;
		}
		const bool bLoop = FCString::Strcmp(Clip.Name, TEXT("Death")) != 0;
		FVector Eye = Chest + Facing * 520.f + Side * 160.f;
		FVector Look = Chest - FVector(0.f, 0.f, 20.f);
		if (Clip.View == EView::Side)
		{
			Eye = Chest + Side * 560.f + Facing * 100.f;
		}
		else if (Clip.View == EView::Head)
		{
			Eye = Chest + FVector(0.f, 0.f, 70.f) + Facing * 190.f + Side * 60.f;
			Look = Chest + FVector(0.f, 0.f, 85.f);
		}
		After(At, [this, Skin, Anim, bLoop, Eye, Look]()
		{
			Skin->PlayAnimation(Anim, bLoop);
			// Nothing drives him yet, so play every move on the spot.
			if (UAnimInstance* AnimInstance = Skin->GetAnimInstance())
			{
				AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			}
			ShootFrom(Eye, Look);
		});
		const FString ShotName = FString::Printf(TEXT("dragon_%02d_%s%s"), Index++, Clip.Name,
			Clip.View == EView::Side ? TEXT("_side") : (Clip.View == EView::Head ? TEXT("_head") : TEXT("")));
		After(At + Clip.ShotAfter, [this, ShotName]() { Shot(ShotName); });
		At += Clip.ShotAfter + 0.4f;
	}
	After(At + 0.3f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartRomeShot()
{
	// Close-ups of RomeDressingSubsystem's pieces, found by mesh name: a banner (twice, to see it move), an eagle
	// standard, a palm growing in the rubble, then the gate end of the arena.
	UWorld* World = GetWorld();
	auto Find = [World](const TCHAR* Prefix, FTransform& Out)
	{
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() != World || !It->GetStaticMesh() || !It->GetStaticMesh()->GetName().StartsWith(Prefix))
			{
				continue;
			}
			if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(*It))
			{
				// The first instance inside the arena.
				for (int32 i = 0; i < Instances->GetInstanceCount(); ++i)
				{
					FTransform Instance;
					Instances->GetInstanceTransform(i, Instance, true);
					if (FVector::Dist2D(Instance.GetLocation(), FVector(-44.f, -5.f, 0.f)) < 3300.f)
					{
						Out = Instance;
						return true;
					}
				}
				continue;
			}
			Out = It->GetComponentTransform();
			return true;
		}
		return false;
	};
	FTransform Banner, Standard, Palm;
	if (Find(TEXT("SM_BannerSPQR"), Banner))
	{
		const FVector Face = Banner.GetRotation().GetForwardVector();
		const FVector Middle = Banner.GetLocation() - FVector(0.f, 0.f, 160.f);
		After(0.2f, [this, Face, Middle]() { ShootFrom(Middle + Face * 520.f - FVector(0.f, 0.f, 20.f) + Face.Cross(FVector::UpVector) * 120.f, Middle); });
		After(0.6f, [this]() { Shot(TEXT("rome_banner")); });
		After(1.4f, [this]() { Shot(TEXT("rome_banner_b")); });
	}
	if (Find(TEXT("SM_EagleStandard"), Standard))
	{
		const FVector Top = Standard.GetLocation() + FVector(0.f, 0.f, 290.f);
		const FVector Facing = Standard.GetRotation().GetForwardVector();
		After(1.6f, [this, Top, Facing]() { ShootFrom(Top + Facing * 330.f - FVector(0.f, 0.f, 90.f) + Facing.Cross(FVector::UpVector) * 140.f, Top - FVector(0.f, 0.f, 40.f)); });
		After(2.0f, [this]() { Shot(TEXT("rome_standard")); });
	}
	if (Find(TEXT("SM_DatePalm"), Palm))
	{
		const FVector Base = Palm.GetLocation();
		const FVector ToCentre = (FVector(-44.f, -5.f, Base.Z) - Base).GetSafeNormal();
		After(2.2f, [this, Base, ToCentre]() { ShootFrom(Base + ToCentre * 1500.f + FVector(0.f, 0.f, 260.f), Base + FVector(0.f, 0.f, 700.f)); });
		After(2.6f, [this]() { Shot(TEXT("rome_palm")); });
	}
	After(2.8f, [this]() { ShootFrom(FVector(-44.f, -700.f, 260.f), FVector(-44.f, 2600.f, 250.f)); });
	After(3.2f, [this]() { Shot(TEXT("rome_gate")); });
	// Down the gate tunnel at +Y, and the colosseum's outside face there.
	After(3.4f, [this]() { ShootFrom(FVector(-44.f, 2150.f, 170.f), FVector(-44.f, 4500.f, 170.f)); });
	After(3.8f, [this]() { Shot(TEXT("rome_tunnel")); });
	After(4.0f, [this]() { ShootFrom(FVector(1400.f, 8200.f, 900.f), FVector(-44.f, 3900.f, 300.f)); });
	After(4.4f, [this]() { Shot(TEXT("rome_outside")); });
	After(4.8f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartOtterShot()
{
	// The rigged otter Vulcan, close up: statue, waking up, each attack, a flinch, death. The player can't die
	// (a death would reload the level mid-run) and Vulcan only does the attacks asked for.
	UWorld* World = GetWorld();
	TActorIterator<AVulcanBoss> It(World);
	AVulcanBoss* Boss = It ? *It : nullptr;
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!Boss || !Player)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (UHealthComponent* PlayerHealth = Player->FindComponentByClass<UHealthComponent>())
	{
		PlayerHealth->bInvulnerable = true;
	}

	// Open sand (-ShotAtX= -ShotAtY=): Vulcan there, the player 6 m in front of him.
	const TCHAR* CommandLine = FCommandLine::Get();
	float AtX = -300.f, AtY = -1700.f;
	FParse::Value(CommandLine, TEXT("ShotAtX="), AtX);
	FParse::Value(CommandLine, TEXT("ShotAtY="), AtY);
	Boss->SetActorLocationAndRotation(FVector(AtX, AtY, 320.f), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	Player->SetActorLocationAndRotation(FVector(AtX + 600.f, AtY, 250.f), FRotator(0.f, 180.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Boss->AggroRange = 1.f;

	TWeakObjectPtr<AVulcanBoss> WeakBoss = Boss;
	float Yaw = 40.f;
	FParse::Value(CommandLine, TEXT("ShotYaw="), Yaw);
	auto Film = [this, WeakBoss, Yaw](const FString& Name)
	{
		if (AVulcanBoss* B = WeakBoss.Get())
		{
			const FVector Target = B->GetActorLocation() + FVector(0.f, 0.f, 30.f);
			ShootFrom(Target + B->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * 520.f + FVector(0.f, 0.f, 90.f), Target);
			// A fill light on the camera: the storm sky leaves him a silhouette otherwise.
			if (ShotCamera && !ShotCamera->FindComponentByClass<UPointLightComponent>())
			{
				UPointLightComponent* Fill = NewObject<UPointLightComponent>(ShotCamera);
				Fill->SetupAttachment(ShotCamera->GetRootComponent());
				Fill->RegisterComponent();
				Fill->SetIntensityUnits(ELightUnits::Candelas);
				Fill->SetIntensity(600.f);
				Fill->SetAttenuationRadius(2500.f);
				Fill->SetCastShadows(false);
			}
		}
		Shot(Name);
	};
	auto Attack = [WeakBoss](EVulcanAttack Which) { if (AVulcanBoss* B = WeakBoss.Get()) { B->PerformAttack(Which); } };
	auto Hit = [WeakBoss, World](float Amount)
	{
		if (AVulcanBoss* B = WeakBoss.Get())
		{
			UGameplayStatics::ApplyDamage(B, Amount, UGameplayStatics::GetPlayerController(World, 0), UGameplayStatics::GetPlayerPawn(World, 0), UDamageType::StaticClass());
		}
	};
	auto Burst = [this, Film](const TCHAR* Name, float Start, float Step, int32 Count)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const FString ShotName = FString::Printf(TEXT("otter_%s_%02d"), Name, i);
			After(Start + i * Step, [Film, ShotName]() { Film(ShotName); });
		}
	};

	Burst(TEXT("a_statue"), 0.6f, 0.3f, 2);
	After(1.2f, [WeakBoss]() { if (AVulcanBoss* B = WeakBoss.Get()) { B->StartFight(); } });
	if (FParse::Param(CommandLine, TEXT("OtterChase")))
	{
		// Run instead: no attacks, the player 22 m away along a line with nothing in Vulcan's way, Vulcan chases.
		After(8.6f, [WeakBoss, Player, World]()
		{
			if (AVulcanBoss* B = WeakBoss.Get())
			{
				B->bEnableTailLash = B->bEnableSpit = B->bEnableBreath = B->bEnableDive = false;
				B->AggroRange = 4000.f;
				const UCapsuleComponent* Capsule = B->GetCapsuleComponent();
				const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight() * 0.8f);
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(B);
				Params.AddIgnoredActor(Player);
				const FVector Start = B->GetActorLocation() + FVector(0.f, 0.f, 20.f);
				FVector Dir = FVector::XAxisVector;
				for (int32 i = 0; i < 16; ++i)
				{
					const FVector Try = FVector::XAxisVector.RotateAngleAxis(i * 22.5f, FVector::UpVector);
					FHitResult Hit;
					if (!World->SweepSingleByChannel(Hit, Start, Start + Try * 2400.f, FQuat::Identity, ECC_Pawn, Shape, Params))
					{
						Dir = Try;
						break;
					}
				}
				Player->SetActorLocation(B->GetActorLocation() + Dir * 2200.f, false, nullptr, ETeleportType::TeleportPhysics);
			}
		});
		Burst(TEXT("z_chase"), 9.0f, 0.1f, 24);
		for (float T = 9.f; T < 12.f; T += 0.5f)
		{
			After(T, [WeakBoss, Player, T]()
			{
				if (const AVulcanBoss* B = WeakBoss.Get())
				{
					const AAIController* AI = Cast<AAIController>(B->GetController());
					UE_LOG(LogTemp, Warning, TEXT("[OtterChase] t=%.1f speed=%.0f statue=%d attack=%d dist=%.0f move=%d boss=%s player=%s"), T, B->GetVelocity().Size2D(),
						B->IsStatue() ? 1 : 0, int32(B->GetCurrentAttack()), FVector::Dist2D(B->GetActorLocation(), Player->GetActorLocation()),
						AI ? int32(AI->GetMoveStatus()) : -1, *B->GetActorLocation().ToCompactString(), *Player->GetActorLocation().ToCompactString());
				}
			});
		}
		After(13.2f, []() { FPlatformMisc::RequestExit(false); });
		return;
	}
	Burst(TEXT("b_awaken"), 1.4f, 0.6f, 12);          // lightning, shaking, wake-up roar
	After(9.2f, [Attack]() { Attack(EVulcanAttack::TailLash); });
	Burst(TEXT("c_taillash"), 9.2f, 0.12f, 13);
	After(11.6f, [Attack]() { Attack(EVulcanAttack::ObsidianSpit); });
	Burst(TEXT("d_spit"), 11.6f, 0.12f, 10);
	After(13.8f, [Attack]() { Attack(EVulcanAttack::MoltenBreath); });
	Burst(TEXT("e_breath"), 13.8f, 0.35f, 12);
	After(18.8f, [Attack]() { Attack(EVulcanAttack::MagmaDive); });
	Burst(TEXT("f_dive"), 18.8f, 0.25f, 18);
	After(24.0f, [Hit]() { Hit(5.f); });
	Burst(TEXT("g_hit"), 24.0f, 0.15f, 5);
	After(25.2f, [Hit]() { Hit(100000.f); });
	Burst(TEXT("h_death"), 25.2f, 0.3f, 10);
	After(28.8f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartElvisShot()
{
	// Elvis's cape through the moves that bend it most: a short run, a dodge roll, then dying.
	UWorld* World = GetWorld();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	if (!Player || !Controller)
	{
		FPlatformMisc::RequestExit(false);
		return;
	}

	// Open ground, facing +X (-ShotAtX= -ShotAtY= to pick another spot).
	const TCHAR* CommandLine = FCommandLine::Get();
	float AtX = 1200.f, AtY = 0.f;
	FParse::Value(CommandLine, TEXT("ShotAtX="), AtX);
	FParse::Value(CommandLine, TEXT("ShotAtY="), AtY);
	Player->SetActorLocationAndRotation(FVector(AtX, AtY, Player->GetActorLocation().Z + 100.f), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	Controller->SetControlRotation(FRotator::ZeroRotator);

	// Film from behind and to the side so the cape shows.
	float Yaw = 140.f;
	float Distance = 380.f;
	FParse::Value(CommandLine, TEXT("ShotYaw="), Yaw);
	FParse::Value(CommandLine, TEXT("ShotDist="), Distance);
	const FVector Target = Player->GetActorLocation() + FVector(0.f, 0.f, -20.f);
	const FVector Offset = Player->GetActorForwardVector().RotateAngleAxis(Yaw, FVector::UpVector) * Distance + FVector(0.f, 0.f, 60.f);
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(Target + Offset, (-Offset).Rotation());
	Camera->AttachToActor(Player, FAttachmentTransformRules::KeepWorldTransform);
	Controller->SetViewTarget(Camera);

	After(0.6f, [this]() { Shot(TEXT("elvis_idle")); });
	for (int32 i = 0; i <= 20; ++i)
	{
		After(0.8f + i * 0.02f, [Player]() { Player->AddMovementInput(Player->GetActorForwardVector(), 1.f); });
	}
	After(1.0f, [this]() { Shot(TEXT("elvis_run")); });
	After(1.24f, [Player]() { if (UDodgeComponent* Dodge = Player->FindComponentByClass<UDodgeComponent>()) { Dodge->TryDodge(); } });
	for (int32 i = 1; i <= 10; ++i)
	{
		After(1.24f + i * 0.1f, [this, i]() { Shot(FString::Printf(TEXT("elvis_roll_%02d"), i)); });
	}
	After(3.0f, [Player, Controller]() { UGameplayStatics::ApplyDamage(Player, 100000.f, Controller, Player, UDamageType::StaticClass()); });
	for (int32 i = 1; i <= 10; ++i)
	{
		After(3.0f + i * 0.3f, [this, i]() { Shot(FString::Printf(TEXT("elvis_death_%02d"), i)); });
	}
	After(6.4f, []() { FPlatformMisc::RequestExit(false); });
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
