#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlayerSetupSubsystem.generated.h"

class ACharacter;

/**
 * Fits the player character out when play starts, without touching its (shared) Blueprint: the hidden mannequin
 * gets UPlayerAnimInstance (the new locomotion), and the character gets UPlayerEquipComponent (draw / sheathe,
 * sprint), and the player's screen becomes UGameHUDWidget. In the open Rome map the spear starts on the back; in the arena
 * and the menu it starts in hand.
 */
UCLASS()
class TALESOFVULCAN_API UPlayerSetupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Console: tov.PlayerSetup 0 keeps the old animation Blueprint and no equip component. */
	static bool IsEnabled();

private:
	void SetUp(ACharacter* Character);

	FTimerHandle Timer;
	bool bHUD = false;
	float Elapsed = 0.f;
	bool bOpenMap = false;
};
