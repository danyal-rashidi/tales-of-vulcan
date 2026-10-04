#include "DodgeComponent.h"
#include "StaminaComponent.h"
#include "HealthComponent.h"
#include "PlungeAttackComponent.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UDodgeComponent::UDodgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UDodgeComponent::TryDodge()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || bDodging || World->GetTimeSeconds() < NextDodgeTime)
	{
		return false;
	}

	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Move || Move->IsFalling())
	{
		return false;
	}

	UHealthComponent* Health = Character->FindComponentByClass<UHealthComponent>();
	if (Health && Health->IsDead())
	{
		return false;
	}

	if (const UPlungeAttackComponent* Plunge = Character->FindComponentByClass<UPlungeAttackComponent>())
	{
		if (Plunge->IsPlunging())
		{
			return false;
		}
	}

	if (UStaminaComponent* Stamina = Character->FindComponentByClass<UStaminaComponent>())
	{
		if (!Stamina->TryUseStamina(StaminaCost))
		{
			return false;
		}
	}

	// Dodge where the player is pressing. No input = backstep.
	FVector Direction = Character->GetLastMovementInputVector();
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		Direction = -Character->GetActorForwardVector();
		Direction.Z = 0.f;
	}
	else
	{
		Character->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}
	Direction = Direction.GetSafeNormal();

	// Remove friction for the dash so the character slides at full speed.
	SavedGroundFriction = Move->GroundFriction;
	SavedBrakingFriction = Move->BrakingFriction;
	SavedBrakingDeceleration = Move->BrakingDecelerationWalking;
	Move->GroundFriction = 0.f;
	Move->BrakingFriction = 0.f;
	Move->BrakingDecelerationWalking = 0.f;
	Move->Velocity = Direction * DodgeSpeed;

	bDodging = true;

	if (Health)
	{
		Health->bInvulnerable = true;
		World->GetTimerManager().SetTimer(IFrameTimer, this, &UDodgeComponent::EndIFrames, FMath::Max(IFrameDuration, 0.01f), false);
	}

	if (DodgeMontage)
	{
		Character->PlayAnimMontage(DodgeMontage);
	}

	World->GetTimerManager().SetTimer(DodgeTimer, this, &UDodgeComponent::EndDodge, FMath::Max(DodgeDuration, 0.01f), false);
	OnDodgeStarted.Broadcast();
	return true;
}

void UDodgeComponent::EndIFrames()
{
	if (AActor* Owner = GetOwner())
	{
		if (UHealthComponent* Health = Owner->FindComponentByClass<UHealthComponent>())
		{
			Health->bInvulnerable = false;
		}
	}
}

void UDodgeComponent::EndDodge()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->GroundFriction = SavedGroundFriction;
			Move->BrakingFriction = SavedBrakingFriction;
			Move->BrakingDecelerationWalking = SavedBrakingDeceleration;
			Move->Velocity = Move->Velocity.GetClampedToMaxSize(Move->MaxWalkSpeed);
		}
	}

	bDodging = false;

	if (UWorld* World = GetWorld())
	{
		NextDodgeTime = World->GetTimeSeconds() + Cooldown;
	}

	OnDodgeEnded.Broadcast();
}
