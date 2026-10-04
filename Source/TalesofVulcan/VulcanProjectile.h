#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VulcanProjectile.generated.h"

class UPointLightComponent;
class URotatingMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * Lava ball spat by Vulcan: a glowing molten core with patches of dark cooled crust,
 * spinning as it flies and lighting up the arena. Damages whatever it hits, then disappears.
 */
UCLASS()
class TALESOFVULCAN_API AVulcanProjectile : public AActor
{
	GENERATED_BODY()

public:
	AVulcanProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<USphereComponent> Collision;

	/** Molten core. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Dark cooled-rock patches on the surface; lava shows through the gaps. */
	UPROPERTY(VisibleAnywhere, Category="Projectile")
	TArray<TObjectPtr<UStaticMeshComponent>> CrustPatches;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<URotatingMovementComponent> Spin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile")
	float Damage = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(HideAlphaChannel))
	FLinearColor LavaColor = FLinearColor(FColor(255, 80, 10));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(HideAlphaChannel))
	FLinearColor CrustColor = FLinearColor(FColor(28, 16, 14));

	/** Spawn an impact effect/sound here in Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category="Projectile")
	void OnImpact(FVector Location);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);
};
