#include "ElvisAnimInstance.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "Retargeter/IKRetargeter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace ElvisCape
{
	// Chains left to right: L, M, R. A chain's sideways axis runs from its right neighbour to its left one.
	const TCHAR* ChainNames[3] = { TEXT("L"), TEXT("M"), TEXT("R") };
	const int32 LeftOf[3] = { 0, 0, 1 };
	const int32 RightOf[3] = { 1, 2, 2 };

	// How strongly each point is pulled back to its place on the back (the top point is pinned).
	// Small numbers let gravity win, so the cape hangs when he crouches or lies down.
	const float ShapeMemory[5] = { 1.f, 0.15f, 0.003f, 0.0008f, 0.0005f };

	const float SubStep = 1.f / 120.f;
	const int32 MaxSubSteps = 8;
	const float Damping = 0.985f;
	const float Gravity = -980.f;
	const float PointRadius = 3.f;
	const float FloorMargin = 5.f;
	const float TeleportDistance = 150.f;

	struct FCapsuleDef { const TCHAR* A; const TCHAR* B; float Radius; };
	const FCapsuleDef Body[] = {
		{ TEXT("Hips"), TEXT("Spine"), 17.f }, { TEXT("Spine"), TEXT("Spine1"), 13.f }, { TEXT("Spine1"), TEXT("Spine2"), 12.f },
		{ TEXT("Spine2"), TEXT("Neck"), 14.f }, { TEXT("Neck"), TEXT("Head"), 7.f },
	};
	const FCapsuleDef Limbs[] = {   // per side, after "Left" / "Right"
		{ TEXT("UpLeg"), TEXT("Leg"), 10.f }, { TEXT("Leg"), TEXT("Foot"), 7.f }, { TEXT("Foot"), TEXT("ToeBase"), 5.f },
		{ TEXT("Arm"), TEXT("ForeArm"), 5.5f }, { TEXT("ForeArm"), TEXT("Hand"), 5.f },
	};
	const float FlapRadius = 7.f;   // the leather flap hanging behind his legs
}

FElvisAnimInstanceProxy::FElvisAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_RetargetPoseFromMesh* InRetargetNode)
	: FAnimInstanceProxy(InAnimInstance)
	, RetargetNode(InRetargetNode)
{
}

void FElvisAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	SetupCape(InAnimInstance->GetSkelMeshComponent());
}

void FElvisAnimInstanceProxy::SetupCape(const USkeletalMeshComponent* Mesh)
{
	using namespace ElvisCape;
	bCapeReady = false;
	bSimStarted = false;
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		return;
	}

	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	TArray<FTransform> RefCS;
	FAnimationRuntime::FillUpComponentSpaceTransforms(Ref, Ref.GetRefBonePose(), RefCS);
	auto Find = [&Ref](const FString& Name) { return Ref.FindBoneIndex(FName(*Name)); };

	Spine2Bone = Find(TEXT("Spine2"));
	HipsBone = Find(TEXT("Hips"));
	if (Spine2Bone == INDEX_NONE || HipsBone == INDEX_NONE)
	{
		return;
	}

	FVector Rest[NumChains][NumPoints];
	for (int32 c = 0; c < NumChains; ++c)
	{
		for (int32 i = 0; i < NumPoints - 1; ++i)
		{
			CapeBones[c][i] = Find(FString::Printf(TEXT("Cape_%s_%02d"), ChainNames[c], i + 1));
			if (CapeBones[c][i] == INDEX_NONE)
			{
				return;   // not a mesh with a cape
			}
			Rest[c][i] = RefCS[CapeBones[c][i]].GetLocation();
		}
		// The last bone has no child: its tip continues the chain.
		Rest[c][NumPoints - 1] = Rest[c][NumPoints - 2] + (Rest[c][NumPoints - 2] - Rest[c][NumPoints - 3]);
	}

	const FTransform& Spine2Rest = RefCS[Spine2Bone];
	for (int32 c = 0; c < NumChains; ++c)
	{
		for (int32 i = 0; i < NumPoints; ++i)
		{
			RestInSpine2[c][i] = Spine2Rest.InverseTransformPosition(Rest[c][i]);
			if (c < NumChains - 1)
			{
				SideLength[c][i] = FVector::Dist(Rest[c][i], Rest[c + 1][i]);
			}
		}
		for (int32 i = 0; i < NumPoints - 1; ++i)
		{
			LinkLength[c][i] = FVector::Dist(Rest[c][i], Rest[c][i + 1]);
			FQuat Frame;
			if (!ChainFrame(Rest, c, i, Rest[c][i], Frame))
			{
				return;
			}
			RestFrameOffset[c][i] = Frame.Inverse() * RefCS[CapeBones[c][i]].GetRotation();
		}
	}

	Capsules.Reset();
	auto AddCapsule = [&](const FString& A, const FString& B, float Radius)
	{
		const int32 BoneA = Find(A), BoneB = Find(B);
		if (BoneA != INDEX_NONE && BoneB != INDEX_NONE)
		{
			Capsules.Add({ BoneA, BoneB, Radius });
		}
	};
	for (const FCapsuleDef& Def : Body)
	{
		AddCapsule(Def.A, Def.B, Def.Radius);
	}
	for (const TCHAR* Side : { TEXT("Left"), TEXT("Right") })
	{
		for (const FCapsuleDef& Def : Limbs)
		{
			AddCapsule(FString(Side) + Def.A, FString(Side) + Def.B, Def.Radius);
		}
	}

	// The flap: from the belt down to the knees, behind the hips.
	FVector Back = Rest[1][0] - Spine2Rest.GetLocation();
	Back.Z = 0.f;
	Back = Back.GetSafeNormal();
	const FTransform& HipsRest = RefCS[HipsBone];
	FlapInHips[0] = HipsRest.InverseTransformPosition(HipsRest.GetLocation() + Back * 10.f - FVector::UpVector * 4.f);
	FlapInHips[1] = HipsRest.InverseTransformPosition(HipsRest.GetLocation() + Back * 17.f - FVector::UpVector * 54.f);

	bCapeReady = true;
}

bool FElvisAnimInstanceProxy::ChainFrame(const FVector (&P)[NumChains][NumPoints], int32 Chain, int32 Index, const FVector& Head, FQuat& OutFrame) const
{
	using namespace ElvisCape;
	const FVector Side = (P[LeftOf[Chain]][Index] - P[RightOf[Chain]][Index]) + (P[LeftOf[Chain]][Index + 1] - P[RightOf[Chain]][Index + 1]);
	const FVector Along = P[Chain][Index + 1] - Head;
	if (FVector::CrossProduct(Along, Side).SizeSquared() < 1e-6f)
	{
		return false;
	}
	OutFrame = FRotationMatrix::MakeFromYX(Along, Side).ToQuat();
	return true;
}

void FElvisAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UElvisAnimInstance* Elvis = Cast<UElvisAnimInstance>(InAnimInstance);
	bSimulate = Elvis && Elvis->bSimulateCape;

	// The floor under him, so the cape can lie on it.
	const USkeletalMeshComponent* Mesh = InAnimInstance->GetSkelMeshComponent();
	const AActor* Owner = Mesh ? Mesh->GetOwner() : nullptr;
	UWorld* World = Mesh ? Mesh->GetWorld() : nullptr;
	FloorZ = Mesh ? Mesh->GetComponentLocation().Z : -UE_BIG_NUMBER;
	if (World && Owner && World->IsGameWorld())
	{
		const FVector Start = Owner->GetActorLocation();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ElvisCapeFloor), false, Owner);
		FloorZ = World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params)
			? Hit.ImpactPoint.Z : -UE_BIG_NUMBER;
	}
}

void FElvisAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	PendingDeltaTime += InContext.GetDeltaTime();
	FAnimInstanceProxy::UpdateAnimationNode(InContext);
}

bool FElvisAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	RetargetNode->Evaluate_AnyThread(Output);
	if (bSimulate && bCapeReady)
	{
		SimulateCape(Output);
	}
	return true;
}

void FElvisAnimInstanceProxy::SimulateCape(FPoseContext& Output)
{
	using namespace ElvisCape;
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	auto Compact = [&Bones](int32 MeshBone) { return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBone)); };
	const FCompactPoseBoneIndex Spine2C = Compact(Spine2Bone);
	const FCompactPoseBoneIndex HipsC = Compact(HipsBone);
	if (!Spine2C.IsValid() || !HipsC.IsValid())
	{
		return;
	}

	FCSPose<FCompactPose> CS;
	CS.InitPose(Output.Pose);
	const FTransform& ToWorld = GetComponentTransform();
	const FTransform Spine2CS = CS.GetComponentSpaceTransform(Spine2C);

	// Where the cape would be if it were stiff on his back, and where his body is.
	FVector Targets[NumChains][NumPoints];
	for (int32 c = 0; c < NumChains; ++c)
	{
		for (int32 i = 0; i < NumPoints; ++i)
		{
			Targets[c][i] = ToWorld.TransformPosition(Spine2CS.TransformPosition(RestInSpine2[c][i]));
		}
	}
	TArray<FVector> CapA, CapB;
	TArray<float> CapR;
	for (const FCapsule& Capsule : Capsules)
	{
		const FCompactPoseBoneIndex A = Compact(Capsule.BoneA), B = Compact(Capsule.BoneB);
		if (A.IsValid() && B.IsValid())
		{
			CapA.Add(ToWorld.TransformPosition(CS.GetComponentSpaceTransform(A).GetLocation()));
			CapB.Add(ToWorld.TransformPosition(CS.GetComponentSpaceTransform(B).GetLocation()));
			CapR.Add(Capsule.Radius);
		}
	}
	const FTransform HipsCS = CS.GetComponentSpaceTransform(HipsC);
	CapA.Add(ToWorld.TransformPosition(HipsCS.TransformPosition(FlapInHips[0])));
	CapB.Add(ToWorld.TransformPosition(HipsCS.TransformPosition(FlapInHips[1])));
	CapR.Add(FlapRadius);

	// First frame, or teleported (level start, respawn): start hanging in place.
	if (!bSimStarted || FVector::DistSquared(Targets[1][0], LastTargets[1][0]) > FMath::Square(TeleportDistance))
	{
		for (int32 c = 0; c < NumChains; ++c)
		{
			for (int32 i = 0; i < NumPoints; ++i)
			{
				Pos[c][i] = PrevPos[c][i] = LastTargets[c][i] = Targets[c][i];
			}
		}
		LastCapA = CapA;
		LastCapB = CapB;
		TimeAccumulator = 0.f;
		bSimStarted = true;
	}

	// Fixed 120 Hz steps, with his body moving smoothly from last frame to this one.
	TimeAccumulator = FMath::Min(TimeAccumulator + PendingDeltaTime, SubStep * MaxSubSteps);
	PendingDeltaTime = 0.f;
	const int32 NumSteps = FMath::FloorToInt(TimeAccumulator / SubStep);
	const bool bSameCapsules = LastCapA.Num() == CapA.Num();
	TArray<FVector> StepA = CapA, StepB = CapB;
	FVector StepTargets[NumChains][NumPoints];
	for (int32 s = 1; s <= NumSteps; ++s)
	{
		const float Alpha = float(s) / NumSteps;
		for (int32 c = 0; c < NumChains; ++c)
		{
			for (int32 i = 0; i < NumPoints; ++i)
			{
				StepTargets[c][i] = FMath::Lerp(LastTargets[c][i], Targets[c][i], Alpha);
			}
		}
		if (bSameCapsules)
		{
			for (int32 k = 0; k < CapA.Num(); ++k)
			{
				StepA[k] = FMath::Lerp(LastCapA[k], CapA[k], Alpha);
				StepB[k] = FMath::Lerp(LastCapB[k], CapB[k], Alpha);
			}
		}
		Step(StepTargets, StepA, StepB, CapR, SubStep);
	}
	TimeAccumulator -= NumSteps * SubStep;
	for (int32 c = 0; c < NumChains; ++c)
	{
		for (int32 i = 0; i < NumPoints; ++i)
		{
			LastTargets[c][i] = Targets[c][i];
		}
	}
	LastCapA = MoveTemp(CapA);
	LastCapB = MoveTemp(CapB);

	// Point each cape bone at the next simulated point, turned to match the cloth's width.
	FVector P[NumChains][NumPoints];
	for (int32 c = 0; c < NumChains; ++c)
	{
		for (int32 i = 0; i < NumPoints; ++i)
		{
			P[c][i] = ToWorld.InverseTransformPosition(Pos[c][i]);
		}
	}
	for (int32 c = 0; c < NumChains; ++c)
	{
		FTransform ParentCS = Spine2CS;
		for (int32 i = 0; i < NumPoints - 1; ++i)
		{
			const FCompactPoseBoneIndex BoneC = Compact(CapeBones[c][i]);
			if (!BoneC.IsValid())
			{
				break;
			}
			const FTransform Stiff = Output.Pose[BoneC] * ParentCS;
			FQuat Frame;
			if (!ChainFrame(P, c, i, Stiff.GetLocation(), Frame))
			{
				ParentCS = Stiff;
				continue;
			}
			FTransform NewCS(Frame * RestFrameOffset[c][i], Stiff.GetLocation(), Stiff.GetScale3D());
			NewCS.NormalizeRotation();
			Output.Pose[BoneC] = NewCS.GetRelativeTransform(ParentCS);
			ParentCS = NewCS;
		}
	}
}

void FElvisAnimInstanceProxy::Step(const FVector (&Targets)[NumChains][NumPoints], const TArray<FVector>& CapA, const TArray<FVector>& CapB, const TArray<float>& CapR, float Dt)
{
	using namespace ElvisCape;
	const FVector Fall(0.f, 0.f, Gravity * Dt * Dt);
	for (int32 c = 0; c < NumChains; ++c)
	{
		for (int32 i = 0; i < NumPoints; ++i)
		{
			const FVector Velocity = (Pos[c][i] - PrevPos[c][i]) * Damping;
			PrevPos[c][i] = Pos[c][i];
			Pos[c][i] += Velocity + Fall;
			Pos[c][i] += (Targets[c][i] - Pos[c][i]) * ShapeMemory[i];
		}
		Pos[c][0] = PrevPos[c][0] = Targets[c][0];
	}

	for (int32 Iteration = 0; Iteration < 4; ++Iteration)
	{
		// Keep each chain's links their length (the top point does not move).
		for (int32 i = 0; i < NumPoints - 1; ++i)
		{
			for (int32 c = 0; c < NumChains; ++c)
			{
				const FVector D = Pos[c][i + 1] - Pos[c][i];
				const float Length = D.Size();
				if (Length < KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const FVector Correction = D * ((Length - LinkLength[c][i]) / Length);
				if (i == 0)
				{
					Pos[c][1] -= Correction;
				}
				else
				{
					Pos[c][i] += Correction * 0.5f;
					Pos[c][i + 1] -= Correction * 0.5f;
				}
			}
		}
		// Keep the cape its width.
		for (int32 c = 0; c < NumChains - 1; ++c)
		{
			for (int32 i = 1; i < NumPoints; ++i)
			{
				const FVector D = Pos[c + 1][i] - Pos[c][i];
				const float Length = D.Size();
				if (Length < KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const FVector Correction = D * ((Length - SideLength[c][i]) / Length) * 0.5f;
				Pos[c][i] += Correction;
				Pos[c + 1][i] -= Correction;
			}
		}
		// Push out of his body and limbs, and keep above the floor.
		for (int32 c = 0; c < NumChains; ++c)
		{
			for (int32 i = 1; i < NumPoints; ++i)
			{
				FVector& X = Pos[c][i];
				float Deepest = 0.f;
				FVector Push = FVector::ZeroVector;
				for (int32 k = 0; k < CapA.Num(); ++k)
				{
					const FVector Closest = FMath::ClosestPointOnSegment(X, CapA[k], CapB[k]);
					const FVector Away = X - Closest;
					const float Distance = Away.Size();
					const float Depth = CapR[k] + PointRadius - Distance;
					if (Depth > Deepest && Distance > KINDA_SMALL_NUMBER)
					{
						Deepest = Depth;
						Push = Away / Distance * Depth;
					}
				}
				X += Push;
				X.Z = FMath::Max(X.Z, FloorZ + FloorMargin);
			}
			Pos[c][0] = Targets[c][0];
		}
	}
}

UElvisAnimInstance::UElvisAnimInstance()
	: Retargeter(FSoftObjectPath(TEXT("/Game/Player/Character/Elvis/RTG_Quinn_To_Elvis.RTG_Quinn_To_Elvis")))
{
}

FAnimInstanceProxy* UElvisAnimInstance::CreateAnimInstanceProxy()
{
	return new FElvisAnimInstanceProxy(this, &RetargetNode);
}

void UElvisAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UElvisAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	RetargetNode.IKRetargeterAsset = Retargeter.LoadSynchronous();
}
