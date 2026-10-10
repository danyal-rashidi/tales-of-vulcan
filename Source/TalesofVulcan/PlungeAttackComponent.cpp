#include "PlungeAttackComponent.h"
#include "PlayerEquipComponent.h"
#include "GameAudio.h"
#include "SlamCameraShake.h"
#include "StaminaComponent.h"
#include "HealthComponent.h"
#include "DodgeComponent.h"
#include "MeleeAttackComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UPlungeAttackComponent::UPlungeAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	LandingCameraShake = USlamCameraShake::StaticClass();
}

void UPlungeAttackComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LandedDelegate.AddDynamic(this, &UPlungeAttackComponent::HandleLanded);
		Character->OnActorHit.AddDynamic(this, &UPlungeAttackComponent::HandleActorHit);
	}
}

bool UPlungeAttackComponent::TryPlunge()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || State != EPlungeState::None)
	{
		return false;
	}

	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Move || !Move->IsFalling())
	{
		return false;
	}
	if (const UPlayerEquipComponent* Equip = Character->FindComponentByClass<UPlayerEquipComponent>(); Equip && (!Equip->IsArmed() || Equip->IsBusy()))
	{
		return false; // no plunge with the spear on the back
	}

	// Too close to the floor? Trace down from the bottom of the capsule.
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(PlungeGroundCheck), false, Character);
	FHitResult Floor;
	if (World->LineTraceSingleByChannel(Floor, Feet, Feet - FVector(0.f, 0.f, MinHeightAboveGround), ECC_Pawn, TraceParams))
	{
		return false;
	}

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

	// Freeze in the air for a moment before slamming down.
	State = EPlungeState::Hanging;
	SavedGravityScale = Move->GravityScale;
	SavedAirControl = Move->AirControl;
	Move->GravityScale = 0.f;
	Move->AirControl = 0.f;
	Move->StopMovementImmediately();

	if (PlungeMontage)
	{
		const FName StartSection = PlungeMontage->IsValidSectionName(FallSection) ? FallSection : NAME_None;
		Character->PlayAnimMontage(PlungeMontage, 1.f, StartSection);
	}

	GameAudio::Play(this, TEXT("Swing"), Character->GetActorLocation(), 0.9f, 0.55f);
	OnPlungeStarted.Broadcast();
	World->GetTimerManager().SetTimer(PlungeTimer, this, &UPlungeAttackComponent::StartDive, FMath::Max(HangTime, 0.01f), false);
	return true;
}

void UPlungeAttackComponent::StartDive()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		CancelPlunge();
		return;
	}

	State = EPlungeState::Diving;
	Move->GravityScale = SavedGravityScale;
	Move->Velocity = FVector(0.f, 0.f, -PlungeSpeed);

	GetWorld()->GetTimerManager().SetTimer(PlungeTimer, this, &UPlungeAttackComponent::CancelPlunge, FMath::Max(MaxPlungeTime, 0.01f), false);
}

void UPlungeAttackComponent::HandleLanded(const FHitResult& Hit)
{
	if (State == EPlungeState::Hanging || State == EPlungeState::Diving)
	{
		Impact(Hit.ImpactPoint);
	}
}

void UPlungeAttackComponent::HandleActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	// Slamming onto the boss's head counts as an impact, even though the player doesn't "land" there.
	if (State == EPlungeState::Diving && Cast<APawn>(OtherActor))
	{
		Impact(Hit.ImpactPoint);
	}
}

void UPlungeAttackComponent::Impact(const FVector& ImpactLocation)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(PlungeTimer);
	State = EPlungeState::Recovering;

	// Restore air movement and root the player in place for the recovery.
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->GravityScale = SavedGravityScale;
		Move->AirControl = SavedAirControl;
		Move->StopMovementImmediately();
		SavedMaxWalkSpeed = Move->MaxWalkSpeed;
		Move->MaxWalkSpeed = 0.f;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		ImpactLocation,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		Params);

	// An actor can have several overlapping parts; only damage it once per slam.
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
	}

	if (bShowDebug)
	{
		DrawDebugSphere(World, ImpactLocation, Radius, 24, AlreadyHit.Num() > 0 ? FColor::Green : FColor::Red, false, 0.6f);
	}

	if (PlungeMontage && PlungeMontage->IsValidSectionName(SlamSection))
	{
		if (UAnimInstance* Anim = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			if (Anim->Montage_IsPlaying(PlungeMontage))
			{
				Anim->Montage_JumpToSection(SlamSection, PlungeMontage);
			}
		}
	}

	if (LandingCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->ClientStartCameraShake(LandingCameraShake, CameraShakeScale);
		}
	}

	// Deep thump, cracking stone and scattering pebbles.
	GameAudio::Play(this, TEXT("Thud"), ImpactLocation, 1.1f, 0.55f, 5000.f);
	GameAudio::Play(this, TEXT("Crack"), ImpactLocation, 0.5f, 0.7f, 5000.f);
	GameAudio::Play(this, TEXT("Stones"), ImpactLocation, 1.8f);
	OnPlungeLanded.Broadcast(ImpactLocation);
	World->GetTimerManager().SetTimer(PlungeTimer, this, &UPlungeAttackComponent::EndRecovery, FMath::Max(LandingRecovery, 0.01f), false);
}

void UPlungeAttackComponent::CancelPlunge()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->GravityScale = SavedGravityScale;
			Move->AirControl = SavedAirControl;
		}

		if (PlungeMontage)
		{
			Character->StopAnimMontage(PlungeMontage);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlungeTimer);
	}

	State = EPlungeState::None;
}

void UPlungeAttackComponent::EndRecovery()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = SavedMaxWalkSpeed;
		}
	}

	State = EPlungeState::None;
}
