#include "PlayerEquipComponent.h"
#include "DodgeComponent.h"
#include "GameAudio.h"
#include "HealthComponent.h"
#include "MeleeAttackComponent.h"
#include "PlayerAnimInstance.h"
#include "PlungeAttackComponent.h"
#include "SpearGripComponent.h"
#include "StaminaComponent.h"
#include "VulcanBoss.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

UPlayerEquipComponent::UPlayerEquipComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // after animation, like the spear grip
}

void UPlayerEquipComponent::BeginPlay()
{
	Super::BeginPlay();
	DrawClip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Player/UAL/A_Q_SwordEnter.A_Q_SwordEnter"));
	SheatheClip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Player/UAL/A_Q_SwordExit.A_Q_SwordExit"));
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (const UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			BaseSpeed = Move->MaxWalkSpeed;
			LastSetSpeed = BaseSpeed;
		}
	}
}

UPlayerAnimInstance* UPlayerEquipComponent::GetAnim() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character && Character->GetMesh() ? Cast<UPlayerAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
}

void UPlayerEquipComponent::SetArmedImmediately(bool bInArmed)
{
	bArmed = bInArmed;
	Action = EAction::None;
	if (USpearGripComponent* Grip = GetOwner()->FindComponentByClass<USpearGripComponent>())
	{
		Grip->bHolstered = !bArmed;
	}
}

bool UPlayerEquipComponent::CanChangeWeapon() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || Action != EAction::None)
	{
		return false;
	}
	if (const UCharacterMovementComponent* Move = Character->GetCharacterMovement(); Move && Move->IsFalling())
	{
		return false;
	}
	if (const UHealthComponent* Health = Character->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead())
	{
		return false;
	}
	TArray<UMeleeAttackComponent*> Attacks;
	Character->GetComponents<UMeleeAttackComponent>(Attacks);
	for (const UMeleeAttackComponent* Attack : Attacks)
	{
		if (Attack->IsAttacking())
		{
			return false;
		}
	}
	if (const UDodgeComponent* Dodge = Character->FindComponentByClass<UDodgeComponent>(); Dodge && Dodge->IsDodging())
	{
		return false;
	}
	return true;
}

void UPlayerEquipComponent::Draw()
{
	if (bArmed || !CanChangeWeapon())
	{
		return;
	}
	Action = EAction::Drawing;
	ActionTime = 0.f;
	bSwapped = false;
	if (UPlayerAnimInstance* Anim = GetAnim())
	{
		Anim->PlayUpperBody(DrawClip, DrawRate);
	}
}

void UPlayerEquipComponent::Sheathe()
{
	if (!bArmed || !CanChangeWeapon())
	{
		return;
	}
	Action = EAction::Sheathing;
	ActionTime = 0.f;
	bSwapped = false;
	if (UPlayerAnimInstance* Anim = GetAnim())
	{
		Anim->PlayUpperBody(SheatheClip, DrawRate);
	}
}

void UPlayerEquipComponent::Toggle()
{
	bArmed ? Sheathe() : Draw();
}

void UPlayerEquipComponent::TryBindInput()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	const ULocalPlayer* Local = Controller->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}
	SprintAction = NewObject<UInputAction>(this, TEXT("IA_SprintOrRoll"));
	SprintAction->ValueType = EInputActionValueType::Boolean;
	EquipAction = NewObject<UInputAction>(this, TEXT("IA_DrawSheathe"));
	EquipAction->ValueType = EInputActionValueType::Boolean;
	// Above the game's own context: Shift goes here instead of straight to the dodge (inputs are consumed by priority).
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Equip"));
	Mapping->MapKey(SprintAction, EKeys::LeftShift);
	Mapping->MapKey(SprintAction, EKeys::Gamepad_FaceButton_Right);
	Mapping->MapKey(EquipAction, EKeys::R);
	Mapping->MapKey(EquipAction, EKeys::Gamepad_FaceButton_Top);
	Subsystem->AddMappingContext(Mapping, 10);

	Input = NewObject<UEnhancedInputComponent>(Controller, TEXT("EquipInput"));
	Input->RegisterComponent();
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &UPlayerEquipComponent::SprintPressed);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &UPlayerEquipComponent::SprintReleased);
	Input->BindAction(EquipAction, ETriggerEvent::Started, this, &UPlayerEquipComponent::Toggle);
	Controller->PushInputComponent(Input);
}

void UPlayerEquipComponent::SprintPressed()
{
	bSprintHeld = true;
	bSprintUsed = false;
	SprintHeldTime = 0.f;
}

void UPlayerEquipComponent::SprintReleased()
{
	// A tap is a roll; a hold that turned into a sprint is not.
	if (bSprintHeld && !bSprintUsed && SprintHeldTime < 0.3f)
	{
		if (UDodgeComponent* Dodge = GetOwner()->FindComponentByClass<UDodgeComponent>())
		{
			Dodge->TryDodge();
		}
	}
	bSprintHeld = false;
	bSprinting = false;
	bExhausted = false;
}

void UPlayerEquipComponent::PlaceOnBack()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USpearGripComponent* Grip = Character ? Character->FindComponentByClass<USpearGripComponent>() : nullptr;
	const USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Grip || !Body || Body->GetBoneIndex(TEXT("spine_04")) == INDEX_NONE)
	{
		return;
	}
	// The torso's own frame, so the spear leans and twists with it.
	const FVector Chest = Body->GetBoneLocation(TEXT("spine_04"));
	FVector Up = (Body->GetBoneLocation(TEXT("neck_01")) - Body->GetBoneLocation(TEXT("spine_02"))).GetSafeNormal();
	if (Up.IsNearlyZero())
	{
		Up = FVector::UpVector;
	}
	FVector Forward = Character->GetActorForwardVector();
	Forward = (Forward - Up * FVector::DotProduct(Forward, Up)).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Up, Forward);
	// Butt end low on the left, point up past the right shoulder, standing off the back over the cape.
	const FVector Dir = (Up * FMath::Cos(FMath::DegreesToRadians(32.f)) + Right * FMath::Sin(FMath::DegreesToRadians(32.f))).GetSafeNormal();
	const FVector Center = Chest - Forward * 24.f - Up * 12.f;
	Grip->PlaceSpear(Center, Dir, -Forward);
}

void UPlayerEquipComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	if (!Input)
	{
		TryBindInput();
	}

	// ---- drawing / sheathing: move the spear when the hand reaches the back
	if (Action != EAction::None)
	{
		ActionTime += DeltaTime * DrawRate;
		const UAnimSequence* Clip = Action == EAction::Drawing ? DrawClip.Get() : SheatheClip.Get();
		const float Length = Clip ? Clip->GetPlayLength() : 0.5f;
		const float SwapAt = (Action == EAction::Drawing ? DrawSwapAt : SheatheSwapAt) * Length;
		if (!bSwapped && ActionTime >= SwapAt)
		{
			bSwapped = true;
			bArmed = Action == EAction::Drawing;
			if (USpearGripComponent* Grip = Character->FindComponentByClass<USpearGripComponent>())
			{
				Grip->bHolstered = !bArmed;
			}
			GameAudio::Play(this, TEXT("Swing"), Character->GetActorLocation(), bArmed ? 1.25f : 0.9f, 0.25f, 1500.f);
		}
		if (ActionTime >= Length)
		{
			Action = EAction::None;
		}
	}
	if (!bArmed)
	{
		PlaceOnBack();
	}

	// ---- when Vulcan wakes up nearby, take the spear in hand
	BossCheckTime -= DeltaTime;
	if (!bAutoDrawn && BossCheckTime <= 0.f)
	{
		BossCheckTime = 0.5f;
		for (TActorIterator<AVulcanBoss> It(GetWorld()); It; ++It)
		{
			if (!It->IsStatue() && !It->IsDefeated() && FVector::Dist(It->GetActorLocation(), Character->GetActorLocation()) < 5000.f)
			{
				bAutoDrawn = true;
				Draw();
			}
		}
	}

	// ---- sprinting
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!Move)
	{
		return;
	}
	if (bSprintHeld)
	{
		SprintHeldTime += DeltaTime;
	}
	const bool bMoving = Move->Velocity.Size2D() > 150.f && !Move->IsFalling();
	if (bSprintHeld && SprintHeldTime > 0.22f && bMoving && !bExhausted)
	{
		UStaminaComponent* Stamina = Character->FindComponentByClass<UStaminaComponent>();
		if (!Stamina || Stamina->TryUseStamina(SprintStaminaPerSecond * DeltaTime))
		{
			bSprinting = true;
			bSprintUsed = true;
		}
		else
		{
			bSprinting = false;
			bExhausted = true; // out of breath: let go and press again
		}
	}
	else if (!bMoving || !bSprintHeld)
	{
		bSprinting = false;
	}
	// Only touch the speed while nobody else has (attacks slow the player and put it back afterwards).
	const float Desired = bSprinting ? SprintSpeed : BaseSpeed;
	if (FMath::IsNearlyEqual(Move->MaxWalkSpeed, LastSetSpeed, 1.f) && !FMath::IsNearlyEqual(Move->MaxWalkSpeed, Desired, 1.f))
	{
		Move->MaxWalkSpeed = Desired;
		LastSetSpeed = Desired;
	}
	SprintAlpha = FMath::FInterpTo(SprintAlpha, bSprinting ? 1.f : 0.f, DeltaTime, 4.f);
	if (UCameraComponent* Camera = Character->FindComponentByClass<UCameraComponent>())
	{
		if (BaseFOV < 0.f)
		{
			BaseFOV = Camera->FieldOfView;
		}
		Camera->SetFieldOfView(BaseFOV + SprintFOV * SprintAlpha);
	}
}
