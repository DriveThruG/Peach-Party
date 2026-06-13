#include "Core/PPGameMode.h"
#include "Core/PPGameState.h"
#include "Core/PPPlayerState.h"
#include "Core/PPPlayerController.h"
#include "Core/PPCharacter.h"
#include "Minigame/PPMinigameBase.h"
#include "Minigame/PPPeachBasketUMGGame.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"

APPGameMode::APPGameMode()
{
	DefaultPawnClass        = APPCharacter::StaticClass();
	PlayerControllerClass   = APPPlayerController::StaticClass();
	PlayerStateClass        = APPPlayerState::StaticClass();
	GameStateClass          = APPGameState::StaticClass();

	BasketGameClass = APPPeachBasketUMGGame::StaticClass();
}

void APPGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Prefer the live-tunable Blueprint child if the user has made one (no rebuild needed to tune).
	if (UClass* BP = LoadClass<APPMinigameBase>(nullptr,
		TEXT("/Game/PeachParty/Blueprints/BP_PeachBasketUMG.BP_PeachBasketUMG_C"), nullptr, LOAD_Quiet | LOAD_NoWarn))
	{
		BasketGameClass = BP;
	}

	if (APPGameState* GS = GetPPGameState())
	{
		GS->SetPhase(EMatchPhase::WaitingForPlayers);
	}
}

void APPGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer)
	{
		if (APPPlayerState* PS = NewPlayer->GetPlayerState<APPPlayerState>())
		{
			if (PS->GetTeam() == EPPTeam::None)
			{
				PS->SetTeam(PickJoinTeam());
			}
		}
	}

	TryStartMatch();
}

void APPGameMode::Logout(AController* Exiting)
{
	// If a participant leaves, tear the match down; it restarts when the player count is met again.
	if (ActiveMatch)
	{
		EndCurrentMatch();
		if (APPGameState* GS = GetPPGameState())
		{
			GS->SetPhase(EMatchPhase::WaitingForPlayers);
		}
	}
	Super::Logout(Exiting);
}

EPPTeam APPGameMode::PickJoinTeam() const
{
	const APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return EPPTeam::TeamA;
	}
	int32 NumA = 0, NumB = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (const APPPlayerState* PPPS = Cast<APPPlayerState>(PS))
		{
			if (PPPS->GetTeam() == EPPTeam::TeamA) { ++NumA; }
			else if (PPPS->GetTeam() == EPPTeam::TeamB) { ++NumB; }
		}
	}
	return (NumA <= NumB) ? EPPTeam::TeamA : EPPTeam::TeamB;
}

void APPGameMode::TryStartMatch()
{
	if (ActiveMatch && !ActiveMatch->IsFinished())
	{
		return; // already playing
	}

	APPGameState* GS = GetPPGameState();
	if (!GS)
	{
		return;
	}

	if (bDebugSoloBasket)
	{
		if (GS->PlayerArray.Num() >= 1)
		{
			StartSoloBasketPreview();
		}
		return;
	}

	// Need one player on each team.
	APPPlayerState* PSA = nullptr;
	APPPlayerState* PSB = nullptr;
	for (APlayerState* PS : GS->PlayerArray)
	{
		APPPlayerState* PPPS = Cast<APPPlayerState>(PS);
		if (!PPPS) { continue; }
		if (!PSA && PPPS->GetTeam() == EPPTeam::TeamA) { PSA = PPPS; }
		else if (!PSB && PPPS->GetTeam() == EPPTeam::TeamB) { PSB = PPPS; }
	}

	if (PSA && PSB)
	{
		StartTwoPlayerMatch(PSA, PSB);
	}
}

APPMinigameBase* APPGameMode::SpawnBasket()
{
	// Far from the origin (where pawns sit) so the match's own camera frames empty space behind the
	// full-screen UMG widget. bAlwaysRelevant keeps it visible to every client.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APPMinigameBase* Match = GetWorld()->SpawnActor<APPMinigameBase>(
		BasketGameClass, FVector(0.f, 0.f, 100000.f), FRotator::ZeroRotator, Params);

	if (Match)
	{
		if (APPGameState* GS = GetPPGameState())
		{
			GS->AddActiveMinigame(Match);
		}
		ActiveMatch = Match;
	}
	return Match;
}

void APPGameMode::StartTwoPlayerMatch(APPPlayerState* TeamAPlayer, APPPlayerState* TeamBPlayer)
{
	if (!TeamAPlayer || !TeamBPlayer)
	{
		return;
	}

	APPMinigameBase* Match = SpawnBasket();
	if (!Match)
	{
		return;
	}
	Match->StartMinigame(TeamAPlayer, TeamBPlayer); // slot 1 = Team A, slot 2 = Team B

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
		GS->SetPhase(EMatchPhase::Playing);
	}
}

void APPGameMode::StartSoloBasketPreview()
{
	APPGameState* GS = GetPPGameState();
	if (!GS || GS->PlayerArray.Num() == 0)
	{
		return;
	}
	APPPlayerState* Solo = Cast<APPPlayerState>(GS->PlayerArray[0]);
	if (!Solo)
	{
		return;
	}

	APPMinigameBase* Match = SpawnBasket();
	if (!Match)
	{
		return;
	}

	// Free-play: the lone player drives Team A (chars 0,1); chars 2,3 stand idle as targets. Same
	// player in both slots avoids a null Player2; bFreePlay disables the timer + match-end.
	if (APPPeachBasketUMGGame* Basket = Cast<APPPeachBasketUMGGame>(Match))
	{
		Basket->bFreePlay = true;
	}
	Match->StartMinigame(Solo, Solo);

	Solo->SetCurrentMinigame(Match);
	if (APPPlayerController* PC = Cast<APPPlayerController>(Solo->GetOwningController()))
	{
		PC->SetServerViewTarget(Match);
	}
	GS->SetPhase(EMatchPhase::Playing);
}

void APPGameMode::NotifyMinigameFinished(APPMinigameBase* Match, EMatchResult Result)
{
	if (!Match || Match != ActiveMatch)
	{
		return;
	}

	// Score the winning team (draw = no point).
	if (APPGameState* GS = GetPPGameState())
	{
		const EPPTeam Winner = Match->GetWinningTeam();
		if (Winner != EPPTeam::None)
		{
			GS->AddTeamScore(Winner, 1);
		}
	}

	// Pause, then restart a fresh match for the connected players.
	GetWorldTimerManager().SetTimer(RestartTimer, this, &APPGameMode::RestartMatch, RestartDelaySeconds, false);
}

void APPGameMode::EndCurrentMatch()
{
	if (!ActiveMatch)
	{
		return;
	}

	if (APPGameState* GS = GetPPGameState())
	{
		GS->RemoveActiveMinigame(ActiveMatch);
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (APPPlayerState* PPPS = Cast<APPPlayerState>(PS))
			{
				if (PPPS->GetCurrentMinigame() == ActiveMatch)
				{
					PPPS->SetCurrentMinigame(nullptr);
					if (APPPlayerController* PC = Cast<APPPlayerController>(PPPS->GetOwningController()))
					{
						PC->SetServerViewTarget(nullptr);
					}
				}
			}
		}
	}

	ActiveMatch->Destroy();
	ActiveMatch = nullptr;
}

void APPGameMode::RestartMatch()
{
	EndCurrentMatch();
	TryStartMatch();
}

APPGameState* APPGameMode::GetPPGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<APPGameState>() : nullptr;
}
