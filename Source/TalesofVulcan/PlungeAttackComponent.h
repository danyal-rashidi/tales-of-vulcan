#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlungeAttackComponent.generated.h"

class UAnimMontage;
class UCameraShakeBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlungeStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlungeLanded, FVector, ImpactLocation);

/**
 * Plunging air attack for the player. Call TryPlunge from the attack input event.
 * - Only works while falling and at least MinHeightAboveGround above the floor.
 * - Spends stamina, hangs in the air for HangTime, then drives the owner straight down.
 * - On landing (or slamming into a pawn), damages every pawn within Radius of the
 *   impact with Apply Damage, plays the camera shake and fires OnPlungeLanded
 *   (spawn the dust there), then roots the owner for LandingRecovery seconds.
 * - Light/heavy attacks and dodges are refused until the recovery ends.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UPlungeAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlungeAttackComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float Damage = 50.f;

	/** Radius of the landing hit around the impact point, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float Radius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float StaminaCost = 30.f;

	/** Downward speed of the slam in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float PlungeSpeed = 2500.f;

	/**
	 * Must be at least this high above the floor to plunge, so tiny hops don't trigger it.
	 * Keep it well under the jump's peak (Jump Z Velocity 500 at gravity 1.0 peaks at ~128 cm).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float MinHeightAboveGround = 60.f;

	/** Seconds frozen at the top before slamming down. Sells the weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float HangTime = 0.12f;

	/** Seconds rooted in place after landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float LandingRecovery = 0.7f;

	/** Gives up (no damage) if the owner still hasn't landed after this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float MaxPlungeTime = 2.f;

	/**
	 * Optional slam animation. If it has a FallSection, that section plays (set it to loop)
	 * until impact, then the montage jumps to SlamSection. Without sections it just plays.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	TObjectPtr<UAnimMontage> PlungeMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	FName FallSection = TEXT("Fall");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	FName SlamSection = TEXT("Slam");

	/** Camera shake played on the player's camera at impact. Defaults to SlamCameraShake; clear it for no shake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	TSubclassOf<UCameraShakeBase> LandingCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	float CameraShakeScale = 1.f;

	/** Draws the impact sphere (green = hit something, red = missed). Turn off for the demo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plunge")
	bool bShowDebug = true;

	/** Hook up a whoosh sound here. */
	UPROPERTY(BlueprintAssignable, Category="Plunge")
	FOnPlungeStarted OnPlungeStarted;

	/** Spawn the dust burst and impact sound here. */
	UPROPERTY(BlueprintAssignable, Category="Plunge")
	FOnPlungeLanded OnPlungeLanded;

	/** Returns true if the plunge started. */
	UFUNCTION(BlueprintCallable, Category="Plunge")
	bool TryPlunge();

	/** True from the moment the plunge starts until the landing recovery ends. */
	UFUNCTION(BlueprintPure, Category="Plunge")
	bool IsPlunging() const { return State != EPlungeState::None; }

protected:
	virtual void BeginPlay() override;

private:
	enum class EPlungeState : uint8
	{
		None,
		Hanging,
		Diving,
		Recovering
	};

	void StartDive();
	void Impact(const FVector& ImpactLocation);
	void CancelPlunge();
	void EndRecovery();

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	UFUNCTION()
	void HandleActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	EPlungeState State = EPlungeState::None;
	float SavedGravityScale = 1.f;
	float SavedAirControl = 0.f;
	float SavedMaxWalkSpeed = 0.f;
	FTimerHandle PlungeTimer;
};
