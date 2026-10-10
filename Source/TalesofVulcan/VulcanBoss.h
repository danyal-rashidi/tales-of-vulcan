#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VulcanBoss.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UHealthComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
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

	// ---------------------------------------------------------------- Otter body

	/**
	 * Builds a volcanic cartoon otter out of simple shapes that follow the mannequin's
	 * bones (so every mannequin/Mixamo animation still works) and hides the mannequin.
	 * Turn off when a real otter model is assigned to Mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body")
	bool bUseOtterBody = true;

	/** Cooled volcanic rock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(HideAlphaChannel))
	FLinearColor FurColor = FLinearColor(FColor(120, 34, 22));

	/** Belly — molten lava. Cools to black when Vulcan dies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(HideAlphaChannel))
	FLinearColor LavaColor = FLinearColor(FColor(255, 80, 10));

	/** Spikes, nose, glasses frames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(HideAlphaChannel))
	FLinearColor ObsidianColor = FLinearColor(FColor(14, 10, 12));

	/** Muzzle/cheeks, inner ears, eyebrow dots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(HideAlphaChannel))
	FLinearColor MuzzleColor = FLinearColor(FColor(255, 196, 150));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(HideAlphaChannel))
	FLinearColor EyeColor = FLinearColor(FColor(12, 10, 10));

	/** White highlight in the eyes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(HideAlphaChannel))
	FLinearColor LensColor = FLinearColor(FColor(240, 240, 240));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body")
	bool bShowGlasses = true;

	/**
	 * Size of the original otter's head and everything on it (face, glasses, ears, head spikes). 1 = original.
	 * The awakened otter (see bAwakenedForm) has its own fixed proportions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(ClampMin="0.25", ClampMax="4"))
	float HeadScale = 2.f;

	/** 0 = matte, 1 = full metal. Applies to fur, muzzle and ears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(ClampMin="0", ClampMax="1"))
	float BodyMetallic = 0.8f;

	/** 0 = mirror-shiny, 1 = dull. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(ClampMin="0", ClampMax="1"))
	float BodyRoughness = 0.25f;

	/** How brightly the lava belly glows (needs the M_VulcanShape material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body", meta=(ClampMin="0"))
	float LavaGlow = 4.f;

	/** Flickering lava light from the chest. Doubles in phase 2, goes out on death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body")
	float CoreGlowIntensity = 15.f;

	/** Not saved: levels saved with an older part list would otherwise stop the body from updating. */
	UPROPERTY(VisibleAnywhere, Transient, Category="Vulcan|Otter Body")
	TArray<TObjectPtr<UStaticMeshComponent>> OtterParts;

	UPROPERTY(VisibleAnywhere, Category="Vulcan|Otter Body")
	TObjectPtr<UPointLightComponent> CoreGlow;

	/** Molten Breath and Obsidian Spit come out here: the rigged otter model's "mouth" bone. */
	UPROPERTY(VisibleAnywhere, Category="Vulcan|Otter Body")
	TObjectPtr<USceneComponent> MouthPoint;

	/**
	 * Once provoked (the statue awakens, the fight starts, or Vulcan gets hit) the original otter turns into the
	 * awakened chibi otter: huge round head, a curved cream face pattern with chubby cheeks, a cream belly with a
	 * small glowing lava core, stubby limbs and a longer tail. Obsidian spikes, no glasses. Same colors as above.
	 * Off = the original otter for the whole fight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Otter Body")
	bool bAwakenedForm = true;

	/** Show the awakened form in the editor viewport instead of the start-up look. Doesn't affect play. */
	UPROPERTY(EditAnywhere, Category="Vulcan|Otter Body")
	bool bPreviewAwakenedForm = false;

	UFUNCTION(BlueprintPure, Category="Vulcan|Otter Body")
	bool IsInAwakenedForm() const { return bAwakenedShown; }

	// ---------------------------------------------------------------- Statue intro

	/**
	 * Vulcan starts as a bronze statue. When the player gets close it shakes, then
	 * transforms into the lava otter and the fight begins. Invulnerable until then.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro")
	bool bStatueIntro = true;

	/**
	 * Put a sculpture model here (Static Mesh) and line it up in the viewport.
	 * Leave it empty and the otter itself turns to bronze instead.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vulcan|Statue Intro")
	TObjectPtr<UStaticMeshComponent> StatueMesh;

	/** The statue awakens when the player comes this close. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro")
	float IntroTriggerRange = 1200.f;

	/** Seconds of shaking before it comes alive. The storm rolls in over this time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro")
	float StatueAwakenTime = 4.f;

	/** A lightning bolt hits the statue when the player comes close, then it starts shaking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro")
	bool bLightningStrike = true;

	/** Seconds the bolt flickers before the statue starts shaking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro")
	float LightningStrikeTime = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro", meta=(HideAlphaChannel))
	FLinearColor LightningColor = FLinearColor(0.75f, 0.85f, 1.f);

	/** How far the statue lurches while awakening (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro")
	float AwakenShake = 10.f;

	/** How far the statue rocks while awakening (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro")
	float AwakenWobble = 4.f;

	/** The player's camera rumbles like an earthquake from the lightning strike until it comes alive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro")
	bool bEarthquake = true;

	/** Earthquake strength at its peak, just before it comes alive (1 = default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro", meta=(ClampMin="0"))
	float EarthquakeStrength = 1.f;

	/** Used when there's no sculpture model: the otter body is tinted this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Statue Intro", meta=(HideAlphaChannel))
	FLinearColor BronzeColor = FLinearColor(FColor(150, 95, 45));

	UFUNCTION(BlueprintPure, Category="Vulcan|Statue Intro")
	bool IsStatue() const { return IntroState != EIntroState::Done; }

	/** Start the transformation now (e.g. from a cutscene trigger). */
	UFUNCTION(BlueprintCallable, Category="Vulcan|Statue Intro")
	void AwakenFromStatue();

	/** Lightning hit the statue: thunder sound, sparks, camera shake. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnStatueStruck(FVector StrikeLocation);

	/** Statue starts shaking: crack sounds, dust, camera shake. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnStatueAwakening();

	// ---------------------------------------------------------------- Storm sky

	/**
	 * While the statue shakes, the level's sky fills with dark storm clouds and the sun
	 * turns blood red, so the fight happens under that sky. Uses the level's Directional
	 * Light, Sky Atmosphere, Volumetric Cloud and Exponential Height Fog (any can be missing).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm")
	bool bStormSky = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(HideAlphaChannel))
	FLinearColor BloodSunColor = FLinearColor(1.f, 0.02f, 0.008f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(ClampMin="0"))
	float BloodSunIntensity = 5.f;

	/** Sun angle during the fight (-90 = straight down). High enough to be seen above the stands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm")
	float StormSunPitch = -30.f;

	/** Move the sun so it hangs in the sky behind Vulcan, as seen from where the player is standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm")
	bool bSunBehindVulcan = true;

	/** Size of the glowing red sun in the sky (degrees across; the real sun is about 0.5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(ClampMin="0"))
	float BloodSunSize = 8.f;

	/** How brightly the red sun glows. Above ~3 it washes out to orange/white on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(ClampMin="0"))
	float BloodSunGlow = 1.2f;

	/** Degrees to the side of Vulcan, so his head doesn't hide the sun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm")
	float SunSideOffset = 25.f;

	/** Multiplies the sky's brightness and color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(HideAlphaChannel))
	FLinearColor StormSkyTint = FLinearColor(0.6f, 0.12f, 0.09f);

	/** Cloud cover during the fight. The level's sky is -0.2; values much above or below that clear the sky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm")
	float StormCloudCoverage = -0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm")
	float StormCloudDensity = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(HideAlphaChannel))
	FLinearColor StormCloudColor = FLinearColor(0.05f, 0.035f, 0.035f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(HideAlphaChannel))
	FLinearColor StormFogColor = FLinearColor(0.08f, 0.015f, 0.01f);

	/** Fog thickness during the fight. The level's fog is thick enough to hide the clouds, so it thins out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(ClampMin="0"))
	float StormFogDensity = 0.004f;

	/** Once Vulcan is dead the storm passes: after this many seconds the sky starts clearing... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(ClampMin="0"))
	float SkyClearDelay = 3.f;

	/** ...and takes this long to return to how it was before he woke. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Storm", meta=(ClampMin="0.1"))
	float SkyClearSeconds = 12.f;

	/** Statue became the otter: burst of fire/smoke, roar. The fight starts right after. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnStatueTransformed();

	/** Provoked: Vulcan just switched to the awakened otter form. Good spot for a puff of smoke. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vulcan|Events")
	void OnAwakenedFormShown();

	// ---------------------------------------------------------------- General

	/** Off = Vulcan waits until StartFight is called (e.g. from an arena trigger box). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	bool bStartFightOnBeginPlay = true;

	/** Draws hitboxes, cones and warning circles while testing. Turn on while tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	bool bShowDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	float AggroRange = 4000.f;

	/** Chase speed (cm/s). A bit quicker than the player's 600 run, so he can't simply be outrun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|General")
	float WalkSpeed = 700.f;

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

	// ---------------------------------------------------------------- Animations
	// Mixamo animations retargeted to the mannequin (RTG_* assets). At BeginPlay each one
	// is turned into a montage for any montage slot that is still empty, so no montage assets are needed.

	/** Fight-start roar and Molten Breath. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimSequenceBase> RoarAnimation;

	/** Magma Dive take-off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimSequenceBase> JumpAnimation;

	/** Magma Dive landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimSequenceBase> LandAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimSequenceBase> HitReactAnimation;

	/** Holds its last frame (lies on the ground). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimSequenceBase> DeathAnimation;

	/** Tail Lash: wind up, spin with the tail out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimSequenceBase> TailLashAnimation;

	/** Obsidian Spit: rear back, snap the head forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimSequenceBase> SpitAnimation;

	/** Molten Breath: inhale, then hold the fire pose until the breath ends. Empty = RoarAnimation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimSequenceBase> BreathAnimation;

	/** Played when the statue comes alive; Vulcan stands still for it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimMontage> RoarMontage;

	/** Flinch when the player lands a hit between attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** Seconds between flinches, so a combo can't stun-lock Vulcan. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vulcan|Animations")
	float HitReactCooldown = 1.5f;

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
	float BreathRange = 420.f;

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
	float PhaseTwoBreathExtraRange = 140.f;

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

	/** Start this attack now, facing the player (tests, cutscenes). Ignored during another attack, the statue intro or death. */
	UFUNCTION(BlueprintCallable, Category="Vulcan")
	void PerformAttack(EVulcanAttack Attack);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	enum class EIntroState : uint8 { Statue, Struck, Awakening, Done };
	EIntroState IntroState = EIntroState::Done;
	FTimerHandle IntroTimer;
	FVector MeshRestLocation = FVector::ZeroVector;
	FVector StatueRestLocation = FVector::ZeroVector;
	FRotator MeshRestRotation = FRotator::ZeroRotator;
	FRotator StatueRestRotation = FRotator::ZeroRotator;

	TWeakObjectPtr<class UCameraShakeBase> Quake;
	void SetQuakeStrength(float Scale);
	void StopQuake();

	void FreezeStatuePose();
	void StrikeStatue();
	void BeginShaking();
	void FinishAwakening();
	bool HasStatueModel() const;

	// Lightning bolt: glowing cylinders from the sky to the statue + a flash light
	void BuildBolt(const FVector& Target);
	void FlickerBolt();
	void ClearBolt();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BoltParts;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> BoltFlash;

	FTimerHandle BoltTimer;
	float BoltEndTime = 0.f;

	// Storm sky: level lighting captured when the statue is struck, then blended to the storm look
	void CaptureSky();
	void ApplyStorm(float Alpha);

	TWeakObjectPtr<class UDirectionalLightComponent> Sun;
	TWeakObjectPtr<class USkyAtmosphereComponent> Atmosphere;
	TWeakObjectPtr<class UVolumetricCloudComponent> Clouds;
	TWeakObjectPtr<class UExponentialHeightFogComponent> Fog;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloudMaterial;

	bool bSkyCaptured = false;
	FLinearColor SunStartColor = FLinearColor::White;
	float SunStartIntensity = 0.f;
	FRotator SunStartRotation = FRotator::ZeroRotator;
	FRotator SunTargetRotation = FRotator::ZeroRotator;
	float StormAlpha = 0.f;

	/** Seconds since Vulcan died, for clearing the sky. */
	float DeadTime = 0.f;

	/** Big glowing sun disk kept far away along the sun direction (the real one hides behind clouds). */
	void UpdateBloodSun();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BloodSunDisk;

	/** Owns BloodSunDisk, so the sun stays in the sky while Vulcan himself is hidden (Magma Dive). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> BloodSunHolder;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BloodSunMaterial;
	FLinearColor SkyStartTint = FLinearColor::White;
	FLinearColor FogStartColor = FLinearColor::White;
	float FogStartDensity = 0.f;
	float CloudStartCoverage = 0.f;
	float CloudStartDensity = 0.f;
	FLinearColor CloudStartAlbedo = FLinearColor::White;
	FLinearColor CloudStartStormAlbedo = FLinearColor::White;

	void ApplyOtterLook();
	void UpdateOtterBody();
	/** Re-finds the Otter_* components by name if OtterParts doesn't match the current part list. */
	void RebindOtterParts();
	void LayoutAwakenedOtter(float S, const FVector& Fwd, const FVector& Right);
	void PlaceCoreGlow(const FVector& Pelvis, const FVector& Spine, const FVector& Fwd, float S);
	void ShowAwakenedForm();

	/** True once provoked (see bAwakenedForm). */
	bool bAwakenedShown = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> OtterMaterials;

	float OtterTime = 0.f;

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

	/** Fills empty montage slots from the *Animation sequences. */
	void BuildMontagesFromAnimations();

	EVulcanAttack CurrentAttack = EVulcanAttack::None;
	float LastHealth = -1.f;
	float NextHitReactTime = 0.f;
	FTimerHandle RoarTimer;
	/** True when there's no nav mesh path, so Tick walks straight at the player instead. */
	bool bDirectChase = false;
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
