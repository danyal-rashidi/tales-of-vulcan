#include "MeleeAttackComponent.h"
#include "StaminaComponent.h"
#include "HealthComponent.h"
#include "DodgeComponent.h"
#include "PlungeAttackComponent.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UMeleeAttackComponent::UMeleeAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UMeleeAttackComponent::TryAttack()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || bAttacking)
	{
		return false;
	}

	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Move || Move->IsFalling())
	{
		return false;
	}

	// Light and heavy attack are two of these components; never let them overlap.
	TArray<UMeleeAttackComponent*> AllAttacks;
	Character->GetComponents<UMeleeAttackComponent>(AllAttacks);
	for (const UMeleeAttackComponent* Attack : AllAttacks)
	{
		if (Attack->IsAttacking())
		{
			return false;
		}
	}

	if (const UDodgeComponent* Dodge = Character->FindComponentByClass<UDodgeComponent>())
	{
		if (Dodge->IsDodging())
		{
			return false;
		}
	}

	if (const UPlungeAttackComponent* Plunge = Character->FindComponentByClass<UPlungeAttackComponent>())
	{
		if (Plunge->IsPlunging())
		{
			return false;
		}
	}

	if (const UHealthComponent* Health = Character->FindComponentByClass<UHealthComponent>())
	{
		if (Health->IsDead())
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

	// Swing toward the direction the player is pressing.
	FVector Direction = Character->GetLastMovementInputVector();
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		Character->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}

	bAttacking = true;
	SavedMaxWalkSpeed = Move->MaxWalkSpeed;
	Move->MaxWalkSpeed = MoveSpeedWhileAttacking;

	if (AttackMontage)
	{
		Character->PlayAnimMontage(AttackMontage);
	}

	OnAttackStarted.Broadcast();
	World->GetTimerManager().SetTimer(AttackTimer, this, &UMeleeAttackComponent::DoHit, FMath::Max(HitDelay, 0.01f), false);
	return true;
}

void UMeleeAttackComponent::DoHit()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		EndAttack();
		return;
	}

	const FVector Center = Character->GetActorLocation() + Character->GetActorForwardVector() * HitDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(HitRadius),
		Params);

	// An actor can have several overlapping parts; only damage it once per swing.
	TSet<AActor*> AlreadyHit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActor == Character || AlreadyHit.Contains(HitActor))
		{
			continue;
		}
		AlreadyHit.Add(HitActor);

		UGameplayStatics::ApplyDamage(HitActor, Damage, Character->GetController(), Character, UDamageType::StaticClass());
		OnAttackHit.Broadcast(HitActor, HitActor->GetActorLocation());
	}

	if (bShowDebug)
	{
		DrawDebugSphere(World, Center, HitRadius, 16, AlreadyHit.Num() > 0 ? FColor::Green : FColor::Red, false, 0.4f);
	}

	World->GetTimerManager().SetTimer(AttackTimer, this, &UMeleeAttackComponent::EndAttack, FMath::Max(AttackDuration - HitDelay, 0.01f), false);
}

void UMeleeAttackComponent::EndAttack()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = SavedMaxWalkSpeed;
		}
	}

	bAttacking = false;
}
