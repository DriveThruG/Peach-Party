#include "Core/PPGameMode.h"
#include "Core/PPGameState.h"
#include "Core/PPPlayerState.h"
#include "Core/PPPlayerController.h"
#include "Core/PPCharacter.h"
#include "Minigame/PPMinigameBase.h"
#include "Minigame/PPPeachBasketGame.h"
#include "Minigame/PPPeachArtilleryGame.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"

APPGameMode::APPGameMode()
{
	GameStateClass = APPGameState::StaticClass();
	PlayerStateClass = APPPlayerState::StaticClass();
	PlayerControllerClass = APPPlayerController::StaticClass();
	DefaultPawnClass = APPCharacter::StaticClass();

	BasketGameClass = APPPeachBasketGame::StaticClass();
	ArtilleryGameClass = APPPeachArtilleryGame::StaticClass();
}

APPGameState* APPGameMode::GetPPGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
}

void APPGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (APPGameState* GS = GetPPGameState())
	{
		if (GS->GetCurrentPhase() == EMatchPhase::None)
		{
			EnterLobbyPhase();
		}
	}
}

void APPGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	NotifyReadyStateChanged();
}

// ---------------------------------------------------------------- Lobby ----

void APPGameMode::EnterLobbyPhase()
{
	if (APPGameState* GS = GetPPGameState())
	{
		GS->SetPhase(EMatchPhase::Lobby);
		GS->SetPhaseEndTime(0.f);
		GS->ClearActiveMinigames();
	}
	bLobbyCountdownActive = false;
}

void APPGameMode::NotifyReadyStateChanged()
{
	APPGameState* GS = GetPPGameState();
	if (!GS || GS->GetCurrentPhase() != EMatchPhase::Lobby)
	{
		return;
	}

	if (AreAllPlayersReady())
	{
		if (!bLobbyCountdownActive)
		{
			bLobbyCountdownActive = true;
			GS->SetPhaseEndTime(GetWorld()->GetTimeSeconds() + LobbyCountdownSeconds);
			GetWorldTimerManager().SetTimer(
				PhaseTimerHandle, this, &APPGameMode::OnLobbyCountdownFinished, LobbyCountdownSeconds, false);
		}
	}
	else if (bLobbyCountdownActive)
	{
		bLobbyCountdownActive = false;
		GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
		GS->SetPhaseEndTime(0.f);
	}
}

bool APPGameMode::AreAllPlayersReady() const
{
	const APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return false;
	}
	const int32 Players = GS->NumPlayers();
	return Players >= MinPlayersToStart && GS->NumReadyPlayers() == Players;
}

void APPGameMode::OnLobbyCountdownFinished()
{
	bLobbyCountdownActive = false;

	if (!AreAllPlayersReady())
	{
		EnterLobbyPhase();
		return;
	}

	AssignTeams();
	StartMinigamePhase();
}

void APPGameMode::AssignTeams()
{
	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}

	int32 Index = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		APPPlayerState* PPPS = Cast<APPPlayerState>(PS);
		if (!PPPS)
		{
			continue;
		}
		PPPS->SetTeam((Index % 2 == 0) ? EPPTeam::TeamA : EPPTeam::TeamB);
		++Index;
	}
}

// ------------------------------------------------------------ Minigame ----

void APPGameMode::StartMinigamePhase()
{
	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}

	GS->SetPhase(EMatchPhase::Minigame);
	bMinigamePhaseResolved = false;
	bTimedOut = false;
	LiveMatchCount = 0;
	WaitingPlayer = nullptr;
	ArenaSlotInUse.Init(false, FMath::Max(MaxPlayers, 2));

	// One global safety timer for the whole phase (both rounds) + replicated HUD countdown.
	GS->SetPhaseEndTime(GetWorld()->GetTimeSeconds() + GlobalMinigameTimeLimit);
	GetWorldTimerManager().SetTimer(
		PhaseTimerHandle, this, &APPGameMode::OnGlobalMinigameTimeout, GlobalMinigameTimeLimit, false);

	StartRound(0);
}

TSubclassOf<APPMinigameBase> APPGameMode::GameClassForRound(int32 RoundIndex) const
{
	return (RoundIndex == 0) ? BasketGameClass : ArtilleryGameClass;
}

void APPGameMode::StartRound(int32 RoundIndex)
{
	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}

	CurrentRound = RoundIndex;
	WaitingPlayer = nullptr;

	// Split by (fixed) team, then shuffle each so opponents are randomized this round.
	TArray<APPPlayerState*> TeamA;
	TArray<APPPlayerState*> TeamB;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (APPPlayerState* PPPS = Cast<APPPlayerState>(PS))
		{
			if (PPPS->GetTeam() == EPPTeam::TeamA) { TeamA.Add(PPPS); }
			else if (PPPS->GetTeam() == EPPTeam::TeamB) { TeamB.Add(PPPS); }
		}
	}

	auto Shuffle = [](TArray<APPPlayerState*>& Arr)
	{
		for (int32 i = Arr.Num() - 1; i > 0; --i)
		{
			const int32 j = FMath::RandRange(0, i);
			Arr.Swap(i, j);
		}
	};
	Shuffle(TeamA);
	Shuffle(TeamB);

	const int32 NumPairs = FMath::Min(TeamA.Num(), TeamB.Num());

	// No cross-team pairing possible (a team is empty) -> nothing to play; end the phase.
	if (NumPairs == 0)
	{
		FinishMinigamePhase();
		return;
	}

	for (int32 i = 0; i < NumPairs; ++i)
	{
		SpawnMatch(TeamA[i], TeamB[i]);
	}

	// At most one leftover with balanced teams; it waits for a bonus match (see NotifyMinigameFinished).
	if (TeamA.Num() > NumPairs)		{ WaitingPlayer = TeamA[NumPairs]; }
	else if (TeamB.Num() > NumPairs){ WaitingPlayer = TeamB[NumPairs]; }

	if (WaitingPlayer)
	{
		WaitingPlayer->SetCurrentMinigame(nullptr); // idle: at PC, may spectate while waiting
		if (APPPlayerController* PC = Cast<APPPlayerController>(WaitingPlayer->GetOwningController()))
		{
			PC->SetServerViewTarget(nullptr);
		}
	}
}

void APPGameMode::SpawnMatch(APPPlayerState* TeamAPlayer, APPPlayerState* TeamBPlayer)
{
	if (!TeamAPlayer || !TeamBPlayer)
	{
		return;
	}

	const int32 Slot = AllocateArenaSlot();
	const FVector ArenaOrigin(Slot * ArenaSpacing, 0.f, 0.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APPMinigameBase* Match = GetWorld()->SpawnActor<APPMinigameBase>(
		GameClassForRound(CurrentRound), ArenaOrigin, FRotator::ZeroRotator, Params);
	if (!Match)
	{
		FreeArenaSlot(Slot);
		return;
	}

	Match->ArenaSlotIndex = Slot;
	Match->StartMinigame(TeamAPlayer, TeamBPlayer); // slot 1 = Team A, slot 2 = Team B

	// Route both players: bind to this match + point their camera at it.
	auto Bind = [Match](APPPlayerState* PS)
	{
		PS->SetCurrentMinigame(Match);
		if (APPPlayerController* PC = Cast<APPPlayerController>(PS->GetOwningController()))
		{
			PC->SetServerViewTarget(Match);
		}
	};
	Bind(TeamAPlayer);
	Bind(TeamBPlayer);

	if (APPGameState* GS = GetPPGameState())
	{
		GS->AddActiveMinigame(Match);
	}
	++LiveMatchCount;
}

void APPGameMode::NotifyMinigameFinished(APPMinigameBase* Match, EMatchResult Result)
{
	if (bMinigamePhaseResolved || !Match)
	{
		return;
	}

	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}

	// Score: each 1v1 win = +1 to the winner's (fixed) team. Draw scores nobody.
	const EPPTeam Winner = Match->GetWinningTeam();
	if (Winner != EPPTeam::None)
	{
		GS->AddTeamScore(Winner, 1);
	}

	APPPlayerState* P1 = Match->GetPlayer1();
	APPPlayerState* P2 = Match->GetPlayer2();

	// Release both players (cleared back to their PC view; can now spectate).
	auto Release = [](APPPlayerState* PS)
	{
		if (!PS) return;
		PS->SetCurrentMinigame(nullptr);
		if (APPPlayerController* PC = Cast<APPPlayerController>(PS->GetOwningController()))
		{
			PC->SetServerViewTarget(nullptr);
		}
	};
	Release(P1);
	Release(P2);

	// Retire the finished match actor + free its arena.
	GS->RemoveActiveMinigame(Match);
	FreeArenaSlot(Match->ArenaSlotIndex);
	--LiveMatchCount;
	Match->Destroy();

	// Bonus match for the odd-one-out: pull the just-finished opposite-team player to face them.
	if (!bTimedOut && WaitingPlayer)
	{
		APPPlayerState* Puller = nullptr;
		if (P1 && P1->GetTeam() != WaitingPlayer->GetTeam())		{ Puller = P1; }
		else if (P2 && P2->GetTeam() != WaitingPlayer->GetTeam()){ Puller = P2; }

		if (Puller)
		{
			// Keep the A-then-B slot convention.
			if (WaitingPlayer->GetTeam() == EPPTeam::TeamA)	{ SpawnMatch(WaitingPlayer, Puller); }
			else											{ SpawnMatch(Puller, WaitingPlayer); }
			WaitingPlayer = nullptr;
		}
	}

	// Round ends when nothing is live and the bonus (if any) has been started + finished.
	if (!bTimedOut && LiveMatchCount == 0 && WaitingPlayer == nullptr)
	{
		OnRoundComplete();
	}
}

void APPGameMode::OnRoundComplete()
{
	if (CurrentRound == 0)
	{
		StartRound(1); // Basket done -> Artillery, reshuffled
	}
	else
	{
		FinishMinigamePhase();
	}
}

void APPGameMode::OnGlobalMinigameTimeout()
{
	if (bMinigamePhaseResolved)
	{
		return;
	}
	bTimedOut = true; // suppresses bonus matches + round advancement while we drain

	APPGameState* GS = GetPPGameState();
	if (GS)
	{
		// Copy: force-finishing mutates the live list.
		TArray<APPMinigameBase*> Snapshot = GS->GetActiveMinigames();
		for (APPMinigameBase* Match : Snapshot)
		{
			if (Match && !Match->IsFinished())
			{
				Match->ForceFinish(); // -> NotifyMinigameFinished (scores + cleans up, no advance)
			}
		}
	}

	FinishMinigamePhase();
}

void APPGameMode::FinishMinigamePhase()
{
	if (bMinigamePhaseResolved)
	{
		return;
	}
	bMinigamePhaseResolved = true;

	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

	if (APPGameState* GS = GetPPGameState())
	{
		GS->ClearActiveMinigames();
		UE_LOG(LogTemp, Log, TEXT("[PeachParty] Minigame phase done. Team A %d, Team B %d."),
			GS->GetTeamScore(EPPTeam::TeamA), GS->GetTeamScore(EPPTeam::TeamB));
	}

	StartRewardPhase();
}

int32 APPGameMode::AllocateArenaSlot()
{
	for (int32 i = 0; i < ArenaSlotInUse.Num(); ++i)
	{
		if (!ArenaSlotInUse[i])
		{
			ArenaSlotInUse[i] = true;
			return i;
		}
	}
	// Should not happen given sizing, but grow rather than fail.
	return ArenaSlotInUse.Add(true);
}

void APPGameMode::FreeArenaSlot(int32 Slot)
{
	if (ArenaSlotInUse.IsValidIndex(Slot))
	{
		ArenaSlotInUse[Slot] = false;
	}
}

// -------------------------------------------------------------- Reward ----

void APPGameMode::StartRewardPhase()
{
	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}

	GS->SetPhase(EMatchPhase::Reward);
	GS->SetPhaseEndTime(GetWorld()->GetTimeSeconds() + RewardDurationSeconds);

	// TODO(reward): grant the winning team (or both, on a draw) their Final-phase advantage.
	UE_LOG(LogTemp, Log, TEXT("[PeachParty] Reward phase."));

	GetWorldTimerManager().SetTimer(
		PhaseTimerHandle, this, &APPGameMode::OnRewardFinished, RewardDurationSeconds, false);
}

void APPGameMode::OnRewardFinished()
{
	StartFinalPhase();
}

// --------------------------------------------------------------- Final ----

void APPGameMode::StartFinalPhase()
{
	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}

	GS->SetPhase(EMatchPhase::Final);
	GS->SetPhaseEndTime(0.f);
	GS->ClearActiveMinigames();

	// TODO(final): switch pawns to first-person, grant weapons, start combat rules.
	UE_LOG(LogTemp, Log, TEXT("[PeachParty] Final (first-person) phase."));
}

void APPGameMode::EndMatch()
{
	if (APPGameState* GS = GetPPGameState())
	{
		GS->SetPhase(EMatchPhase::PostMatch);
		GS->SetPhaseEndTime(0.f);
	}
}
