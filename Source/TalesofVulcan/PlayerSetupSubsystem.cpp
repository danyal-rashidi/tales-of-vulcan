#include "PlayerSetupSubsystem.h"
#include "GameHUDWidget.h"
#include "GameFramework/PlayerController.h"
#include "PlayerAnimInstance.h"
#include "PlayerEquipComponent.h"
#include "SpearGripComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Landscape.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarPlayerSetup(TEXT("tov.PlayerSetup"), 1, TEXT("1: the player gets the C++ locomotion and draw/sheathe/sprint. 0: the old animation Blueprint."));

bool UPlayerSetupSubsystem::IsEnabled()
{
	return CVarPlayerSetup.GetValueOnGameThread() != 0;
}

bool UPlayerSetupSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UPlayerSetupSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!IsEnabled())
	{
		return;
	}
	bOpenMap = TActorIterator<ALandscapeProxy>(&InWorld) ? true : false;
	// The player can spawn a little after play starts: look for a few seconds.
	InWorld.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UWorld* World = GetWorld();
		Elapsed += 0.05f;
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (It->FindComponentByClass<USpearGripComponent>() && !It->FindComponentByClass<UPlayerEquipComponent>())
			{
				SetUp(*It);
			}
		}
		// The new HUD, once the player is possessing a character that's been fitted out.
		APlayerController* Controller = World->GetFirstPlayerController();
		if (!bHUD && Controller && Controller->IsLocalController() && Controller->GetPawn() && Controller->GetPawn()->FindComponentByClass<UPlayerEquipComponent>())
		{
			if (UGameHUDWidget* HUD = CreateWidget<UGameHUDWidget>(Controller, UGameHUDWidget::StaticClass()))
			{
				HUD->AddToViewport(4);
				bHUD = true;
			}
		}
		if (Elapsed > 10.f)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
	}), 0.05f, true, 0.f);
}

void UPlayerSetupSubsystem::SetUp(ACharacter* Character)
{
	if (USkeletalMeshComponent* Body = Character->GetMesh())
	{
		Body->SetAnimInstanceClass(UPlayerAnimInstance::StaticClass());
	}
	UPlayerEquipComponent* Equip = NewObject<UPlayerEquipComponent>(Character, TEXT("Equip"));
	Equip->RegisterComponent();
	Equip->SetArmedImmediately(!bOpenMap);
}
