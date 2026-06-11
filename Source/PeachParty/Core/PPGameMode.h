#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/PPTypes.h"
#include "Minigame/PPMinigameTypes.h"
#include "PPGameMode.generated.h"

class APPGameState;
class APPMinigameBase;
class APPPlayerState;

/**
 * SERVER ONLY. The match brain. Exists only on the listen-server host; clients never see it.
 *
 * Minigame phase = two rounds (Peach Basket, then Peach Artillery). Before EACH round, opponents
 * are reshuffled across teams (teams themselves stay fixed all match). Each 1v1 win = +1 team point.
 *
 * Odd headcount: the unmatched player on the bigger team waits/spectates; when the FIRST opposite-
 * team player finishes, those two play a bonus match of the same game (both results count).
 *
 * Reward/Final remain stubs (core-first): they set the phase + arm a timer.
 */
UCLASS()
class PEACHPARTY_API APPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APPGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Spread players out when the level has no PlayerStarts (placeholder hub). */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Called by PC stations whenever a player's ready flag flips during Lobby. */
	void NotifyReadyStateChanged();

	/** Called by a minigame match when it is decided. Scores it and drives the matchmaker. */
	void NotifyMinigameFinished(APPMinigameBase* Match, EMatchResult Result);

	// ---- Phase transitions (server authoritative) ----
	void EnterLobbyPhase();
	void StartMinigamePhase();
	void StartRewardPhase();
	void StartFinalPhase();
	void EndMatch();

	// ---- Rules ----
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	int32 MinPlayersToStart = 2;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	int32 MaxPlayers = 8;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	float LobbyCountdownSeconds = 3.f;

	/** Hard cap on the whole minigame phase; unfinished matches are force-resolved. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	float GlobalMinigameTimeLimit = 150.f;

	/** Distance between adjacent 1v1 arenas. Big enough that nothing overlaps. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	float ArenaSpacing = 100000.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	float RewardDurationSeconds = 8.f;

	/** Game played in round 1 / round 2. Default to the C++ classes; overridable in BP. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	TSubclassOf<APPMinigameBase> BasketGameClass;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	TSubclassOf<APPMinigameBase> ArtilleryGameClass;

	// ---- Placeholder test hub (built at runtime so the project is playable with no level art) ----
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Placeholder")
	bool bSpawnPlaceholderHub = true;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Placeholder")
	int32 NumPlaceholderStations = 4;

protected:
	void BuildPlaceholderHub();
	// ---- Lobby ----
	bool AreAllPlayersReady() const;
	void AssignTeams();
	void OnLobbyCountdownFinished();

	// ---- Minigame round matchmaker ----
	void StartRound(int32 RoundIndex);
	void SpawnMatch(APPPlayerState* TeamAPlayer, APPPlayerState* TeamBPlayer);
	void OnRoundComplete();
	void FinishMinigamePhase();
	void OnGlobalMinigameTimeout();

	int32 AllocateArenaSlot();
	void FreeArenaSlot(int32 Slot);

	TSubclassOf<APPMinigameBase> GameClassForRound(int32 RoundIndex) const;

	void OnRewardFinished();

	APPGameState* GetPPGameState() const;

	FTimerHandle PhaseTimerHandle;
	bool bLobbyCountdownActive = false;

	// ---- Server-side minigame-phase bookkeeping ----
	int32 CurrentRound = 0;					// 0 = Basket, 1 = Artillery
	int32 LiveMatchCount = 0;				// matches currently running this round
	APPPlayerState* WaitingPlayer = nullptr; // odd-one-out awaiting a bonus match (null = none)
	TArray<bool> ArenaSlotInUse;			// recycled arena slots keep world coords bounded
	bool bMinigamePhaseResolved = false;
	bool bTimedOut = false;

	/** Counts spawned players to spread them out across placeholder spawn points. */
	int32 PlayerSpawnCounter = 0;
};
