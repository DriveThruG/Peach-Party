#include "Final/PPRefillStation.h"
#include "Core/PPCharacter.h"
#include "Core/PPPlayerState.h"
#include "Core/PPGameState.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

APPRefillStation::APPRefillStation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // cosmetic presence; refill logic is server-only

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded())
	{
		Mesh->SetStaticMesh(Cyl.Object);
		Mesh->SetWorldScale3D(FVector(1.2f, 1.2f, 1.6f));
	}
}

void APPRefillStation::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(RefillTimer, this, &APPRefillStation::RefillTick, RefillInterval, true);
	}
}

void APPRefillStation::RefillTick()
{
	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	const FVector Center = GetActorLocation();
	const float RadiusSq = FMath::Square(Radius);

	for (APlayerState* PS : GS->PlayerArray)
	{
		APPPlayerState* PPPS = Cast<APPPlayerState>(PS);
		if (!PPPS)
		{
			continue;
		}
		const AController* C = PPPS->GetOwningController();
		APPCharacter* Char = C ? Cast<APPCharacter>(C->GetPawn()) : nullptr;
		if (!Char || FVector::DistSquared(Char->GetActorLocation(), Center) > RadiusSq)
		{
			continue;
		}
		const int32 Amount = FMath::RoundToInt(AmmoPerInterval * PPPS->GetClassStats().RefillSpeedMul); // Runner faster
		Char->AddAmmo(Amount);
	}
}
