#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HipRootMotionComponent.generated.h"

/**
 * Mixamo attack animations move the hips forward instead of the root, so during a montage the body
 * slides away from the capsule and snaps back when it ends. This turns that hip travel into real
 * movement: while a montage plays, the character moves by however far the hips moved (blocked by
 * walls), and the mesh is shifted back so the body stays over the capsule. The normal walk/run
 * cycles are left alone.
 */
UCLASS(ClassGroup=(Animation), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UHipRootMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHipRootMotionComponent();

	/** Hip bone of the character's animated Mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hip Root Motion")
	FName PelvisBone = TEXT("pelvis");

	/** 1 = move as far as the hips do, 0 = stay in place (the body still stays over the capsule). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hip Root Motion", meta=(ClampMin="0", ClampMax="2"))
	float MoveScale = 1.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The mesh was moved on purpose (e.g. lowered onto the floor): keep it there from now on. */
	void RebaseMesh();

protected:
	virtual void BeginPlay() override;

private:
	FVector MeshBaseLocation = FVector::ZeroVector;
	FVector RestPelvis = FVector::ZeroVector;
	FVector LastTravel = FVector::ZeroVector;
	bool bReady = false;
};
