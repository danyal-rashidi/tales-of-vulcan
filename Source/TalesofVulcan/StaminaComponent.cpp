#include "StaminaComponent.h"

UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStaminaComponent::BeginPlay()
{
	Super::BeginPlay();

	TimeSinceSpent = RegenDelay;
	SetStamina(MaxStamina);
}

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceSpent += DeltaTime;

	if (TimeSinceSpent >= RegenDelay && Stamina < MaxStamina)
	{
		SetStamina(Stamina + RegenPerSecond * DeltaTime);
	}
}

bool UStaminaComponent::CanAfford(float Cost) const
{
	return bAllowActionWhenLow ? Stamina > 0.f : Stamina >= Cost;
}

bool UStaminaComponent::TryUseStamina(float Cost)
{
	if (!CanAfford(Cost))
	{
		return false;
	}

	SetStamina(Stamina - FMath::Max(Cost, 0.f));
	TimeSinceSpent = 0.f;
	return true;
}

void UStaminaComponent::RefillStamina()
{
	SetStamina(MaxStamina);
}

void UStaminaComponent::SetStamina(float NewValue)
{
	const float Clamped = FMath::Clamp(NewValue, 0.f, MaxStamina);
	if (!FMath::IsNearlyEqual(Clamped, Stamina))
	{
		Stamina = Clamped;
		OnStaminaChanged.Broadcast(Stamina, MaxStamina);
	}
}
