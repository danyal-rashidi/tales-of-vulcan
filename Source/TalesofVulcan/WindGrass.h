#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindGrass.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Tufts of dry desert grass that bend in the wind. Each blade is a thin cone in an instanced mesh
 * (three straw colours); the wind sways them with travelling gusts, updated on the CPU like
 * DriftParticles. Placed at runtime by ArenaDressingSubsystem.
 */
UCLASS(NotBlueprintable)
class TALESOFVULCAN_API AWindGrass : public AActor
{
	GENERATED_BODY()

public:
	AWindGrass();

	/** Wind direction (flattened) and how far gusts bend the blades, in degrees. */
	FVector WindDirection = FVector(1.f, 0.35f, 0.f);
	float WindBendDegrees = 16.f;

	/** Adds a tuft of blades growing from Ground. Call FinishTufts when done. */
	void AddTuft(const FVector& Ground, FRandomStream& Random);
	void FinishTufts();

	int32 NumBlades() const { return Blades.Num(); }

	virtual void Tick(float DeltaSeconds) override;

private:
	struct FBlade
	{
		FVector Base = FVector::ZeroVector;
		FQuat Rest = FQuat::Identity;
		FVector Scale = FVector::OneVector;
		float Phase = 0.f;
		float Flex = 1.f;
		int32 Band = 0;
		int32 Index = 0;
	};

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Bands[3];

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BandMaterials[3];

	TArray<FBlade> Blades;
	TArray<FTransform> Transforms[3];

	/** Cone mesh, local space: base height, height and width (it is scaled into a blade). */
	float LocalBaseZ = -50.f;
	float LocalHeight = 100.f;
	float LocalWidth = 100.f;

	float Time = 0.f;

	FTransform BladeTransform(const FBlade& Blade, const FQuat& Rotation) const;
};
