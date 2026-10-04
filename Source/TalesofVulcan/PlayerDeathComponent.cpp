#include "PlayerDeathComponent.h"
#include "GameAudio.h"
#include "HealthComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UPlayerDeathComponent::UPlayerDeathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerDeathComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
		{
			Health->OnDeath.AddDynamic(this, &UPlayerDeathComponent::HandleDeath);
			Health->OnHealthChanged.AddDynamic(this, &UPlayerDeathComponent::HandleHealthChanged);
		}
	}
}

void UPlayerDeathComponent::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	const bool bHurt = NewHealth < (LastHealth < 0.f ? MaxHealth : LastHealth);
	LastHealth = NewHealth;
	if (bHurt && NewHealth > 0.f && GetWorld()->GetTimeSeconds() >= NextHurtSoundTime)
	{
		NextHurtSoundTime = GetWorld()->GetTimeSeconds() + 0.4f;
		GameAudio::Play(this, TEXT("Hit"), GetOwner()->GetActorLocation(), 0.8f, 0.75f);
	}
}

void UPlayerDeathComponent::HandleDeath(AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	// No more moving, attacking or dodging.
	if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
	{
		Character->DisableInput(PC);
	}

	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}

	// Cancel any attack or roll animation, then fall over.
	Character->StopAnimMontage();
	if (DeathMontage)
	{
		Character->PlayAnimMontage(DeathMontage);
	}

	// Body hits the sand, then a deep toll under "YOU DIED".
	GameAudio::Play(this, TEXT("Thud"), Character->GetActorLocation(), 1.2f, 0.6f);
	GameAudio::Play2D(this, TEXT("Bell"), 0.7f, 0.35f);

	OnPlayerDied.Broadcast();

	if (RestartDelay > 0.f)
	{
		World->GetTimerManager().SetTimer(RestartTimer, this, &UPlayerDeathComponent::RestartLevel, RestartDelay, false);
	}
}

void UPlayerDeathComponent::RestartLevel()
{
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}
