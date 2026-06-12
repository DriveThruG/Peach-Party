#include "Core/PPPlayerController.h"
#include "Interaction/PPInteractable.h"
#include "Interaction/PPPCStation.h"
#include "Core/PPPlayerState.h"
#include "Core/PPGameState.h"
#include "Minigame/PPMinigameBase.h"
#include "Minigame/PPPeachBasketUMGGame.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void APPPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Camera state only matters to the local owner.
	DOREPLIFETIME_CONDITION(APPPlayerController, SeatedStation, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(APPPlayerController, ServerViewTarget, COND_OwnerOnly);
}

void APPPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (IsLocalController())
	{
		UpdateMinigameHud();
	}
}

AActor* APPPlayerController::GetViewedMinigame() const
{
	if (Cast<APPMinigameBase>(ServerViewTarget))
	{
		return ServerViewTarget;
	}
	if (const APPPlayerState* PS = GetPlayerState<APPPlayerState>())
	{
		return PS->GetCurrentMinigame();
	}
	return nullptr;
}

void APPPlayerController::UpdateMinigameHud()
{
	// Show the widget only while viewing a UMG-type minigame; remove it otherwise.
	const bool bWantHud = Cast<APPPeachBasketUMGGame>(GetViewedMinigame()) != nullptr;

	if (bWantHud && !MinigameHud)
	{
		if (UClass* WClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/PeachParty/UI/WBP_BasketGame.WBP_BasketGame_C")))
		{
			MinigameHud = CreateWidget<UUserWidget>(this, WClass);
			if (MinigameHud) { MinigameHud->AddToViewport(50); }
		}
	}
	else if (!bWantHud && MinigameHud)
	{
		MinigameHud->RemoveFromParent();
		MinigameHud = nullptr;
	}
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

void APPPlayerController::ServerMinigameInput_Implementation(FName Action, bool bPressed)
{
	// SERVER. Forward to the match this player is actually in; reject otherwise.
	APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	APPMinigameBase* Match = PS ? PS->GetCurrentMinigame() : nullptr;
	if (Match && !Match->IsFinished())
	{
		Match->HandleInput(PS, Action, bPressed);
	}
}

void APPPlayerController::ServerSelectClass_Implementation(EPPClass NewClass)
{
	if (APPPlayerState* PS = GetPlayerState<APPPlayerState>())
	{
		PS->SetSelectedClass(NewClass); // PlayerState gates it to the slipping/respawn window
	}
}

void APPPlayerController::OnRep_View()
{
	RefreshViewTarget();
	if (IsLocalController())
	{
		// Seated at a PC but NOT in a minigame yet -> fade to black and HOLD ("you're at your PC,
		// waiting"). Otherwise (entering a minigame, standing up, spectating) fade black-and-back.
		const bool bSeatedIdle = (SeatedStation != nullptr) && (ServerViewTarget == nullptr);
		PlayTransitionFade(0.25f, /*bHold=*/bSeatedIdle);
	}
}

void APPPlayerController::ServerSelectReward_Implementation(EPPReward Reward)
{
	APPGameState* GS = GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
	const APPPlayerState* PS = GetPlayerState<APPPlayerState>();
	if (!GS || !PS || GS->GetCurrentPhase() != EMatchPhase::Reward)
	{
		return; // only during the reward window
	}
	if (!GS->IsRewardEligible(PS->GetTeam()))
	{
		return; // only the team(s) that earned a reward
	}
	GS->SetTeamReward(PS->GetTeam(), Reward);
}

void APPPlayerController::PlayTransitionFade(float HalfSeconds, bool bHold)
{
	if (!PlayerCameraManager)
	{
		return;
	}
	// Always (re)start the fade-to-black so a hold can't get stuck if timers overlap.
	GetWorldTimerManager().ClearTimer(FadeTimer);
	PlayerCameraManager->StartCameraFade(0.f, 1.f, HalfSeconds, FLinearColor::Black, false, /*bHoldWhenFinished=*/true);

	if (!bHold)
	{
		GetWorldTimerManager().SetTimer(FadeTimer,
			FTimerDelegate::CreateUObject(this, &APPPlayerController::FadeBackIn, HalfSeconds), HalfSeconds, false);
	}
}

void APPPlayerController::FadeBackIn(float Seconds)
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, Seconds, FLinearColor::Black, false, false);
	}
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
