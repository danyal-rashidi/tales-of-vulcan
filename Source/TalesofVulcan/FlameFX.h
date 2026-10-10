#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlameFX.generated.h"

class UMaterialBillboardComponent;
class UPointLightComponent;

/**
 * A big, detailed fire: several camera-facing flame tongues drawn with M_Flame (/Game/FX), an animated shader
 * that scrolls and distorts fire noise into licking flames, each tongue with its own seed so no two move alike.
 * Adds a flickering light (it lights volumetric fog too) and rising embers (AFireFX). Burns until destroyed.
 */
UCLASS(NotBlueprintable)
class TALESOFVULCAN_API AFlameFX : public AActor
{
	GENERATED_BODY()

public:
	AFlameFX();

	/** A fire Height cm tall standing on Base. LightIntensity is in candelas (0 = no light). */
	static AFlameFX* Spawn(UWorld* World, const FVector& Base, float Height, float LightIntensity = 300.f);

	virtual void Tick(float DeltaSeconds) override;

private:
	void Build(float Height, float LightIntensity);

	UPROPERTY()
	TArray<TObjectPtr<UMaterialBillboardComponent>> Tongues;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	float BaseIntensity = 0.f;
	FVector LightRest = FVector::ZeroVector;
	float Time = 0.f;
	float Seed = 0.f;
};
