#include "RomeMapTools.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"

#if WITH_EDITOR
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeSubsystem.h"
#endif

FString URomeMapTools::BuildLandscapeFromRaw(UWorld* World, const FString& RawFile, int32 Size, int32 QuadsPerSection, int32 SectionsPerComponent,
	FVector Center, FVector Scale, UMaterialInterface* Material, int32 GridSize)
{
#if WITH_EDITOR
	if (!World)
	{
		return TEXT("no world");
	}
	const int32 QuadsPerComponent = QuadsPerSection * SectionsPerComponent;
	if (QuadsPerComponent <= 0 || (Size - 1) % QuadsPerComponent != 0)
	{
		return FString::Printf(TEXT("size %d is not a whole number of %d-quad components plus one"), Size, QuadsPerComponent);
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *RawFile) || Bytes.Num() != Size * Size * 2)
	{
		return FString::Printf(TEXT("couldn't read %d x %d 16-bit heights from %s (%d bytes)"), Size, Size, *RawFile, Bytes.Num());
	}
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(Size * Size);
	FMemory::Memcpy(Heights.GetData(), Bytes.GetData(), Bytes.Num());

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	// Placed so its middle sits on Center.
	const FVector Corner = Center - FVector((Size - 1) * 0.5 * Scale.X, (Size - 1) * 0.5 * Scale.Y, 0.0);
	ALandscape* Landscape = World->SpawnActor<ALandscape>(Corner, FRotator::ZeroRotator);
	if (!Landscape)
	{
		return TEXT("couldn't spawn the landscape");
	}
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((Size * Size) / (2048 * 2048) + 1), (uint32)2);
	Landscape->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, SectionsPerComponent, QuadsPerSection, HeightDataPerLayers, nullptr,
		MaterialLayerDataPerLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	Landscape->SetActorLabel(TEXT("RomeLandscape"));

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return TEXT("the landscape has no info after import");
	}
	Info->UpdateLayerInfoMap(Landscape);

	ULandscapeSubsystem* Landscapes = World->GetSubsystem<ULandscapeSubsystem>();
	const bool bGrid = Landscapes && Landscapes->IsGridBased();
	if (bGrid)
	{
		Landscapes->ChangeGridSize(Info, GridSize);
	}
	Landscape->MarkPackageDirty();

	const int32 Components = (Size - 1) / QuadsPerComponent;
	return FString::Printf(TEXT("landscape %d x %d components (%d quads each), %.0f x %.0f m, %s"), Components, Components, QuadsPerComponent,
		(Size - 1) * Scale.X / 100.0, (Size - 1) * Scale.Y / 100.0, bGrid ? *FString::Printf(TEXT("split into a %d-component streaming grid"), GridSize) : TEXT("not grid based"));
#else
	return TEXT("editor only");
#endif
}
