#include "DustComponent.h"
#include "DodgeComponent.h"
#include "FireFX.h"
#include "PlungeAttackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UDustComponent::UDustComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDustComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	// The template player's capsule radius is 42 cm.
	Scale = Character->GetCapsuleComponent()->GetScaledCapsuleRadius() / 42.f;
	Character->LandedDelegate.AddDynamic(this, &UDustComponent::HandleLanded);
	if (UDodgeComponent* Dodge = Character->FindComponentByClass<UDodgeComponent>())
	{
		Dodge->OnDodgeStarted.AddDynamic(this, &UDustComponent::HandleDodge);
	}
	if (UPlungeAttackComponent* Plunge = Character->FindComponentByClass<UPlungeAttackComponent>())
	{
		Plunge->OnPlungeLanded.AddDynamic(this, &UDustComponent::HandlePlunge);
	}
}

void UDustComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	const float Speed = Move ? Move->Velocity.Size2D() : 0.f;
	if (!Move || !Move->IsMovingOnGround() || Speed < MinSpeed || Character->IsHidden())
	{
		return;
	}

	// ponytail: puffs every stride of distance, not on the animation's real footfalls; anim notifies if it ever looks off.
	Travelled += Speed * DeltaTime;
	if (Travelled >= StrideLength * Scale)
	{
		Travelled = 0.f;
		bLeftFoot = !bLeftFoot;
		const FVector Side = Character->GetActorRightVector() * Character->GetCapsuleComponent()->GetScaledCapsuleRadius() * (bLeftFoot ? -0.4f : 0.4f);
		Puff(GetFeet() + Side, 50.f, 5);
	}
}

FVector UDustComponent::GetFeet() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void UDustComponent::Puff(const FVector& Location, float Radius, int32 Count) const
{
	AFireFX::Spawn(GetWorld(), Location, AFireFX::DustPreset(Radius * Scale, Count));
}

void UDustComponent::HandleLanded(const FHitResult& Hit)
{
	Puff(GetFeet(), 80.f, 12);
}

void UDustComponent::HandleDodge()
{
	Puff(GetFeet(), 70.f, 10);
}

void UDustComponent::HandlePlunge(FVector ImpactLocation)
{
	// Not scaled: the slam's own radius already sets the size.
	AFireFX::Spawn(GetWorld(), ImpactLocation, AFireFX::DustPreset(200.f, 40));
}

void UDustSetupSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.IsGameWorld())
	{
		// After the player is possessed and every character has its components.
		InWorld.GetTimerManager().SetTimer(SetupTimer, this, &UDustSetupSubsystem::AddToCharacters, 0.2f, false);
	}
}

void UDustSetupSubsystem::AddToCharacters()
{
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (!It->FindComponentByClass<UDustComponent>())
		{
			NewObject<UDustComponent>(*It, TEXT("Dust"))->RegisterComponent();
		}
	}
}
