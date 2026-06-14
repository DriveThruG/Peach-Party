#include "Core/PPGameMode.h"
#include "Core/PPGameState.h"
#include "Core/PPPlayerState.h"
#include "Core/PPPlayerController.h"
#include "Core/PPCharacter.h"
#include "Core/PPTeamPlayerStart.h"
#include "Final/PPObjectiveRoom.h"
#include "Core/PPDebug.h"
#include "EngineUtils.h" // TActorIterator
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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
}

APPGameState* APPGameMode::GetPPGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
}

// ------------------------------------------------------------- join / teams ----

void APPGameMode::PostLogin(APlayerController* NewPlayer)
{
	// Balance the team on JOIN, BEFORE Super spawns the pawn, so the spawn uses the right team start.
	if (NewPlayer)
	{
		if (APPPlayerState* PS = NewPlayer->GetPlayerState<APPPlayerState>())
		{
			PS->SetTeam(PickJoinTeam());
		}
	}

	Super::PostLogin(NewPlayer);

	if (APPGameState* GS = GetPPGameState())
	{
		if (GS->GetCurrentPhase() == EMatchPhase::None)
		{
			EnterClassSelect();
		}
	}
}

EPPTeam APPGameMode::PickJoinTeam() const
{
	int32 A = 0, B = 0;
	if (const APPGameState* GS = GetPPGameState())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (const APPPlayerState* P = Cast<APPPlayerState>(PS))
			{
				if (P->GetTeam() == EPPTeam::TeamA) { ++A; }
				else if (P->GetTeam() == EPPTeam::TeamB) { ++B; }
			}
		}
	}
	return (A <= B) ? EPPTeam::TeamA : EPPTeam::TeamB; // join the smaller team (A on a tie)
}

void APPGameMode::AssignTeams()
{
	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (APPPlayerState* P = Cast<APPPlayerState>(PS))
		{
			if (P->GetTeam() == EPPTeam::None)
			{
				P->SetTeam(PickJoinTeam());
			}
		}
	}
}

void APPGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	// If a player leaves during class select, the start gate simply won't be met until 2 are present again.
}

// --------------------------------------------------------- spawn selection ----

AActor* APPGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* TeamStart = ChooseTeamStart(Player))
	{
		return TeamStart;
	}
	if (AActor* Existing = Super::ChoosePlayerStart_Implementation(Player))
	{
		return Existing;
	}

	// None in the level: spread-out fallback near the origin.
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const int32 Index = PlayerSpawnCounter++;
	const FVector Loc(-250.f, (Index - 0.5f) * 200.f, 110.f);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Loc, FRotator::ZeroRotator, P);
}

AActor* APPGameMode::ChooseTeamStart(AController* Player) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	EPPTeam Team = EPPTeam::None;
	if (Player)
	{
		if (const APPPlayerState* PS = Player->GetPlayerState<APPPlayerState>())
		{
			Team = PS->GetTeam();
		}
	}

	TArray<APPTeamPlayerStart*> All;
	TArray<APPTeamPlayerStart*> Matching;
	for (TActorIterator<APPTeamPlayerStart> It(World); It; ++It)
	{
		APPTeamPlayerStart* Start = *It;
		All.Add(Start);
		if (Start->Team == Team)
		{
			Matching.Add(Start);
		}
	}

	const TArray<APPTeamPlayerStart*>& Pool = (Matching.Num() > 0) ? Matching : All;
	if (Pool.Num() == 0)
	{
		return nullptr;
	}
	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}

// ------------------------------------------------------------- class select ----

void APPGameMode::EnterClassSelect()
{
	AssignTeams();
	if (APPGameState* GS = GetPPGameState())
	{
		GS->SetPhase(EMatchPhase::ClassSelect);
		GS->SetPhaseEndTime(0.f); // untimed: wait for both picks
	}
	PPDebug::Print(TEXT("PHASE  ClassSelect — press 1-4 in BOTH windows to pick a class"), FColor::Magenta, 6.f);
}

bool APPGameMode::AllPlayersChosen() const
{
	const APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return false;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		const APPPlayerState* P = Cast<APPPlayerState>(PS);
		if (!P || !P->HasChosenClass())
		{
			return false;
		}
	}
	return true;
}

void APPGameMode::NotifyClassChosen()
{
	APPGameState* GS = GetPPGameState();
	if (!GS || GS->GetCurrentPhase() != EMatchPhase::ClassSelect)
	{
		return;
	}
	if (GS->NumPlayers() < MinPlayersToStart || !AllPlayersChosen())
	{
		int32 Chosen = 0;
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (const APPPlayerState* P = Cast<APPPlayerState>(PS)) { Chosen += P->HasChosenClass() ? 1 : 0; }
		}
		PPDebug::Print(FString::Printf(TEXT("WAITING  %d/%d players ready (need %d)"),
			Chosen, GS->NumPlayers(), MinPlayersToStart), FColor::Magenta, 3.f);
		return; // still waiting for the other player / their pick
	}
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
	GS->SetAttackingTeam(DefaultAttackingTeam);

	// Gather the placed objective rooms, ordered by RoomIndex.
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
		PPDebug::Print(TEXT("ERROR  no APPObjectiveRoom placed — fight cannot progress!"), FColor::Red, 10.f);
		return;
	}

	CurrentRoomArrayIndex = -1;
	PPDebug::Print(FString::Printf(TEXT("FIGHT START — attackers Team %d, %d rooms, %.0fs/room"),
		(int32)DefaultAttackingTeam, Rooms.Num(), RoomTimeLimit), FColor::Green, 6.f);
	ActivateRoom(0); // open room 1 + start the timer
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
	PPDebug::Print(FString::Printf(TEXT("ROOM %d now ACTIVE — attackers go capture it"), Room->GetRoomIndex()),
		FColor::Green, 5.f);

	// Per-room timer reset (renewed pressure each stage).
	GS->SetPhaseEndTime(GetWorld()->GetTimeSeconds() + RoomTimeLimit);
	GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &APPGameMode::OnRoomTimeLimitReached, RoomTimeLimit, false);

	// No teleport: the rooms are spread through the map and players walk between them. We only switch
	// which room is contestable here; positions are left alone.
}

void APPGameMode::NotifyRoomCaptured(APPObjectiveRoom* Room)
{
	if (!Room || !Rooms.IsValidIndex(CurrentRoomArrayIndex) || Rooms[CurrentRoomArrayIndex] != Room)
	{
		return; // only the active room counts
	}
	ActivateRoom(CurrentRoomArrayIndex + 1); // frontline shifts forward (timer resets inside)
}

void APPGameMode::OnRoomTimeLimitReached()
{
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
		GS->SetMatchWinner(WinningTeam);
		GS->SetPhase(EMatchPhase::PostMatch);
		GS->SetPhaseEndTime(0.f);
	}
	PPDebug::Print(FString::Printf(TEXT("MATCH OVER — Team %d WINS!"), (int32)WinningTeam), FColor::Green, 20.f);
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
	if (APPPlayerState* PS = Controller->GetPlayerState<APPPlayerState>())
	{
		PS->ResetForRespawn();
	}
	// Destroy the slipped pawn, then respawn at the player's team start (ChoosePlayerStart) — no teleport
	// to the room; the player walks back into the fight.
	if (APawn* Old = Controller->GetPawn())
	{
		Controller->UnPossess();
		Old->Destroy();
	}
	RestartPlayer(Controller);

	const APPPlayerState* PS = Controller->GetPlayerState<APPPlayerState>();
	PPDebug::Print(FString::Printf(TEXT("RESPAWN  %s"), PS ? *PS->GetPlayerName() : TEXT("?")), FColor::Cyan, 3.f);
}
