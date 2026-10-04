#include "HipRootMotionComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UHipRootMotionComponent::UHipRootMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After animation, so this frame's hip position is known.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UHipRootMotionComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || Mesh->GetBoneIndex(PelvisBone) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("HipRootMotion on %s: no Mesh with a '%s' bone."), *GetNameSafe(GetOwner()), *PelvisBone.ToString());
		return;
	}

	AddTickPrerequisiteComponent(Mesh);
	MeshBaseLocation = Mesh->GetRelativeLocation();
	RestPelvis = Mesh->GetBoneLocation(PelvisBone, EBoneSpaces::ComponentSpace);
	bReady = true;
}

void UHipRootMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!bReady || !Anim)
	{
		return;
	}

	const FVector Pelvis = Mesh->GetBoneLocation(PelvisBone, EBoneSpaces::ComponentSpace);
	const UAnimMontage* Montage = Anim->GetCurrentActiveMontage();
	if (!Montage)
	{
		// Normal locomotion: remember where the hips rest and keep the mesh where the Blueprint put it.
		RestPelvis = Pelvis;
		LastTravel = FVector::ZeroVector;
		Mesh->SetRelativeLocation(MeshBaseLocation);
		return;
	}

	// How far the hips have travelled sideways since the montage started, in the character's own space.
	FVector Travel = Mesh->GetRelativeTransform().TransformVector(Pelvis - RestPelvis);
	Travel.Z = 0.f;

	// While the montage plays (not while it blends out), that travel moves the character for real.
	if (!Anim->Montage_GetIsStopped(Montage) && MoveScale > 0.f)
	{
		const FVector Step = Character->GetActorQuat().RotateVector(Travel - LastTravel) * MoveScale;
		Character->AddActorWorldOffset(Step, true);
	}
	LastTravel = Travel;

	// Keep the body over the capsule.
	Mesh->SetRelativeLocation(MeshBaseLocation - Travel);
}
