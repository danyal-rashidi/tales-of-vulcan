#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpearGripComponent.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Keeps the spear in both hands. Every frame, after animation, the spear is placed so its shaft
 * runs through both palms with the spearhead past the front hand, so every swing of the
 * (two-handed sword) animations actually swings the spear.
 * When the hands are too close or too far apart to define a line (one-handed moments),
 * the spear stays glued to the front hand the way it was held last.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API USpearGripComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USpearGripComponent();

	/** The spear: a static mesh component on the owner with this name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	FName WeaponComponentName = TEXT("Spear");

	/** The visible skin whose hands hold the spear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	FName HandsMeshName = TEXT("NightHunterMesh");

	/** Hand nearer the spearhead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	FName FrontHandBone = TEXT("RightHand");

	/** Hand nearer the back end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	FName BackHandBone = TEXT("LeftHand");

	/** Finger bones used to find the palm (the grip point sits between wrist and knuckles). Missing bones are fine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	FName FrontKnuckleBone = TEXT("RightHandIndex1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	FName BackKnuckleBone = TEXT("LeftHandIndex1");

	/** Where the front hand holds the spear: fraction of its length from the back end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip", meta=(ClampMin="0", ClampMax="1"))
	float FrontHandPosition = 0.4f;

	/** Tick if the spearhead ends up pointing backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	bool bFlipSpear = false;

	/** Spins the spear around its shaft, in degrees (turns the blade flat or upright). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	float Roll = 0.f;

	/** Hands closer than this (cm) don't define a direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	float MinHandSpacing = 4.f;

	/** Hands further apart than this (cm) mean one hand let go. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spear Grip")
	float MaxHandSpacing = 50.f;

	/** World-space ends of the spear shaft (back end and tip). Returns false if there is no spear. */
	UFUNCTION(BlueprintPure, Category="Spear Grip")
	bool GetSpearSegment(FVector& OutBack, FVector& OutTip) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	FVector GripPoint(FName HandBone, FName KnuckleBone) const;

	TWeakObjectPtr<UStaticMeshComponent> Weapon;
	TWeakObjectPtr<USkeletalMeshComponent> Hands;

	/** Spear mesh, local space: unit axis from back end to tip, centre and length. */
	FVector LocalAxis = FVector::ForwardVector;
	FVector LocalUp = FVector::UpVector;
	FVector LocalCenter = FVector::ZeroVector;
	float Length = 0.f;

	/** Spear transform relative to the front hand, from the last frame both hands held it. */
	FTransform HeldInFrontHand;
	bool bHasHeld = false;
};
