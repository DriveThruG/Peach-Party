#include "Core/PPPlayerController.h"
#include "Interaction/PPInteractable.h"
#include "Interaction/PPPCStation.h"
#include "Core/PPPlayerState.h"
#include "Core/PPGameState.h"
#include "Minigame/PPMinigameBase.h"
#include "Net/UnrealNetwork.h"

void APPPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Camera state only matters to the local owner.
	DOREPLIFETIME_CONDITION(APPPlayerController, SeatedStation, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APPPlayerController, ServerViewTarget, COND_OwnerOnly);
}

void APPPlayerController::ServerRequestInteract_Implementation(AActor* TargetActor)
{
	// SERVER. Validate the request before trusting it.
	if (!TargetActor)
	{
		return;
	}

	IPPInteractable* Interactable = Cast<IPPInteractable>(TargetActor);
	if (!Interactable)
	{
		return;
	}

	// Distance gate: reject reach-across-the-map interaction attempts.
	if (const APawn* MyPawn = GetPawn())
	{
		const float DistSq = FVector::DistSquared(MyPawn->GetActorLocation(), TargetActor->GetActorLocation());
		const float MaxReach = 400.f; // generous; per-interactable CanInteract can be stricter
		if (DistSq > FMath::Square(MaxReach))
		{
			return;
		}
	}

	if (Interactable->CanInteract(this))
	{
		Interactable->ServerInteract(this);
	}
}

void APPPlayerController::ServerLeaveStation_Implementation()
{
	// SERVER. Only allow standing up during the Lobby — you can't bail mid-minigame.
	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	if (GS && GS->GetCurrentPhase() != EMatchPhase::Lobby)
	{
		return;
	}

	if (SeatedStation)
	{
		SeatedStation->ServerReleaseOccupant();
	}
}

void APPPlayerController::SetSeatedStation(APPPCStation* NewStation)
{
	if (!HasAuthority() || SeatedStation == NewStation)
	{
		return;
	}

	SeatedStation = NewStation;
	OnRep_View(); // listen-server host is also a client: mirror the camera path locally.
}

void APPPlayerController::SetServerViewTarget(AActor* NewTarget)
{
	if (!HasAuthority() || ServerViewTarget == NewTarget)
	{
		return;
	}

	ServerViewTarget = NewTarget;
	OnRep_View();
}

void APPPlayerController::ServerCycleSpectate_Implementation(int32 Dir)
{
	// SERVER. Not allowed while you're still playing your own match.
	const APPPlayerState* MyPS = GetPlayerState<APPPlayerState>();
	const APPMinigameBase* MyMatch = MyPS ? MyPS->GetCurrentMinigame() : nullptr;
	if (MyMatch && !MyMatch->IsFinished())
	{
		return; // can't peek mid-match
	}

	const APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	const TArray<APPMinigameBase*>& Matches = GS->GetActiveMinigames();
	if (Matches.Num() == 0)
	{
		SetServerViewTarget(nullptr); // nothing live -> back to our own PC view
		return;
	}

	// Step to the next live match to watch.
	for (int32 Tries = 0; Tries < Matches.Num(); ++Tries)
	{
		SpectateIndex = (SpectateIndex + Dir + Matches.Num()) % Matches.Num();
		if (APPMinigameBase* Match = Matches[SpectateIndex])
		{
			SetServerViewTarget(Match);
			return;
		}
	}

	SetServerViewTarget(nullptr);
}

void APPPlayerController::OnRep_View()
{
	RefreshViewTarget();
}

void APPPlayerController::RefreshViewTarget(float BlendTime)
{
	// Priority: active minigame / spectated match  >  seated PC  >  own pawn.
	AActor* Target = ServerViewTarget;
	if (!Target)
	{
		Target = SeatedStation; // APPPCStation is an AActor with a camera component
	}
	if (!Target)
	{
		Target = GetPawn();
	}

	if (Target)
	{
		SetViewTargetWithBlend(Target, BlendTime, EViewTargetBlendFunction::VTBlend_Cubic);
	}
}
