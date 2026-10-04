#include "PlayerHUDWidget.h"
#include "HealthComponent.h"
#include "StaminaComponent.h"
#include "PlayerDeathComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"

void UPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (YouDiedText)
	{
		YouDiedText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!BoundPawn.IsValid())
	{
		if (APawn* Pawn = GetOwningPlayerPawn())
		{
			SetupForPawn(Pawn);
		}
	}
}

void UPlayerHUDWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UPlayerHUDWidget::SetupForPawn(APawn* Pawn)
{
	if (!Pawn || BoundPawn.Get() == Pawn)
	{
		return;
	}

	Unbind();
	BoundPawn = Pawn;

	UPlayerDeathComponent* Death = Pawn->FindComponentByClass<UPlayerDeathComponent>();

	if (UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>())
	{
		Health->OnHealthChanged.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleHealthChanged);
		HandleHealthChanged(Health->Health, Health->MaxHealth);

		// Without a PlayerDeathComponent, fall back to the health component's death event.
		if (!Death)
		{
			Health->OnDeath.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleHealthDeath);
		}
	}

	if (UStaminaComponent* Stamina = Pawn->FindComponentByClass<UStaminaComponent>())
	{
		Stamina->OnStaminaChanged.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleStaminaChanged);
		HandleStaminaChanged(Stamina->Stamina, Stamina->MaxStamina);
	}

	if (Death)
	{
		Death->OnPlayerDied.AddUniqueDynamic(this, &UPlayerHUDWidget::HandlePlayerDied);
	}
}

void UPlayerHUDWidget::Unbind()
{
	if (APawn* Pawn = BoundPawn.Get())
	{
		if (UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>())
		{
			Health->OnHealthChanged.RemoveAll(this);
			Health->OnDeath.RemoveAll(this);
		}
		if (UStaminaComponent* Stamina = Pawn->FindComponentByClass<UStaminaComponent>())
		{
			Stamina->OnStaminaChanged.RemoveAll(this);
		}
		if (UPlayerDeathComponent* Death = Pawn->FindComponentByClass<UPlayerDeathComponent>())
		{
			Death->OnPlayerDied.RemoveAll(this);
		}
	}
	BoundPawn.Reset();
}

void UPlayerHUDWidget::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	if (HealthBar)
	{
		HealthBar->SetPercent(MaxHealth > 0.f ? NewHealth / MaxHealth : 0.f);
	}
}

void UPlayerHUDWidget::HandleStaminaChanged(float NewStamina, float MaxStamina)
{
	if (StaminaBar)
	{
		StaminaBar->SetPercent(MaxStamina > 0.f ? NewStamina / MaxStamina : 0.f);
	}
}

void UPlayerHUDWidget::HandlePlayerDied()
{
	if (YouDiedText)
	{
		YouDiedText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UPlayerHUDWidget::HandleHealthDeath(AActor* Killer)
{
	HandlePlayerDied();
}
