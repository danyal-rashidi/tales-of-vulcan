#include "RomeExitGate.h"
#include "GameAudio.h"
#include "HealthComponent.h"
#include "VulcanBoss.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

ARomeExitGate::ARomeExitGate()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Portcullis = CreateDefaultSubobject<USceneComponent>(TEXT("Portcullis"));
	Portcullis->SetupAttachment(Root);

	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(Root);
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));

	Passage = CreateDefaultSubobject<UBoxComponent>(TEXT("Passage"));
	Passage->SetupAttachment(Root);
	Passage->SetCollisionProfileName(TEXT("Trigger"));
	Passage->SetGenerateOverlapEvents(false); // armed once the gate opens
}

void ARomeExitGate::BeginPlay()
{
	Super::BeginPlay();
	Build();

	// Opens when Vulcan dies (listening to his health from outside his code).
	for (TActorIterator<AVulcanBoss> It(GetWorld()); It; ++It)
	{
		if (UHealthComponent* Health = It->FindComponentByClass<UHealthComponent>())
		{
			Health->OnDeath.AddUniqueDynamic(this, &ARomeExitGate::HandleBossDeath);
		}
	}
	Passage->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARomeExitGate::HandlePassage);
}

void ARomeExitGate::Build()
{
	auto Load = [](const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); };
	UStaticMesh* Cube = Load(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = Load(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cone = Load(TEXT("/Engine/BasicShapes/Cone.Cone"));
	UStaticMesh* Block = Load(TEXT("/Game/ThirdPerson/Colosseum/World/SM_RomanBlock_A.SM_RomanBlock_A"));
	UMaterialInterface* Solid = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Rome/Materials/M_RomeSolid.M_RomeSolid"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Cube || !Cylinder || !Cone || !Solid)
	{
		return;
	}
	auto Paint = [this, Solid](const FLinearColor& Color, float Metallic, float Roughness)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Solid, this);
		M->SetVectorParameterValue(TEXT("Tint"), Color);
		M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		M->SetScalarParameterValue(TEXT("WindAmount"), 0.f);
		return M;
	};
	UMaterialInstanceDynamic* Iron = Paint(FLinearColor(0.035f, 0.032f, 0.03f), 0.85f, 0.55f);
	UMaterialInstanceDynamic* Dark = Paint(FLinearColor(0.f, 0.f, 0.f), 0.f, 1.f);

	// Size is the box the mesh is stretched into; Centre is relative to this actor.
	auto Part = [this](UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Centre, const FVector& Size, const FRotator& Rotation, UMaterialInterface* Material, bool bSolid)
	{
		const FBox Box = Mesh->GetBoundingBox();
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetStaticMesh(Mesh);
		Piece->SetupAttachment(Parent);
		Piece->SetRelativeScale3D(Size / Box.GetSize());
		Piece->SetRelativeRotation(Rotation);
		Piece->SetRelativeLocation(Centre - Rotation.RotateVector(Box.GetCenter() * (Size / Box.GetSize())));
		if (Material)
		{
			Piece->SetMaterial(0, Material);
		}
		Piece->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
		Piece->RegisterComponent();
		return Piece;
	};

	const float HalfW = Width * 0.5f;
	// Stone frame: two piers and a lintel, standing proud of the wall.
	if (Block)
	{
		Part(Block, Root, FVector(-30.f, -HalfW - 45.f, Height * 0.5f), FVector(90.f, 90.f, Height + 40.f), FRotator::ZeroRotator, nullptr, true);
		Part(Block, Root, FVector(-30.f, HalfW + 45.f, Height * 0.5f), FVector(90.f, 90.f, Height + 40.f), FRotator::ZeroRotator, nullptr, true);
		Part(Block, Root, FVector(-30.f, 0.f, Height + 60.f), FVector(100.f, Width + 200.f, 80.f), FRotator::ZeroRotator, nullptr, true);
	}
	// Darkness behind the bars, so the opening reads as a passage.
	Part(Cube, Root, FVector(14.f, 0.f, Height * 0.5f), FVector(4.f, Width + 20.f, Height + 20.f), FRotator::ZeroRotator, Dark, false);

	// The portcullis: vertical bars with spiked feet, three cross bars.
	for (float Y = -HalfW + 20.f; Y <= HalfW - 19.f; Y += 30.f)
	{
		Part(Cylinder, Portcullis, FVector(0.f, Y, Height * 0.5f + 10.f), FVector(6.f, 6.f, Height - 20.f), FRotator::ZeroRotator, Iron, false);
		Part(Cone, Portcullis, FVector(0.f, Y, 10.f), FVector(9.f, 9.f, 22.f), FRotator(180.f, 0.f, 0.f), Iron, false);
	}
	for (const float Z : { Height * 0.22f, Height * 0.55f, Height * 0.88f })
	{
		Part(Cube, Portcullis, FVector(-4.f, 0.f, Z), FVector(8.f, Width - 20.f, 10.f), FRotator::ZeroRotator, Iron, false);
	}

	Blocker->SetBoxExtent(FVector(15.f, HalfW, Height * 0.5f));
	Blocker->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
	Passage->SetBoxExtent(FVector(40.f, HalfW - 20.f, Height * 0.4f));
	Passage->SetRelativeLocation(FVector(25.f, 0.f, Height * 0.4f));
}

void ARomeExitGate::HandleBossDeath(AActor* Killer)
{
	// A beat after he falls, then the bars start to rise.
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { Open(); }), 3.f, false);
}

void ARomeExitGate::Open()
{
	if (bOpening)
	{
		return;
	}
	bOpening = true;
	GameAudio::Play(this, TEXT("Crack"), GetActorLocation() + FVector(0.f, 0.f, Height), 1.f, 0.6f, 6000.f);
}

void ARomeExitGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bOpening || Raised >= 1.f)
	{
		return;
	}
	Raised = FMath::Min(Raised + DeltaSeconds / FMath::Max(RaiseSeconds, 0.1f), 1.f);
	// Grinding up in small jolts, the way a heavy gate on a chain moves.
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Raised, 2.f);
	const float Jolt = 6.f * FMath::Sin(Raised * 40.f) * (1.f - Raised);
	Portcullis->SetRelativeLocation(FVector(0.f, 0.f, Eased * (Height - 30.f) + Jolt));
	if (Raised >= 0.7f && Blocker->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Passage->SetGenerateOverlapEvents(true);
		Passage->UpdateOverlaps();
	}
}

void ARomeExitGate::HandlePassage(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	if (bLeaving || Other != Player || !Controller)
	{
		return;
	}
	bLeaving = true;
	Controller->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.8f, FLinearColor::Black, false, true);
	TWeakObjectPtr<ARomeExitGate> WeakThis(this);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakThis, Player, Controller]()
	{
		ARomeExitGate* Self = WeakThis.Get();
		if (!Self || !IsValid(Player))
		{
			return;
		}
		const FRotator Facing(0.f, Self->GetActorRotation().Yaw + Self->ExitYaw, 0.f);
		Player->TeleportTo(Self->GetActorTransform().TransformPosition(Self->ExitLocation), Facing);
		Controller->SetControlRotation(Facing);
		Controller->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.2f, FLinearColor::Black, false, false);
		Self->bLeaving = false;
	}), 0.9f, false);
}
