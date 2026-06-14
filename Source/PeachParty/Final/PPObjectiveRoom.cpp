#include "Final/PPObjectiveRoom.h"
#include "Core/PPGameState.h"
#include "Core/PPGameMode.h"
#include "Core/PPPlayerState.h"
#include "Core/PPDebug.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

APPObjectiveRoom::APPObjectiveRoom()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CaptureZone = CreateDefaultSubobject<UBoxComponent>(TEXT("CaptureZone"));
	CaptureZone->SetupAttachment(SceneRoot);
	CaptureZone->SetBoxExtent(FVector(400.f, 400.f, 200.f));
	CaptureZone->SetCollisionEnabled(ECollisionEnabled::NoCollision); // we use a distance check, not overlaps

	AttackerSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("AttackerSpawn"));
	AttackerSpawnPoint->SetupAttachment(SceneRoot);
	AttackerSpawnPoint->SetRelativeLocation(FVector(-800.f, 0.f, 100.f));

	DefenderSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DefenderSpawn"));
	DefenderSpawnPoint->SetupAttachment(SceneRoot);
	DefenderSpawnPoint->SetRelativeLocation(FVector(800.f, 0.f, 100.f));
}

void APPObjectiveRoom::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APPObjectiveRoom, bIsActive);
	DOREPLIFETIME(APPObjectiveRoom, bCaptured);
	DOREPLIFETIME(APPObjectiveRoom, CaptureProgress);
}

void APPObjectiveRoom::SetActive(bool bNewActive)
{
	if (HasAuthority())
	{
		bIsActive = bNewActive;
	}
}

FTransform APPObjectiveRoom::GetAttackerSpawn() const
{
	return AttackerSpawnPoint ? AttackerSpawnPoint->GetComponentTransform() : GetActorTransform();
}

FTransform APPObjectiveRoom::GetDefenderSpawn() const
{
	return DefenderSpawnPoint ? DefenderSpawnPoint->GetComponentTransform() : GetActorTransform();
}

void APPObjectiveRoom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// SERVER only, and only the active, not-yet-captured room progresses.
	if (!HasAuthority() || !bIsActive || bCaptured)
	{
		return;
	}

	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	const EPPTeam AttackTeam = GS->GetAttackingTeam();
	const FVector Center = GetActorLocation();
	const float RadiusSq = FMath::Square(CaptureRadius);

	float AttackerPower = 0.f; // sum of attacker capture multipliers in the zone
	int32 Defenders = 0;

	for (APlayerState* PS : GS->PlayerArray)
	{
		APPPlayerState* PPPS = Cast<APPPlayerState>(PS);
		if (!PPPS || PPPS->IsSlipping())
		{
			continue; // slipping players don't contest
		}
		const AController* C = PPPS->GetOwningController();
		const APawn* Pawn = C ? C->GetPawn() : nullptr;
		if (!Pawn || FVector::DistSquared(Pawn->GetActorLocation(), Center) > RadiusSq)
		{
			continue;
		}

		if (PPPS->GetTeam() == AttackTeam)
		{
			AttackerPower += PPPS->GetClassStats().CaptureSpeedMul; // Engineers add a lot
		}
		else
		{
			++Defenders;
		}
	}

	// Defenders contest -> stall/reverse (interrupt/defuse). Else attackers fill (faster with more / Engineers).
	if (Defenders > 0)
	{
		CaptureProgress = FMath::Max(0.f, CaptureProgress - DefuseRatePerSec * DeltaSeconds);
	}
	else if (AttackerPower > 0.f)
	{
		CaptureProgress = FMath::Min(1.f, CaptureProgress + CaptureRatePerSec * AttackerPower * DeltaSeconds);
		if (CaptureProgress >= 1.f)
		{
			bCaptured = true;
			bIsActive = false;
			OnRep_Captured(); // host mirror
			PPDebug::Print(FString::Printf(TEXT("ROOM %d CAPTURED!"), RoomIndex), FColor::Green, 5.f);
			if (APPGameMode* GM = GetWorld()->GetAuthGameMode<APPGameMode>())
			{
				GM->NotifyRoomCaptured(this);
			}
		}
	}

	// Live capture readout (keyed per room -> updates in place instead of spamming).
	PPDebug::Print(FString::Printf(TEXT("ROOM %d  capture %3.0f%%  (atk %.1f, def %d)"),
		RoomIndex, CaptureProgress * 100.f, AttackerPower, Defenders),
		FColor::White, 1.f, /*Key=*/100 + RoomIndex);
}

void APPObjectiveRoom::OnRep_Captured()
{
	BP_OnCaptured();
}
