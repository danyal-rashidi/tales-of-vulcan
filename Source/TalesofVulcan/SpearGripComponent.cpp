#include "SpearGripComponent.h"
#include "HipRootMotionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

USpearGripComponent::USpearGripComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After animation, so the spear follows this frame's hands.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void USpearGripComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component->GetFName() == WeaponComponentName)
		{
			Weapon = Cast<UStaticMeshComponent>(Component);
		}
		else if (Component->GetFName() == HandsMeshName)
		{
			Hands = Cast<USkeletalMeshComponent>(Component);
		}
	}

	UStaticMeshComponent* WeaponComponent = Weapon.Get();
	USkeletalMeshComponent* HandsComponent = Hands.Get();
	if (!WeaponComponent || !HandsComponent || !WeaponComponent->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("SpearGrip on %s: couldn't find the weapon '%s' or the hands mesh '%s'."),
			*Owner->GetName(), *WeaponComponentName.ToString(), *HandsMeshName.ToString());
		return;
	}

	AddTickPrerequisiteComponent(HandsComponent);
	if (UHipRootMotionComponent* HipMotion = Owner->FindComponentByClass<UHipRootMotionComponent>())
	{
		// It moves the mesh after animation; follow the hands after that.
		AddTickPrerequisiteComponent(HipMotion);
	}

	// The spear's longest local axis is its shaft.
	const FBox Box = WeaponComponent->GetStaticMesh()->GetBoundingBox();
	const FVector Extent = Box.GetExtent();
	const int32 Axis = Extent.X >= Extent.Y && Extent.X >= Extent.Z ? 0 : (Extent.Y >= Extent.Z ? 1 : 2);
	LocalAxis = FVector::ZeroVector;
	LocalAxis[Axis] = 1.f;
	LocalUp = FVector::ZeroVector;
	LocalUp[(Axis + 1) % 3] = 1.f;
	LocalCenter = Box.GetCenter();
	Length = 2.f * Extent[Axis] * FMath::Abs(WeaponComponent->GetComponentScale()[Axis]);

	// Until both hands first hold it, keep the spear where the Blueprint put it.
	HeldInFrontHand = WeaponComponent->GetComponentTransform().GetRelativeTransform(HandsComponent->GetSocketTransform(FrontHandBone));
	bHasHeld = true;
}

FVector USpearGripComponent::GripPoint(FName HandBone, FName KnuckleBone) const
{
	const USkeletalMeshComponent* HandsComponent = Hands.Get();
	const FVector Wrist = HandsComponent->GetSocketLocation(HandBone);
	if (HandsComponent->GetBoneIndex(KnuckleBone) == INDEX_NONE)
	{
		return Wrist;
	}
	// The shaft runs through the palm, a bit more than halfway from the wrist to the knuckles.
	return FMath::Lerp(Wrist, HandsComponent->GetSocketLocation(KnuckleBone), 0.6f);
}

void USpearGripComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UStaticMeshComponent* WeaponComponent = Weapon.Get();
	USkeletalMeshComponent* HandsComponent = Hands.Get();
	if (!WeaponComponent || !HandsComponent || Length <= 0.f)
	{
		return;
	}

	const FTransform FrontHand = HandsComponent->GetSocketTransform(FrontHandBone);
	const FVector Front = GripPoint(FrontHandBone, FrontKnuckleBone);
	const FVector Back = GripPoint(BackHandBone, BackKnuckleBone);
	const FVector Between = Front - Back;
	const float Spacing = Between.Size();

	FTransform SpearWorld;
	if (Spacing >= MinHandSpacing && Spacing <= MaxHandSpacing)
	{
		const FVector Dir = Between / Spacing;

		// Turn the spear around its shaft to follow the front hand (wrist -> knuckles, flattened against the shaft).
		FVector UpRef = HandsComponent->GetBoneIndex(FrontKnuckleBone) != INDEX_NONE
			? HandsComponent->GetSocketLocation(FrontKnuckleBone) - FrontHand.GetLocation()
			: FrontHand.GetRotation().GetUpVector();
		UpRef -= Dir * FVector::DotProduct(UpRef, Dir);
		if (!UpRef.Normalize())
		{
			UpRef = FMath::Abs(Dir.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		}

		const FVector ShaftAxis = bFlipSpear ? -LocalAxis : LocalAxis;
		const FQuat Target = FRotationMatrix::MakeFromXZ(Dir, UpRef).ToQuat();
		const FQuat Local = FRotationMatrix::MakeFromXZ(ShaftAxis, LocalUp).ToQuat();
		const FQuat Rotation = FQuat(Dir, FMath::DegreesToRadians(Roll)) * Target * Local.Inverse();

		// The front hand sits FrontHandPosition of the way along the spear from its back end.
		const FVector Scale = WeaponComponent->GetComponentScale();
		const FVector CenterWorld = Front + Dir * ((0.5f - FrontHandPosition) * Length);
		const FVector Location = CenterWorld - Rotation.RotateVector(LocalCenter * Scale);

		SpearWorld = FTransform(Rotation, Location, Scale);
		HeldInFrontHand = SpearWorld.GetRelativeTransform(FrontHand);
		bHasHeld = true;
	}
	else if (bHasHeld)
	{
		// One hand let go: keep the spear in the front hand the way it was last held.
		SpearWorld = HeldInFrontHand * FrontHand;
	}
	else
	{
		return;
	}

	WeaponComponent->SetWorldLocationAndRotation(SpearWorld.GetLocation(), SpearWorld.GetRotation());
}

bool USpearGripComponent::GetSpearSegment(FVector& OutBack, FVector& OutTip) const
{
	const UStaticMeshComponent* WeaponComponent = Weapon.Get();
	if (!WeaponComponent || Length <= 0.f)
	{
		return false;
	}

	const FTransform& Transform = WeaponComponent->GetComponentTransform();
	const FVector Center = Transform.TransformPosition(LocalCenter);
	const FVector Axis = Transform.TransformVectorNoScale(bFlipSpear ? -LocalAxis : LocalAxis);
	OutBack = Center - Axis * (0.5f * Length);
	OutTip = Center + Axis * (0.5f * Length);
	return true;
}
