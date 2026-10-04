#include "SceneryCollisionSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void USceneryCollisionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Scenery meshes that only decorate the background.
	static const FName NoCollisionMeshes[] = { TEXT("SM_Dunes") };

	for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
	{
		UStaticMeshComponent* MeshComponent = It->GetStaticMeshComponent();
		const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}

		for (const FName& Name : NoCollisionMeshes)
		{
			if (Mesh->GetFName() == Name)
			{
				MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				break;
			}
		}
	}
}
