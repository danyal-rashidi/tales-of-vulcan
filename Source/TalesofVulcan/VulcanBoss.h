#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VulcanBoss.generated.h"

class UAnimMontage;
class UHealthComponent;
class AVulcanProjectile;

UENUM(BlueprintType)
enum class EVulcanAttack : uint8
{
	None,
	TailLash,
	ObsidianSpit,
	MoltenBreath,
	MagmaDive
};

/**
 * Vulcan, the Silver Tide.
 *
 * Self-contained boss: no Behavior Tree needed. Every ThinkInterval seconds it
 * either chases the player or picks an attack based on distance. Hit timing is
 * driven by timers, so montages are purely visual (no anim notifies required).
 *
 * Make a Blueprint child (BP_Vulcan), assign mesh/montages, and hook the
 * "On ..." events below to particle effects and sounds.
 */
UCLASS()
class TALESOFVULCAN_API AVulcanBoss : public ACharacter
{
	GENERATED_BODY()

public:
	AVulcanBoss();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vulcan")
	TObjectPtr<UHealthComponent> HealthComponent;

	// ---------------------------------------------------------------- General

	/** Off = Vulcan waits until StartFight is called (e.g. from an arena trigger box). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	bool bStartFightOnBeginPlay = true;

	/** Draws hitboxes, cones and warning circles while testing. Turn off for the demo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	bool bShowDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	float AggroRange = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	float WalkSpeed = 400.f;

	/** Pause between attacks, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	float AttackCooldown = 1.2f;

	/** Inside this distance Vulcan prefers Tail Lash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	float MeleeRange = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	float ThinkInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	TObjectPtr<UAnimMontage> DeathMontage;

	// ---------------------------------------------------------------- Phase 2 "Eruption"

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Phase Two", meta=(ClampMin="0", ClampMax="1"))
	float PhaseTwoHealthPercent = 0.5f;

	/** Animations, timings and walk speed are all multiplied by this in phase 2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Phase Two")
	float PhaseTwoSpeedMultiplier = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Phase Two")
	float PhaseTwoDamageMultiplier = 1.25f;

	// ---------------------------------------------------------------- Tail Lash

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Tail Lash")
	bool bEnableTailLash = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Tail Lash")
	TObjectPtr<UAnimMontage> TailLashMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Tail Lash")
	float TailLashDamage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Tail Lash")
	float TailLashRange = 350.f;

	/** Total width of the swipe in front of Vulcan. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Tail Lash")
	float TailLashArcDegrees = 160.f;

	/** Seconds from start of the attack until the hit lands. Match it to the animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Tail Lash")
	float TailLashHitDelay = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Tail Lash")
	float TailLashDuration = 1.5f;

	// ---------------------------------------------------------------- Obsidian Spit

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit")
	bool bEnableSpit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit")
	TObjectPtr<UAnimMontage> SpitMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit")
	TSubclassOf<AVulcanProjectile> ProjectileClass;

	/** Where shards spawn, relative to Vulcan (X forward, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit")
	FVector SpitMuzzleOffset = FVector(120.f, 0.f, 60.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit")
	float SpitFireDelay = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit")
	float SpitDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit", meta=(ClampMin="1"))
	int32 SpitShardCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit", meta=(ClampMin="1"))
	int32 PhaseTwoSpitShardCount = 3;

	/** Yaw spread to each side when firing several shards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Obsidian Spit")
	float SpitSpreadDegrees = 12.f;

	// ---------------------------------------------------------------- Molten Breath (flamethrower)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	bool bEnableBreath = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	TObjectPtr<UAnimMontage> BreathMontage;

	/** Inhale time before fire starts: the player's cue to move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float BreathWindup = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float BreathDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float BreathRange = 600.f;

	/** Half the cone width (15 = 30 degree cone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float BreathHalfAngle = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float BreathDamagePerTick = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float BreathTickInterval = 0.1f;

	/** How fast the flames sweep toward the player. Lower = easier to outrun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float BreathTurnRate = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float PhaseTwoBreathExtraRange = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float PhaseTwoBreathExtraHalfAngle = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Molten Breath")
	float PhaseTwoBreathExtraDuration = 1.0f;

	// ---------------------------------------------------------------- Magma Dive

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	bool bEnableDive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	TObjectPtr<UAnimMontage> DiveMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	TObjectPtr<UAnimMontage> EmergeMontage;

	/** Time spent playing the dive animation before vanishing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	float DiveSubmergeTime = 0.6f;

	/** Time fully hidden before the warning circle appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	float DiveHiddenTime = 0.8f;

	/** Time the warning circle is shown before Vulcan bursts out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	float DiveWarningTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	float DiveRecoverTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	float DiveDamage = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	float DiveRadius = 300.f;

	/** Phase 2: the emerge spot keeps burning for this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	float BurnPatchDuration = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Magma Dive")
	float BurnPatchDamagePerTick = 5.f;

	// ---------------------------------------------------------------- Functions

	/** Call from an arena trigger if bStartFightOnBeginPlay is off. */
	UFUNCTION(BlueprintCallable, Category="Vulcan")
	void StartFight();

	UFUNCTION(BlueprintPure, Category="Vulcan")
	bool IsInPhaseTwo() const { return bPhaseTwo; }

	UFUNCTION(BlueprintPure, Category="Vulcan")
	bool IsDefeated() const { return bDead; }

	UFUNCTION(BlueprintPure, Category="Vulcan")
	EVulcanAttack GetCurrentAttack() const { return CurrentAttack; }

	// ---------------------------------------------------------------- Blueprint hooks (visuals / sound / UI)

	/** Show the boss health bar here. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnFightStarted();

	/** Brighten the lava cracks, play a roar, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnPhaseTwoStarted();

	/** Cool the cracks to black, show "PROJECT SUBMITTED". */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnVulcanDefeated();

	/** Inhale: glow the mouth, play a charge-up sound. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnBreathWindup();

	/** Activate the fire Niagara effect attached to the mouth. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnBreathStarted();

	/** Deactivate the fire effect. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnBreathEnded();

	/** Lava splash where Vulcan disappears. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnDiveSubmerged(FVector Location);

	/** Spawn a glowing warning decal at Location (ground level). */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnDiveWarning(FVector Location, float Radius);

	/** Eruption effect + camera shake. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnDiveEmerged(FVector Location);

	/** Phase 2 only: spawn a fire patch effect for Duration seconds. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnBurnPatchStarted(FVector Location, float Radius, float Duration);

protected:
	virtual void BeginPlay() override;

private:
	void Think();
	bool TryStartAttack(float DistanceToPlayer);
	void FinishAttack();

	void StartTailLash();
	void TailLashHit();

	void StartSpit();
	void SpitFire();

	void StartBreath();
	void BeginBreathing();
	void BreathTick();
	void EndBreath();

	void StartDive();
	void DiveSubmerge();
	void DiveShowWarning();
	void DiveEmerge();
	void BurnPatchTick();

	APawn* GetPlayer() const;
	void FacePlayer();
	bool IsPlayerInCone(float Range, float HalfAngleDegrees) const;
	void DamagePlayer(float BaseAmount);
	float GetSpeedScale() const { return bPhaseTwo ? PhaseTwoSpeedMultiplier : 1.f; }
	float PlayMontageScaled(UAnimMontage* Montage);
	FVector GetFeetLocation() const;
	void Schedule(FTimerHandle& Handle, void (AVulcanBoss::*Callback)(), float DelaySeconds);

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	EVulcanAttack CurrentAttack = EVulcanAttack::None;
	bool bFightActive = false;
	bool bPhaseTwo = false;
	bool bDead = false;
	float NextAttackTime = 0.f;
	float BreathTimeRemaining = 0.f;
	FVector DiveTarget = FVector::ZeroVector;
	FVector BurnPatchLocation = FVector::ZeroVector;
	float BurnPatchTimeRemaining = 0.f;

	FTimerHandle ThinkTimer;
	FTimerHandle AttackTimer;
	FTimerHandle BreathTimer;
	FTimerHandle BurnTimer;
};
