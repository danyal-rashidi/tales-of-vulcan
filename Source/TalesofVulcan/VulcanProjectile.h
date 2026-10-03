#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VulcanProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** Obsidian shard spat by Vulcan. Damages whatever it hits, then disappears. */
UCLASS()
class TALESOFVULCAN_API AVulcanProjectile : public AActor
{
	GENERATED_BODY()

public:
	AVulcanProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<USphereComponent> Collision;

	/** Defaults to a small engine sphere. Swap in a shard mesh in the Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile")
	float Damage = 15.f;

	/** Spawn an impact effect/sound here in Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category="Projectile")
	void OnImpact(FVector Location);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);
};
