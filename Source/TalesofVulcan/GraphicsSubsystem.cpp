#include "GraphicsSubsystem.h"
#include "SpearGripComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarGraphics(TEXT("tov.Graphics"), 1, TEXT("1: the play-time picture-quality pass (volumetric fog, contact shadows, player key light...). 0: off."));

bool UGraphicsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGraphicsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarGraphics.GetValueOnGameThread() == 0)
	{
		return;
	}
	// After the dressing subsystems (they set the sun and fog a frame into play).
	InWorld.GetTimerManager().SetTimer(Timer, this, &UGraphicsSubsystem::Apply, 0.25f, false);
	InWorld.GetTimerManager().SetTimer(PlayerTimer, this, &UGraphicsSubsystem::LightThePlayer, 0.2f, true);
}

void UGraphicsSubsystem::Apply()
{
	UWorld* World = GetWorld();
	auto Set = [](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByGameSetting);
		}
	};
	Set(TEXT("r.Tonemapper.Sharpen"), 0.6f);
	Set(TEXT("r.Lumen.HardwareRayTracing"), 1.f);   // Lumen falls back to software tracing on cards without ray tracing
	Set(TEXT("r.VolumetricFog.GridPixelSize"), 8.f);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UExponentialHeightFogComponent* Fog = It->FindComponentByClass<UExponentialHeightFogComponent>())
		{
			Fog->SetVolumetricFog(true);
			Fog->SetVolumetricFogScatteringDistribution(0.75f);  // light scatters forward: shafts toward the sun
			Fog->SetVolumetricFogExtinctionScale(0.4f);
			Fog->SetVolumetricFogDistance(9000.f);
		}
		if (UDirectionalLightComponent* Sun = It->FindComponentByClass<UDirectionalLightComponent>())
		{
			Sun->ContactShadowLength = 0.04f;
			Sun->LightSourceAngle = 1.2f;
			Sun->SetVolumetricScatteringIntensity(0.5f);
			Sun->MarkRenderStateDirty();
		}
	}

	// Our own unbound volume, only for what the dressing volumes leave alone.
	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>();
	Volume->bUnbound = true;
	Volume->Priority = -1.f;
	FPostProcessSettings& S = Volume->Settings;
	S.bOverride_BloomIntensity = true;            S.BloomIntensity = 0.55f;
	S.bOverride_BloomThreshold = true;            S.BloomThreshold = -1.f;
	S.bOverride_SceneFringeIntensity = true;      S.SceneFringeIntensity = 0.25f;
	S.bOverride_MotionBlurAmount = true;          S.MotionBlurAmount = 0.3f;
	S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = 0.6f;
	S.bOverride_AmbientOcclusionRadius = true;    S.AmbientOcclusionRadius = 120.f;
	S.bOverride_LumenFinalGatherQuality = true;   S.LumenFinalGatherQuality = 1.5f;
	S.bOverride_LumenReflectionQuality = true;    S.LumenReflectionQuality = 1.5f;
}

void UGraphicsSubsystem::LightThePlayer()
{
	UWorld* World = GetWorld();
	PlayerWait += 0.2f;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Player = *It;
		UCameraComponent* Camera = Player->FindComponentByClass<UCameraComponent>();
		if (!Player->IsPlayerControlled() || !Player->FindComponentByClass<USpearGripComponent>() || !Camera)
		{
			continue;
		}
		// Every mesh on the player also takes light from channel 1, where only this light lives.
		TArray<UPrimitiveComponent*> Parts;
		Player->GetComponents<UPrimitiveComponent>(Parts);
		for (UPrimitiveComponent* Part : Parts)
		{
			Part->SetLightingChannels(true, true, false);
		}
		UPointLightComponent* Key = NewObject<UPointLightComponent>(Player, TEXT("PlayerKeyLight"));
		Key->SetupAttachment(Camera);
		Key->SetRelativeLocation(FVector(60.f, 90.f, 70.f));   // just ahead of the camera, up and to the right
		Key->SetIntensityUnits(ELightUnits::Candelas);
		Key->SetIntensity(2.5f);
		Key->SetAttenuationRadius(900.f);
		Key->SetLightColor(FLinearColor(1.f, 0.88f, 0.75f));
		Key->SetCastShadows(false);
		Key->SetLightingChannels(false, true, false);
		Key->SetSpecularScale(0.4f);
		Key->RegisterComponent();
		World->GetTimerManager().ClearTimer(PlayerTimer);
		return;
	}
	if (PlayerWait > 10.f)
	{
		World->GetTimerManager().ClearTimer(PlayerTimer);
	}
}
