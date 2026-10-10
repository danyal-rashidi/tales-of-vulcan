#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RomeBrazier.generated.h"

/**
 * A brazier you can place in any level: a broken-column stand, an iron bowl of glowing coals and real flames
 * (AFlameFX) with a flickering light. Built when play starts; the box shows where it goes in the editor.
 */
UCLASS()
class TALESOFVULCAN_API ARomeBrazier : public AActor
{
	GENERATED_BODY()

public:
	ARomeBrazier();

	/** Height of the flames (cm) and their light (candelas, 0 = none). */
	UPROPERTY(EditAnywhere, Category="Brazier")
	float FlameHeight = 120.f;

	UPROPERTY(EditAnywhere, Category="Brazier")
	float LightCandelas = 60.f;

	/** Off: just the bowl and flames on the ground (e.g. a fire pit). */
	UPROPERTY(EditAnywhere, Category="Brazier")
	bool bStand = true;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Brazier")
	TObjectPtr<class UBoxComponent> Footprint;
};
