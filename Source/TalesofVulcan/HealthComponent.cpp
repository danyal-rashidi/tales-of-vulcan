#include "HealthComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;

	if (AActor* MyOwner = GetOwner())
	{
		MyOwner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
	}

	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f || bInvulnerable || IsDead())
	{
		return;
	}

	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);

	if (bPrintDamageToScreen && GEngine)
	{
		const FString Message = FString::Printf(TEXT("%s took %.0f damage (%.0f / %.0f)"),
			*GetNameSafe(DamagedActor), Damage, Health, MaxHealth);
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, Message);
	}

	if (IsDead())
	{
		if (bPrintDamageToScreen && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("%s DIED"), *GetNameSafe(DamagedActor)));
		}
		OnDeath.Broadcast(DamageCauser);
	}
}

void UHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.f || IsDead())
	{
		return;
	}

	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UHealthComponent::ResetHealth()
{
	Health = MaxHealth;
	OnHealthChanged.Broadcast(Health, MaxHealth);
}
