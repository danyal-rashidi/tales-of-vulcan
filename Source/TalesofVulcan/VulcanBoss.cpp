#include "VulcanBoss.h"
#include "HealthComponent.h"
#include "VulcanProjectile.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AVulcanBoss::AVulcanBoss()
{
	PrimaryActorTick.bCanEverTick = true;

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
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));

	// Boss-sized, and easy to tell apart from the player mannequin.
	GetCapsuleComponent()->SetRelativeScale3D(FVector(1.6f));

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

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	HealthComponent->OnHealthChanged.AddDynamic(this, &AVulcanBoss::HandleHealthChanged);
	HealthComponent->OnDeath.AddDynamic(this, &AVulcanBoss::HandleDeath);

	if (bStartFightOnBeginPlay)
	{
		StartFight();
	}

	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AVulcanBoss::Think, FMath::Max(ThinkInterval, 0.05f), true);
}

void AVulcanBoss::StartFight()
{
	if (bFightActive || bDead)
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
	Schedule(AttackTimer, &AVulcanBoss::DiveSubmerge, DiveSubmergeTime / GetSpeedScale());
}

void AVulcanBoss::DiveSubmerge()
{
	OnDiveSubmerged(GetFeetLocation());

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
