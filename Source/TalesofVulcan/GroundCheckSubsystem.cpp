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
		|| FParse::Param(CommandLine, TEXT("ElvisShot")) || FParse::Param(CommandLine, TEXT("OtterShot")) || FParse::Param(CommandLine, TEXT("RomeShot")) || FParse::Param(CommandLine, TEXT("DragonShot")) || FParse::Param(CommandLine, TEXT("MapShot")) || FParse::Param(CommandLine, TEXT("ExitShot"));
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
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("SpearShots") / (Name + TEXT(".png")), Name.StartsWith(TEXT("lock")), false);
}

void UGroundCheckSubsystem::StartExitShot()
{
	// The way out of the colosseum (L_Rome): Vulcan is killed, the north gate's portcullis rises, the player steps
	// into it and comes out on the plaza outside, facing the town.
	UWorld* World = GetWorld();
	TActorIterator<AVulcanBoss> BossIt(World);
	AVulcanBoss* Boss = BossIt ? *BossIt : nullptr;
	TActorIterator<ARomeExitGate> GateIt(World);
	ARomeExitGate* Gate = GateIt ? *GateIt : nullptr;
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	if (!Boss || !Gate || !Player || !Controller)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ExitShot] boss %d gate %d player %d"), Boss != nullptr, Gate != nullptr, Player != nullptr);
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (UHealthComponent* PlayerHealth = Player->FindComponentByClass<UHealthComponent>())
	{
		PlayerHealth->bInvulnerable = true;
	}
	Boss->StartFight();
	const FVector Into = Gate->GetActorForwardVector();
	const FVector Front = Gate->GetActorLocation() - Into * 900.f + FVector(0.f, 0.f, 220.f);
	const FVector Look = Gate->GetActorLocation() + FVector(0.f, 0.f, 220.f);
	After(5.5f, [this, Front, Look]() { ShootFrom(Front, Look); });
	After(6.0f, [this]() { Shot(TEXT("exit_0_shut")); });
	After(6.2f, [World, Boss, Player]() { UGameplayStatics::ApplyDamage(Boss, 100000.f, UGameplayStatics::GetPlayerController(World, 0), Player, UDamageType::StaticClass()); });
	After(11.0f, [this]() { Shot(TEXT("exit_1_rising")); });
	After(14.5f, [this]() { Shot(TEXT("exit_2_open")); });
	// Walk in: put the player just in front of the gate, then into the passage.
	After(14.7f, [Player, Gate, Into]()
	{
		Player->SetActorLocation(Gate->GetActorLocation() - Into * 200.f + FVector(0.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	After(15.0f, [Player, Gate, Into]()
	{
		Player->SetActorLocation(Gate->GetActorLocation() + Into * 25.f + FVector(0.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	After(17.5f, [Controller, Player]() { Controller->SetViewTarget(Player); });
	After(18.5f, [this]() { Shot(TEXT("exit_3_outside")); });
	After(18.7f, [this, Player]()
	{
		const FVector At = Player->GetActorLocation();
		UE_LOG(LogTemp, Display, TEXT("[ExitShot] player now at %s"), *At.ToString());
		ShootFrom(At + FVector(-900.f, 1400.f, 700.f), At + FVector(0.f, -1500.f, 600.f));
	});
	After(19.2f, [this]() { Shot(TEXT("exit_4_around")); });
	After(19.6f, []() { FPlatformMisc::RequestExit(false); });
}

void UGroundCheckSubsystem::StartMapShot()
{
	// The Rome map (L_Rome) from the air and from the ground: over the colosseum looking north to the town and
	// temple hill, east to the oasis, south down the desert road, and standing outside the colosseum's north gate.
	const FVector C(-44.f, -5.f, 0.f);
	struct FView { const TCHAR* Name; FVector Eye; FVector Look; };
	const FView Views[] = {
		{ TEXT("map_north"), C + FVector(0.f, -22000.f, 16000.f), C + FVector(0.f, 30000.f, 0.f) },
		{ TEXT("map_high"), C + FVector(-60000.f, -60000.f, 70000.f), C },
		{ TEXT("map_oasis"), C + FVector(40000.f, 6000.f, 1800.f), C + FVector(56000.f, 17000.f, -300.f) },
		{ TEXT("map_oasis_shore"), C + FVector(47500.f, 13500.f, 900.f), C + FVector(58000.f, 18500.f, -150.f) },
		{ TEXT("map_south"), C + FVector(0.f, -9000.f, 2500.f), C + FVector(0.f, -120000.f, 2000.f) },
		{ TEXT("map_gate"), C + FVector(1200.f, 9000.f, 250.f), C + FVector(0.f, 3500.f, 700.f) },
		{ TEXT("map_temple"), C + FVector(-20000.f, 20000.f, 2400.f), C + FVector(-39000.f, 43000.f, 1400.f) } };
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
