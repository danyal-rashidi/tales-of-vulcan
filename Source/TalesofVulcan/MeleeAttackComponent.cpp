#include "MeleeAttackComponent.h"
#include "StaminaComponent.h"
#include "HealthComponent.h"
#include "DodgeComponent.h"
#include "PlungeAttackComponent.h"
#include "SpearGripComponent.h"
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

	GatherHitTimes();
	NextHit = 0;
	OnAttackStarted.Broadcast();

	FTimerManager& Timers = World->GetTimerManager();
	Timers.SetTimer(AttackTimer, this, &UMeleeAttackComponent::DoHit, HitTimes[0], false);
	Timers.SetTimer(EndTimer, this, &UMeleeAttackComponent::EndAttack, FMath::Max(AttackDuration, HitTimes.Last() + 0.01f), false);
	return true;
}

void UMeleeAttackComponent::GatherHitTimes()
{
	HitTimes.Reset();

	// One hit per notify placed on a strike frame, so the damage matches the animation at any Rate Scale.
	if (AttackMontage && !HitNotifyName.IsNone())
	{
		const float Rate = FMath::Max(AttackMontage->RateScale, 0.01f);
		for (const FAnimNotifyEvent& Notify : AttackMontage->Notifies)
		{
			if (Notify.NotifyName == HitNotifyName)
			{
				HitTimes.Add(FMath::Max(Notify.GetTriggerTime() / Rate, 0.01f));
			}
		}
		HitTimes.Sort();
	}

	if (HitTimes.IsEmpty())
	{
		HitTimes.Add(FMath::Max(HitDelay, 0.01f));
	}
}

void UMeleeAttackComponent::DoHit()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	// Default hit shape: a sphere HitDistance in front of the owner.
	FVector Center = Character->GetActorLocation() + Character->GetActorForwardVector() * HitDistance;
	FQuat ShapeRotation = FQuat::Identity;
	FCollisionShape Shape = FCollisionShape::MakeSphere(HitRadius);

	// With a spear in hand, hit wherever its front two thirds actually are on this frame.
	FVector SpearBack, SpearTip;
	const USpearGripComponent* Grip = bHitAlongWeapon ? Character->FindComponentByClass<USpearGripComponent>() : nullptr;
	if (Grip && Grip->GetSpearSegment(SpearBack, SpearTip))
	{
		const FVector Start = FMath::Lerp(SpearBack, SpearTip, 0.35f);
		Center = (Start + SpearTip) * 0.5f;
		ShapeRotation = FRotationMatrix::MakeFromZ(SpearTip - Start).ToQuat();
		Shape = FCollisionShape::MakeCapsule(HitRadius, 0.5f * FVector::Dist(Start, SpearTip) + HitRadius);
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		ShapeRotation,
		FCollisionObjectQueryParams(ECC_Pawn),
		Shape,
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
		const FColor Color = AlreadyHit.Num() > 0 ? FColor::Green : FColor::Red;
		if (Shape.IsCapsule())
		{
			DrawDebugCapsule(World, Center, Shape.GetCapsuleHalfHeight(), HitRadius, ShapeRotation, Color, false, 0.4f);
		}
		else
		{
			DrawDebugSphere(World, Center, HitRadius, 16, Color, false, 0.4f);
		}
	}

	// Line up the next swing if the montage has more than one Hit notify.
	++NextHit;
	if (HitTimes.IsValidIndex(NextHit))
	{
		World->GetTimerManager().SetTimer(AttackTimer, this, &UMeleeAttackComponent::DoHit, FMath::Max(HitTimes[NextHit] - HitTimes[NextHit - 1], 0.01f), false);
	}
}

void UMeleeAttackComponent::EndAttack()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimer);
	}

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = SavedMaxWalkSpeed;
		}
	}

	bAttacking = false;
}
