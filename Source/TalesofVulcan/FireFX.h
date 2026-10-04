#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FireFX.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
class UPointLightComponent;

/** How a fire effect emits flames. Use the presets on AFireFX. */
struct FFireSettings
{
	/** Flames spawned at once when the effect starts. */
	int32 Burst = 0;
	/** Flames per second while emitting. */
	float Rate = 0.f;
	/** How long it keeps emitting (seconds). 0 = burst only. */
	float Duration = 0.f;
	/** Flames start anywhere on a flat disc of this radius (cm). */
	float SpawnRadius = 0.f;
	/** Main direction the flames fly (world space) and the cone half-angle around it. */
	FVector Direction = FVector::UpVector;
	float SpreadDegrees = 25.f;
	/** Ranges (min, max): start speed cm/s, lifetime s, size cm. */
	FVector2D Speed = FVector2D(150.f, 400.f);
	FVector2D Lifetime = FVector2D(0.4f, 0.9f);
	FVector2D Size = FVector2D(20.f, 45.f);
	/** Upward pull (cm/s^2) and air drag (per second). */
	float Rise = 350.f;
	float Drag = 1.5f;
	/** Extra start speed away from the centre of the spawn disc (cm/s), for bursts. */
	float Outward = 0.f;
	/** Flame size at the end of its life relative to its start (flames widen as they travel). */
	float GrowTo = 1.3f;
	/** How much flames stretch along their motion. */
	float Stretch = 1.8f;
	/** Share of each colour band (white-hot, orange and dark-red embers by default). */
	FVector Mix = FVector(0.25f, 0.5f, 0.25f);
	FLinearColor Colors[3] = { FLinearColor(1.f, 0.6f, 0.15f), FLinearColor(1.f, 0.24f, 0.03f), FLinearColor(0.7f, 0.06f, 0.01f) };
	/** Emissive strength per band (lava uses 4). */
	float Glow[3] = { 4.f, 2.5f, 1.2f };
	/** Above 0: soft see-through puffs (M_SandWisp, e.g. dust) instead of solid flames. */
	float Opacity = 0.f;
	/** Size wobble, as a fraction of the size. */
	float Flicker = 0.18f;
	/** Flickering orange light. 0 = no light. */
	float LightIntensity = 0.f;
	float LightRadius = 900.f;
	/** Crackling fire loop (S_FireLoop). 0 = silent. */
	float SoundVolume = 0.f;
	float SoundRange = 2500.f;
	/** Breath: the effect follows a component and fires along its owner's forward, tilted down by this much. */
	float FollowPitchDegrees = -8.f;
};

/**
 * Cheap fire made of glowing shapes (one instanced mesh per colour), simulated on the CPU like
 * DriftParticles. No Niagara assets needed. Spawn it with AFireFX::Spawn and a preset; it
 * destroys itself once emission is over and the last flame has died.
 */
UCLASS(NotBlueprintable)
class TALESOFVULCAN_API AFireFX : public AActor
{
	GENERATED_BODY()

public:
	AFireFX();

	/** Starts a fire effect at Location. With Follow set, it moves with that component (e.g. a mouth). */
	static AFireFX* Spawn(UWorld* World, const FVector& Location, const FFireSettings& Settings, USceneComponent* Follow = nullptr);

	/** Flamethrower: a cone of flames Range long and HalfAngle wide, for Duration seconds. */
	static FFireSettings BreathPreset(float Range, float HalfAngleDegrees, float Duration);
	/** One burst of flames bursting up and out over a disc. */
	static FFireSettings BurstPreset(float Radius, int32 Count);
	/** Slow embers rising from the ground (warnings). */
	static FFireSettings EmbersPreset(float Radius, float Duration);
	/** Flames licking up from a burning patch of ground. */
	static FFireSettings GroundFirePreset(float Radius, float Duration);
	/** A brazier or torch that burns forever, with a warm flickering light. */
	static FFireSettings TorchPreset(float Radius);
	/** A puff of sand kicked up from the ground, spreading out to about Radius. */
	static FFireSettings DustPreset(float Radius, int32 Count);

	/** Stop emitting; the effect fades out and removes itself. */
	void Stop();

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FFlame
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 0.f;
		float Size = 0.f;
		float Phase = 0.f;
		bool bAlive = false;
	};

	static constexpr int32 BandCount = 3;

	void Start(const FFireSettings& InSettings, USceneComponent* InFollow);
	void SpawnFlame();
	FVector EmitterLocation() const;
	FVector EmitterDirection() const;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Bands[3];

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Audio;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BandMaterials[3];

	FFireSettings Settings;
	TWeakObjectPtr<USceneComponent> Follow;

	/** Each band owns a fixed slice of flame slots; slot i of a band is instance i of its mesh. */
	TArray<FFlame> Flames[BandCount];
	TArray<FTransform> Transforms[BandCount];

	float Time = 0.f;
	float EmitTime = 0.f;
	float SpawnBudget = 0.f;
	float LightLevel = 0.f;
	bool bEmitting = false;
};
