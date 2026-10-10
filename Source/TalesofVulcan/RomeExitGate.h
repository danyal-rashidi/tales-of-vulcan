#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RomeExitGate.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * A gate in the colosseum (placed in L_Rome): an iron portcullis in a stone frame against a wall, darkness
 * behind it; walking through fades to black and brings the player out at ExitLocation.
 *  - The way out (at the arena's north gate): stays shut until Vulcan dies, then grinds open.
 *  - The way in (bEntrance, on the outer wall): open from the start. Once the player has gone in, a death brings
 *    them back in front of it rather than at the start of the map.
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

	/** The way into the colosseum: open from the start instead of when Vulcan dies. */
	UPROPERTY(EditAnywhere, Category="Exit")
	bool bEntrance = false;

	/** Opens the gate (Vulcan's death calls this). */
	UFUNCTION(BlueprintCallable, Category="Exit")
	void Open();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	void SetRaised(float Amount);
	void ReturnPlayerToDoor();

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
