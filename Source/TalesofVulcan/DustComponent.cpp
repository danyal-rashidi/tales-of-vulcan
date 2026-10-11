#include "DustComponent.h"
#include "DodgeComponent.h"
#include "GameAudio.h"
#include "GroundCheckSubsystem.h"
#include "FireFX.h"
#include "PlungeAttackComponent.h"
#include "Components/CapsuleComponent.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
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
	if (!Move || !Move->IsMovingOnGround() || Character->IsHidden())
	{
		return;
	}

	// Footstep sounds at any pace: shorter strides and softer steps when walking, longer and louder when running.
	if (Speed >= 60.f)
	{
		const float Pace = FMath::Clamp((Speed - 150.f) / 450.f, 0.f, 1.f);
		StepTravelled += Speed * DeltaTime;
		if (StepTravelled >= FMath::Lerp(90.f, 200.f, Pace) * Scale)
		{
			StepTravelled = 0.f;
			PlayStep(Pace);
		}
	}
	if (Speed < MinSpeed)
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

void UDustComponent::PlayStep(float Pace) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character->IsPlayerControlled())
	{
		const TCHAR* Set = StepSoundUnderfoot();
		GameAudio::Play(this, Set, GetFeet(), FMath::Lerp(0.22f, 0.5f, Pace), 1.f, 2200.f);
		if (UGroundCheckSubsystem::IsTestRun())
		{
			UE_LOG(LogTemp, Display, TEXT("[Steps] player %s at speed %.0f"), Set, Character->GetVelocity().Size2D());
		}
	}
	else
	{
		if (UGroundCheckSubsystem::IsTestRun())
		{
			UE_LOG(LogTemp, Display, TEXT("[Steps] %s BossStep at speed %.0f"), *Character->GetName(), Character->GetVelocity().Size2D());
		}
		// Vulcan stomps (the old version used a pitched-down thud only once he ran).
		GameAudio::Play(this, TEXT("BossStep"), GetFeet(), FMath::Lerp(0.6f, 1.1f, Pace), 1.f, 7000.f);
	}
}

const TCHAR* UDustComponent::StepSoundUnderfoot() const
{
	const FVector Feet = GetFeet();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Footstep), /*bTraceComplex=*/true, GetOwner());
	Params.bReturnFaceIndex = true;
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, 30.f), Feet - FVector(0.f, 0.f, 120.f), ECC_Visibility, Params))
	{
		return TEXT("Step_Sand");
	}
	if (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>())
	{
		// The Rome map's town paving and the paved road in from the colosseum: the same shapes M_RomeLandscape paves
		// (rome_landscape_material.py), in metres from the colosseum centre.
		const FVector2D P = (FVector2D(Hit.ImpactPoint) - FVector2D(-44.f, -5.f)) / 100.f;
		const bool bTown = FMath::Abs(P.X) < 158.f && FMath::Abs(P.Y - 330.f) < 158.f;
		const bool bRoad = FMath::Abs(P.X) < 7.5f && P.Y > 36.f && P.Y < 173.f;
		if (bTown || bRoad)
		{
			return TEXT("Step_Stone");
		}
		// The gravel roads out of town (the same lines M_RomeLandscape paints with gravel).
		static const float Roads[][5] = { { 0.f, 488.f, 0.f, 1750.f, 6.5f }, { 0.f, -40.f, 0.f, -1750.f, 5.f }, { 45.f, 0.f, 470.f, 150.f, 5.f },
			{ -260.f, 380.f, -345.f, 420.f, 4.5f }, { -160.f, 386.f, -260.f, 380.f, 4.5f }, { 160.f, 330.f, 196.f, 330.f, 4.5f } };   // from, to, half-width (m)
		for (const float* Road : Roads)
		{
			const FVector2D A(Road[0], Road[1]), B(Road[2], Road[3]);
			if (FMath::PointDistToSegment(FVector(P, 0.f), FVector(A, 0.f), FVector(B, 0.f)) < Road[4] * 0.85f)
			{
				return TEXT("Step_Gravel");
			}
		}
		return Hit.ImpactNormal.Z < 0.85f ? TEXT("Step_Gravel") : TEXT("Step_Sand");
	}
	// Meshes: go by the material under the foot (our Rome materials are named for what they are).
	const UPrimitiveComponent* Component = Hit.GetComponent();
	const UMaterialInterface* Material = nullptr;
	if (Component)
	{
		int32 Section = 0;
		Material = Hit.FaceIndex != INDEX_NONE ? Component->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section) : nullptr;
		Material = Material ? Material : Component->GetMaterial(0);
	}
	const FString Name = Material ? Material->GetName() : FString();
	if (Name.Contains(TEXT("Wood")))
	{
		return TEXT("Step_Wood");
	}
	if (Name.Contains(TEXT("Rock")) || Name.Contains(TEXT("Boulder")) || Name.Contains(TEXT("Photoscan")))
	{
		return TEXT("Step_Gravel");
	}
	for (const TCHAR* Stone : { TEXT("Plaster"), TEXT("Ashlar"), TEXT("Brick"), TEXT("Stone"), TEXT("Surface"), TEXT("Marble"), TEXT("Tile"), TEXT("Paving") })
	{
		if (Name.Contains(Stone))
		{
			return TEXT("Step_Stone");
		}
	}
	return TEXT("Step_Sand");
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
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->IsPlayerControlled())
	{
		GameAudio::Play(this, TEXT("Land"), GetFeet(), 0.7f, 1.f, 2500.f);
		GameAudio::Play(this, StepSoundUnderfoot(), GetFeet(), 0.45f, 0.9f, 2500.f);
	}
	else
	{
		GameAudio::Play(this, TEXT("Thud"), GetFeet(), 0.5f * Scale, 1.1f / FMath::Max(Scale, 1.f), 3000.f);
	}
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
