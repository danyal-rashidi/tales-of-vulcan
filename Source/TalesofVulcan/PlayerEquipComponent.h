#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerEquipComponent.generated.h"

class UAnimSequence;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;

/**
 * The spear in hand or on the back, and sprinting.
 *  - R / gamepad Y draws or sheathes the spear: the reach-over-the-shoulder clip plays on the upper body, and the
 *    spear moves between the hands and the back at the moment the hand reaches it. Sheathed, it rides diagonally
 *    across the back, point up over the right shoulder. Attacking while sheathed draws it instead.
 *  - Shift / gamepad B: tap to roll (the existing dodge), hold while moving to sprint. Sprinting drains stamina.
 * The keys live in a mapping context made at play time, above the game's own, so no input asset changes.
 * Added to the player at play time by UPlayerSetupSubsystem.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class TALESOFVULCAN_API UPlayerEquipComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerEquipComponent();

	UFUNCTION(BlueprintPure, Category="Equip")
	bool IsArmed() const { return bArmed; }

	/** Drawing or sheathing right now. */
	UFUNCTION(BlueprintPure, Category="Equip")
	bool IsBusy() const { return Action != EAction::None; }

	UFUNCTION(BlueprintPure, Category="Equip")
	bool IsSprinting() const { return bSprinting; }

	UFUNCTION(BlueprintCallable, Category="Equip")
	void Draw();

	UFUNCTION(BlueprintCallable, Category="Equip")
	void Sheathe();

	UFUNCTION(BlueprintCallable, Category="Equip")
	void Toggle();

	/** The sprint / roll button, for scripted play (test runs): held and released like the real one. */
	void SetSprintHeld(bool bHeld) { bHeld ? SprintPressed() : SprintReleased(); }

	/** Set the state straight away, no animation (at the start of a level). */
	void SetArmedImmediately(bool bInArmed);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equip")
	float SprintSpeed = 860.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equip")
	float SprintStaminaPerSecond = 16.f;

	/** Extra field of view at full sprint (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equip")
	float SprintFOV = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equip")
	float DrawRate = 1.35f;

	/** Fraction of the draw / sheathe clip at which the hand reaches the spear on the back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equip")
	float DrawSwapAt = 0.36f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Equip")
	float SheatheSwapAt = 0.5f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	enum class EAction : uint8 { None, Drawing, Sheathing };

	void TryBindInput();
	void SprintPressed();
	void SprintReleased();
	void PlaceOnBack();
	bool CanChangeWeapon() const;
	class UPlayerAnimInstance* GetAnim() const;

	bool bArmed = true;
	EAction Action = EAction::None;
	float ActionTime = 0.f;
	bool bSwapped = false;

	bool bSprintHeld = false;
	bool bSprintUsed = false;
	bool bSprinting = false;
	bool bExhausted = false;
	float SprintHeldTime = 0.f;
	float SprintAlpha = 0.f;
	float BaseSpeed = 600.f;
	float LastSetSpeed = -1.f;
	float BaseFOV = -1.f;
	bool bAutoDrawn = false;
	float BossCheckTime = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> DrawClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> SheatheClip;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> EquipAction;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> Input;
};
