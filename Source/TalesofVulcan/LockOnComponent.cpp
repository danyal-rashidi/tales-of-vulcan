#include "LockOnComponent.h"
#include "HealthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ULockOnComponent::ULockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After movement, so the camera aims from this frame's position.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

APlayerController* ULockOnComponent::GetPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

FVector ULockOnComponent::GetTargetPoint() const
{
	const AActor* Locked = Target.Get();
	if (!Locked)
	{
		return FVector::ZeroVector;
	}
	return Locked->GetActorLocation() + FVector(0.f, 0.f, Locked->GetSimpleCollisionHalfHeight() * TargetHeight);
}

AActor* ULockOnComponent::FindBestTarget() const
{
	APlayerController* Controller = GetPlayerController();
	AActor* Owner = GetOwner();
	if (!Controller || !Owner)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();

	// The living enemy closest to the centre of the screen, in range and in sight.
	AActor* Best = nullptr;
	float BestScore = -1.f;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		const UHealthComponent* Health = Candidate ? Candidate->FindComponentByClass<UHealthComponent>() : nullptr;
		if (!Candidate || Candidate == Owner || !Health || Health->IsDead() || Candidate->IsHidden())
		{
			continue;
		}

		const FVector Point = Candidate->GetActorLocation() + FVector(0.f, 0.f, Candidate->GetSimpleCollisionHalfHeight() * TargetHeight);
		if (FVector::Dist(Point, Owner->GetActorLocation()) > MaxDistance)
		{
			continue;
		}
		const float Facing = FVector::DotProduct((Point - ViewLocation).GetSafeNormal(), ViewDirection);
		if (Facing < 0.25f)
		{
			continue;
		}

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LockOnSight), false, Owner);
		Params.AddIgnoredActor(Candidate);
		if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, Point, ECC_Visibility, Params))
		{
			continue;
		}

		if (Facing > BestScore)
		{
			BestScore = Facing;
			Best = Candidate;
		}
	}
	return Best;
}

bool ULockOnComponent::ToggleLock()
{
	if (IsLocked())
	{
		ReleaseLock();
		return false;
	}

	APlayerController* Controller = GetPlayerController();
	AActor* Best = FindBestTarget();
	if (!Best)
	{
		// Nothing to lock onto: recentre the camera behind the player.
		if (Controller && GetOwner())
		{
			FRotator Behind = Controller->GetControlRotation();
			Behind.Yaw = GetOwner()->GetActorRotation().Yaw;
			Controller->SetControlRotation(Behind);
		}
		return false;
	}

	Target = Best;
	HiddenTime = 0.f;
	SetFacingTarget(true);

	if (Controller && !Marker)
	{
		Marker = CreateWidget<ULockOnMarkerWidget>(Controller, ULockOnMarkerWidget::StaticClass());
	}
	if (Marker)
	{
		Marker->SetDesiredSizeInViewport(FVector2D(16.f, 16.f));
		Marker->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
		Marker->AddToViewport(10);
	}
	return true;
}

void ULockOnComponent::ReleaseLock()
{
	if (!Target.IsValid() && !Marker)
	{
		return;
	}
	Target = nullptr;
	SetFacingTarget(false);
	if (Marker)
	{
		Marker->RemoveFromParent();
	}
}

void ULockOnComponent::SetFacingTarget(bool bFacing)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}

	if (bFacing)
	{
		// Turn toward the camera (which is aimed at the target) instead of toward the movement.
		bSavedOrientToMovement = Move->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = Move->bUseControllerDesiredRotation;
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = true;
	}
	else
	{
		Move->bOrientRotationToMovement = bSavedOrientToMovement;
		Move->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
	}
}

void ULockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APlayerController* Controller = GetPlayerController();
	AActor* Owner = GetOwner();
	if (!Controller || !Owner)
	{
		return;
	}

	if (Controller->WasInputKeyJustPressed(LockKey) || Controller->WasInputKeyJustPressed(AltLockKey))
	{
		ToggleLock();
	}

	AActor* Locked = Target.Get();
	if (!Locked)
	{
		if (Marker && Marker->IsInViewport())
		{
			ReleaseLock();
		}
		return;
	}

	const UHealthComponent* Health = Locked->FindComponentByClass<UHealthComponent>();
	if ((Health && Health->IsDead()) || FVector::Dist(Locked->GetActorLocation(), Owner->GetActorLocation()) > MaxDistance * 1.25f)
	{
		ReleaseLock();
		return;
	}

	// A hidden target (Vulcan under the lava) keeps the lock for a while but isn't tracked.
	if (Locked->IsHidden())
	{
		HiddenTime += DeltaTime;
		if (Marker)
		{
			Marker->SetVisibility(ESlateVisibility::Hidden);
		}
		if (HiddenTime > HiddenGrace)
		{
			ReleaseLock();
		}
		return;
	}
	HiddenTime = 0.f;

	// Swing the camera onto the target, looking a little down so the player stays in view.
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector Point = GetTargetPoint();
	FRotator Desired = (Point - ViewLocation).Rotation();
	Desired.Pitch = FMath::Clamp(Desired.Pitch + PitchOffset, -40.f, 15.f);
	Desired.Yaw += YawOffset;
	Desired.Roll = 0.f;
	Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaTime, CameraTurnSpeed));

	if (Marker)
	{
		FVector2D Screen;
		if (Controller->ProjectWorldLocationToScreen(Point, Screen, true))
		{
			Marker->SetPositionInViewport(Screen, true);
			Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Marker->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void ULockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseLock();
	Super::EndPlay(EndPlayReason);
}

void ULockOnMarkerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// A white dot with a soft dark rim.
		UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Dot"));
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.ImageSize = FVector2D(16.f, 16.f);
		Brush.TintColor = FSlateColor(FLinearColor(1.f, 0.97f, 0.9f, 0.95f));
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(8.f, 8.f, 8.f, 8.f), FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.6f)), 2.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Dot->SetBrush(Brush);
		WidgetTree->RootWidget = Dot;
	}
}

void ULockOnSetupSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.IsGameWorld())
	{
		// The player is possessed during begin play; add the component a moment later.
		InWorld.GetTimerManager().SetTimer(SetupTimer, this, &ULockOnSetupSubsystem::AddToPlayer, 0.2f, false);
	}
}

void ULockOnSetupSubsystem::AddToPlayer()
{
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (Player && !Player->FindComponentByClass<ULockOnComponent>())
	{
		ULockOnComponent* LockOn = NewObject<ULockOnComponent>(Player, TEXT("LockOn"));
		LockOn->RegisterComponent();
	}
}
