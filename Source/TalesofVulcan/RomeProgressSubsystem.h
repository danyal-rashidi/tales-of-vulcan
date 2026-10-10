#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RomeProgressSubsystem.generated.h"

/**
 * What the player has done in this run of the game, kept across level reloads (a death reloads the level).
 * Cleared when the main menu opens.
 */
UCLASS()
class TALESOFVULCAN_API URomeProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The player has gone into the colosseum: after a death they come back at its door, not in the town. */
	bool bEnteredArena = false;

	void Reset() { bEnteredArena = false; }
};
