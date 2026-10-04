#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GroundCheckSubsystem.generated.h"

/**
 * Debug tool. Start the game with -GroundCheck and, a few seconds in, it logs what every character
 * is standing on (floor actor, gap under the capsule, lowest visible point, every collision surface
 * straight below), then quits. Does nothing in normal play.
 */
UCLASS()
class TALESOFVULCAN_API UGroundCheckSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void Report();

	FTimerHandle ReportTimer;
};
