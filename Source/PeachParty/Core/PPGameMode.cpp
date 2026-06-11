#include "Core/PPGameMode.h"
#include "Core/PPGameState.h"
#include "Core/PPPlayerState.h"
#include "Core/PPPlayerController.h"
#include "Core/PPCharacter.h"
#include "Minigame/PPMinigameBase.h"
#include "Minigame/PPPeachBasketGame.h"
#include "Minigame/PPPeachArtilleryGame.h"
#include "Interaction/PPPCStation.h"
#include "Core/PPPlaceholderBlock.h"
#include "Final/PPObjectiveRoom.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
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

void APPGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bSpawnPlaceholderHub)
	{
		BuildPlaceholderHub();
	}
}

void APPGameMode::BuildPlaceholderHub()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Floor: a big flat block centred on the origin (scale carried by the spawn transform).
	const FTransform FloorXform(FRotator::ZeroRotator, FVector(0.f, 0.f, -25.f), FVector(60.f, 60.f, 0.5f));
	World->SpawnActor<APPPlaceholderBlock>(APPPlaceholderBlock::StaticClass(), FloorXform, P);

	// A row of PC stations to walk up to and ready up at. Seat side faces -X (toward spawn).
	for (int32 i = 0; i < NumPlaceholderStations; ++i)
	{
		const float Y = (i - (NumPlaceholderStations - 1) * 0.5f) * 220.f;
		const FVector Loc(450.f, Y, 0.f);
		World->SpawnActor<APPPCStation>(APPPCStation::StaticClass(), Loc, FRotator::ZeroRotator, P);
	}

	// Optional light — OFF by default. Levels usually have their own lighting; spawning one here
	// "competes" with it. Only enable for a genuinely empty level (bSpawnPlaceholderLight).
	if (bSpawnPlaceholderLight)
	{
		if (ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(
				ADirectionalLight::StaticClass(), FVector(0.f, 0.f, 2000.f), FRotator(-55.f, -60.f, 0.f), P))
		{
			Light->SetReplicates(true);
			if (Light->GetLightComponent())
			{
				Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
				Light->GetLightComponent()->SetIntensity(4.f);
			}
		}
	}
}

AActor* APPGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Prefer real PlayerStarts placed in the level.
	if (AActor* Existing = Super::ChoosePlayerStart_Implementation(Player))
	{
		return Existing;
	}

	// None in the level: spawn a spread-out placeholder start near the origin, facing the PCs (+X).
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const int32 Index = PlayerSpawnCounter++;
	const float Y = (Index - 3.5f) * 150.f; // spread up to 8 players along Y
	const FVector Loc(-250.f, Y, 110.f);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Loc, FRotator::ZeroRotator, P);
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
	RoundStartTeamAScore = GS->GetTeamScore(EPPTeam::TeamA); // snapshot to compute this round's winner
	RoundStartTeamBScore = GS->GetTeamScore(EPPTeam::TeamB);

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
	// Far above the hub so the minigame never overlaps the normal 3D map (the camera follows it there).
	const FVector ArenaOrigin(Slot * ArenaSpacing, 0.f, 100000.f);

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
	// Winner of THIS minigame = whoever gained more wins this round (for the result screen).
	if (APPGameState* GS = GetPPGameState())
	{
		const int32 DeltaA = GS->GetTeamScore(EPPTeam::TeamA) - RoundStartTeamAScore;
		const int32 DeltaB = GS->GetTeamScore(EPPTeam::TeamB) - RoundStartTeamBScore;
		GS->SetLastRoundWinner(DeltaA > DeltaB ? EPPTeam::TeamA : (DeltaB > DeltaA ? EPPTeam::TeamB : EPPTeam::None));
	}

	// Hold on the result screen for a beat, then move on.
	GetWorldTimerManager().SetTimer(RoundResultTimer, this, &APPGameMode::AfterRoundResult, RoundResultSeconds, false);
}

void APPGameMode::AfterRoundResult()
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

	// Both games are over: stand everyone up. Clearing each station's occupant turns its screen off
	// (OnRep_Occupant) and clears the player's SeatedStation -> their camera blends back to their
	// first-person pawn, so they're back in control in the 3D world.
	TArray<AActor*> Stations;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APPPCStation::StaticClass(), Stations);
	for (AActor* A : Stations)
	{
		if (APPPCStation* Station = Cast<APPPCStation>(A))
		{
			Station->ServerReleaseOccupant();
		}
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
	GS->ClearActiveMinigames();

	// Reward = the minigame winner attacks first (early-game advantage). Tie -> Team A attacks.
	const EPPTeam Attackers = (GS->GetTeamScore(EPPTeam::TeamB) > GS->GetTeamScore(EPPTeam::TeamA))
		? EPPTeam::TeamB : EPPTeam::TeamA;
	GS->SetAttackingTeam(Attackers);

	// Gather the 3 placed objective rooms, ordered by RoomIndex.
	Rooms.Reset();
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APPObjectiveRoom::StaticClass(), Found);
	for (AActor* A : Found)
	{
		if (APPObjectiveRoom* Room = Cast<APPObjectiveRoom>(A)) { Rooms.Add(Room); }
	}
	Rooms.Sort([](const APPObjectiveRoom& A, const APPObjectiveRoom& B) { return A.GetRoomIndex() < B.GetRoomIndex(); });

	if (Rooms.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PeachParty] Final phase: no APPObjectiveRoom placed in the level."));
		return;
	}

	CurrentRoomArrayIndex = -1;
	ActivateRoom(0); // open room 1 (also repositions both teams + starts the timer)

	UE_LOG(LogTemp, Log, TEXT("[PeachParty] Final phase. Attackers = Team %d."), (int32)Attackers);
}

void APPGameMode::ActivateRoom(int32 RoomArrayIndex)
{
	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}

	if (Rooms.IsValidIndex(CurrentRoomArrayIndex))
	{
		Rooms[CurrentRoomArrayIndex]->SetActive(false);
	}

	// Past the last room -> all captured -> attackers win.
	if (!Rooms.IsValidIndex(RoomArrayIndex))
	{
		EndFinalPhase(GS->GetAttackingTeam());
		return;
	}

	CurrentRoomArrayIndex = RoomArrayIndex;
	APPObjectiveRoom* Room = Rooms[RoomArrayIndex];
	Room->SetActive(true);
	GS->SetActiveRoomIndex(Room->GetRoomIndex());

	// Per-room timer reset/extended (renewed pressure each stage).
	GS->SetPhaseEndTime(GetWorld()->GetTimeSeconds() + RoomTimeLimit);
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &APPGameMode::OnRoomTimeLimitReached, RoomTimeLimit, false);

	// Reposition both teams at the new frontline (clean stage transition).
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (APPPlayerState* PPPS = Cast<APPPlayerState>(PS))
		{
			if (AController* C = PPPS->GetOwningController())
			{
				RespawnNow(C);
			}
		}
	}
}

void APPGameMode::NotifyRoomCaptured(APPObjectiveRoom* Room)
{
	if (!Room || !Rooms.IsValidIndex(CurrentRoomArrayIndex) || Rooms[CurrentRoomArrayIndex] != Room)
	{
		return; // only the active room counts
	}
	// Frontline shifts forward to the next room (timer resets inside ActivateRoom).
	ActivateRoom(CurrentRoomArrayIndex + 1);
}

void APPGameMode::OnRoomTimeLimitReached()
{
	// Attackers failed to take the current room in time -> defenders win.
	if (APPGameState* GS = GetPPGameState())
	{
		const EPPTeam Defenders = (GS->GetAttackingTeam() == EPPTeam::TeamA) ? EPPTeam::TeamB : EPPTeam::TeamA;
		EndFinalPhase(Defenders);
	}
}

void APPGameMode::EndFinalPhase(EPPTeam WinningTeam)
{
	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);
	for (APPObjectiveRoom* Room : Rooms)
	{
		if (Room) { Room->SetActive(false); }
	}
	if (APPGameState* GS = GetPPGameState())
	{
		GS->SetPhase(EMatchPhase::PostMatch);
		GS->SetPhaseEndTime(0.f);
	}
	UE_LOG(LogTemp, Log, TEXT("[PeachParty] Final phase over. Winner = Team %d."), (int32)WinningTeam);
}

FTransform APPGameMode::GetRoleSpawnTransform(const APPPlayerState* PS) const
{
	if (PS && Rooms.IsValidIndex(CurrentRoomArrayIndex))
	{
		const APPObjectiveRoom* Room = Rooms[CurrentRoomArrayIndex];
		const APPGameState* GS = GetPPGameState();
		const bool bAttacker = GS && PS->GetTeam() == GS->GetAttackingTeam();
		return bAttacker ? Room->GetAttackerSpawn() : Room->GetDefenderSpawn();
	}
	return FTransform(FVector(0.f, 0.f, 200.f));
}

void APPGameMode::ScheduleRespawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	FTimerHandle Handle;
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &APPGameMode::RespawnNow, Controller);
	GetWorldTimerManager().SetTimer(Handle, Del, RespawnDelay, false);
}

void APPGameMode::RespawnNow(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	APPPlayerState* PS = Controller->GetPlayerState<APPPlayerState>();
	if (PS)
	{
		PS->ResetForRespawn();
	}
	RestartPlayerAtTransform(Controller, GetRoleSpawnTransform(PS));
}

void APPGameMode::EndMatch()
{
	if (APPGameState* GS = GetPPGameState())
	{
		GS->SetPhase(EMatchPhase::PostMatch);
		GS->SetPhaseEndTime(0.f);
	}
}
