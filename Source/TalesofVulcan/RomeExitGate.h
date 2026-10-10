#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RomeExitGate.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * The way out of the colosseum in the Rome map (placed in L_Rome at the north gate): an iron portcullis in a
 * stone frame against the arena wall, darkness behind it. It stays shut until Vulcan dies, then grinds open;
 * walking through fades to black and brings the player out at ExitLocation outside the colosseum.
 * Faces its +X (into the gate); the frame, bars and trigger are built when play starts.
 */
UCLASS()
class TALESOFVULCAN_API ARomeExitGate : public AActor
{
	GENERATED_BODY()

public:
	ARomeExitGate();

	/** Where the player comes out, relative to this actor, and the way they face (yaw, relative). */
	UPROPERTY(EditAnywhere, Category="Exit", meta=(MakeEditWidget))
	FVector ExitLocation = FVector(2100.f, 0.f, 150.f);

	UPROPERTY(EditAnywhere, Category="Exit")
	float ExitYaw = 0.f;

	/** Opening size (cm) and how long the bars take to rise (s). */
	UPROPERTY(EditAnywhere, Category="Exit")
	float Width = 360.f;

	UPROPERTY(EditAnywhere, Category="Exit")
	float Height = 420.f;

	UPROPERTY(EditAnywhere, Category="Exit")
	float RaiseSeconds = 4.f;

	/** Opens the gate (Vulcan's death calls this). */
	UFUNCTION(BlueprintCallable, Category="Exit")
	void Open();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void Build();

	UFUNCTION()
	void HandleBossDeath(AActor* Killer);

	UFUNCTION()
	void HandlePassage(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UPROPERTY(VisibleAnywhere, Category="Exit")
	TObjectPtr<USceneComponent> Root;

	/** The bars, raised as one. */
	UPROPERTY(VisibleAnywhere, Category="Exit")
	TObjectPtr<USceneComponent> Portcullis;

	/** Blocks the opening while shut. */
	UPROPERTY(VisibleAnywhere, Category="Exit")
	TObjectPtr<UBoxComponent> Blocker;

	/** Behind the bars: stepping in here takes the player outside. */
	UPROPERTY(VisibleAnywhere, Category="Exit")
	TObjectPtr<UBoxComponent> Passage;

	bool bOpening = false;
	bool bLeaving = false;
	float Raised = 0.f;
};
