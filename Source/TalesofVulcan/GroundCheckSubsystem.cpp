#include "GroundCheckSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

bool UGroundCheckSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FParse::Param(FCommandLine::Get(), TEXT("GroundCheck")) && Super::ShouldCreateSubsystem(Outer);
}

void UGroundCheckSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.IsGameWorld())
	{
		InWorld.GetTimerManager().SetTimer(ReportTimer, this, &UGroundCheckSubsystem::Report, 4.f, false);
	}
}

void UGroundCheckSubsystem::Report()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Character = *It;
		const FVector Location = Character->GetActorLocation();
		const float CapsuleBottom = Location.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();

		UE_LOG(LogTemp, Warning, TEXT("[GroundCheck] %s at %s  capsule bottom z=%.1f  movement mode=%d"),
			*Character->GetName(), *Location.ToString(), CapsuleBottom, Move ? (int32)Move->MovementMode.GetValue() : -1);

		if (Move)
		{
			const FFindFloorResult& Floor = Move->CurrentFloor;
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck]   standing on: actor=%s component=%s floor z=%.1f gap=%.1f walkable=%d"),
				*GetNameSafe(Floor.HitResult.GetActor()), *GetNameSafe(Floor.HitResult.GetComponent()),
				Floor.HitResult.ImpactPoint.Z, Floor.FloorDist, Floor.bWalkableFloor ? 1 : 0);
		}

		// Every collision surface straight below, top to bottom.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundCheck), true, Character);
		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, Location, Location - FVector(0.f, 0.f, 1500.f),
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), Params);
		for (const FHitResult& Hit : Hits)
		{
			const UPrimitiveComponent* Prim = Hit.GetComponent();
			const AActor* HitActor = Hit.GetActor();
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck]   below: z=%.1f actor=%s (%s) component=%s visible=%d hiddenInGame=%d profile=%s"),
				Hit.ImpactPoint.Z, *GetNameSafe(HitActor), HitActor ? *HitActor->GetActorNameOrLabel() : TEXT("-"),
				*GetNameSafe(Prim), Prim && Prim->IsVisible() ? 1 : 0, Prim && Prim->bHiddenInGame ? 1 : 0,
				Prim ? *Prim->GetCollisionProfileName().ToString() : TEXT("-"));
		}

		// Lowest visible point of each visible part (skin, otter shapes, weapon...).
		TArray<UPrimitiveComponent*> Parts;
		Character->GetComponents(Parts);
		for (const UPrimitiveComponent* Part : Parts)
		{
			if (Part == Character->GetCapsuleComponent() || !Part->IsVisible() || Part->bHiddenInGame)
			{
				continue;
			}
			const float Lowest = Part->Bounds.Origin.Z - Part->Bounds.BoxExtent.Z;
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck]   part %s (%s) lowest z=%.1f  (%.1f above capsule bottom)"),
				*Part->GetName(), *Part->GetClass()->GetName(), Lowest, Lowest - CapsuleBottom);
		}

		// Feet of every skeletal mesh (bounds can be loose, bones are exact).
		TArray<USkeletalMeshComponent*> Meshes;
		Character->GetComponents(Meshes);
		for (const USkeletalMeshComponent* Mesh : Meshes)
		{
			float LowestBoneZ = TNumericLimits<float>::Max();
			FName LowestBone;
			for (int32 i = 0; i < Mesh->GetNumBones(); ++i)
			{
				const FName Bone = Mesh->GetBoneName(i);
				const float Z = Mesh->GetBoneLocation(Bone).Z;
				if (Z < LowestBoneZ)
				{
					LowestBoneZ = Z;
					LowestBone = Bone;
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck]   skeleton %s visible=%d lowest bone %s z=%.1f  (%.1f above capsule bottom)"),
				*Mesh->GetName(), Mesh->IsVisible() && !Mesh->bHiddenInGame ? 1 : 0, *LowestBone.ToString(), LowestBoneZ, LowestBoneZ - CapsuleBottom);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[GroundCheck] done"));
	FPlatformMisc::RequestExit(false);
}
