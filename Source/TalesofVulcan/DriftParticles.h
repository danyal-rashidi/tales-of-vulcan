#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DriftParticles.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Lightweight weather: thousands of small stretched shapes (one instanced mesh) drifting
 * with the wind inside a box that wraps around. Used for the blood rain (follows the camera,
 * starts with Vulcan's storm) and for sand blowing across the desert outside the colosseum.
 */
UCLASS()
class TALESOFVULCAN_API ADriftParticles : public AActor
{
	GENERATED_BODY()

public:
	ADriftParticles();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drift")
	TObjectPtr<UInstancedStaticMeshComponent> Particles;

	/** Maximum number of particles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift", meta=(ClampMin="1", ClampMax="20000"))
	int32 Count = 3000;

	/** Size of the box the particles live in (cm). It wraps around, so particles never run out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift")
	FVector Area = FVector(5000.f, 5000.f, 2500.f);

	/** On: the box stays centred on the player's camera (rain). Off: the box sits at this actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift")
	bool bFollowCamera = true;

	/** Wind + fall speed in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift")
	FVector Velocity = FVector(250.f, 120.f, -2200.f);

	/** Random gusting on top of Velocity (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift", meta=(ClampMin="0"))
	float Turbulence = 120.f;

	/** Particle size in cm; Z is the length along the direction of travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift")
	FVector ParticleSize = FVector(1.5f, 1.5f, 70.f);

	/** Random +/- variation of the particle size (0.3 = up to 30%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift", meta=(ClampMin="0", ClampMax="1"))
	float SizeVariation = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift|Look", meta=(HideAlphaChannel))
	FLinearColor Color = FLinearColor(0.35f, 0.01f, 0.01f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift|Look", meta=(ClampMin="0"))
	float Glow = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift|Look", meta=(ClampMin="0", ClampMax="1"))
	float Roughness = 0.2f;

	/** Optional material (e.g. M_SandWisp for soft translucent streaks). Empty = solid shape material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift|Look")
	TObjectPtr<UMaterialInterface> CustomMaterial;

	/** Used by translucent custom materials. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift|Look", meta=(ClampMin="0", ClampMax="1"))
	float Opacity = 0.2f;

	/** 0..1: fraction of particles shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift", meta=(ClampMin="0", ClampMax="1"))
	float Intensity = 1.f;

	/** Starts off and is turned on by Vulcan's storm once the sky has turned red. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift")
	bool bWaitForStorm = false;

	/** Hide particles inside this ellipse around the actor (cm radii, 0 = off), e.g. the colosseum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift")
	FVector2D ExcludeEllipse = FVector2D::ZeroVector;

	/** The ellipse only hides particles below this height above the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drift")
	float ExcludeHeight = 2000.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TArray<FVector> Positions;
	TArray<float> Phases;
	TArray<float> Sizes;
	TArray<FTransform> Transforms;
	float Time = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FVector GetBoxCenter() const;
};
