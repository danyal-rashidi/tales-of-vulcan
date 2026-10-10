#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RomeMapTools.generated.h"

class UMaterialInterface;

/**
 * Editor-only helpers for building the Rome map (/Game/Rome/L_Rome) from editor Python
 * (Saved/ClaudeScripts/build_rome_map.py). They do nothing in a packaged game.
 */
UCLASS()
class TALESOFVULCAN_API URomeMapTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates a landscape in World from a raw 16-bit heightmap: little-endian, Size x Size samples, where 32768 is
	 * the landscape's own height. Size must be (components * SectionsPerComponent * QuadsPerSection) + 1.
	 * The landscape is centred on Center; Scale is cm per quad in X/Y, and in Z 100 gives +-256 m of height range.
	 * In a World Partition map it is then split into streaming proxies of GridSize components, like the editor's
	 * New Landscape tool does. Returns a short report, or why it failed.
	 */
	UFUNCTION(BlueprintCallable, Category="Rome|Editor")
	static FString BuildLandscapeFromRaw(UWorld* World, const FString& RawFile, int32 Size, int32 QuadsPerSection, int32 SectionsPerComponent,
		FVector Center, FVector Scale, UMaterialInterface* Material, int32 GridSize);
};
