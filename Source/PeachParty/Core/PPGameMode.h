#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/PPTypes.h"
#include "Minigame/PPMinigameTypes.h"
#include "PPGameMode.generated.h"

class APPGameState;
class APPMinigameBase;
class APPPlayerState;
class APPObjectiveRoom;

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
UCLASS(Config=Game)
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

	/** Pick a placed APPTeamPlayerStart matching the player's team (nullptr if none placed). */
	AActor* ChooseTeamStart(AController* Player) const;

	/** Called by PC stations whenever a player's ready flag flips during Lobby. */
	void NotifyReadyStateChanged();

	/** Called by a minigame match when it is decided. Scores it and drives the matchmaker. */
	void NotifyMinigameFinished(APPMinigameBase* Match, EMatchResult Result);

	/** Called by an objective room when fully captured: lock it, shift the frontline, reset the timer. */
	void NotifyRoomCaptured(APPObjectiveRoom* Room);

	/** Called when a player slips (wetness 100): schedule their respawn at the current frontline spawn. */
	void ScheduleRespawn(AController* Controller);

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
	float RewardDurationSeconds = 12.f; // window for the result + reward-pick UI

	/** Pause after each minigame to show the result screen. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	float RoundResultSeconds = 5.f;

	/** Final phase: time the attackers get per room (reset/extended on each capture). */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	float RoomTimeLimit = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	float RespawnDelay = 4.f;

	/** Game played in round 1 / round 2. Default to the C++ classes; overridable in BP. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	TSubclassOf<APPMinigameBase> BasketGameClass;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Rules")
	TSubclassOf<APPMinigameBase> ArtilleryGameClass;

	// ---- Placeholder test hub (built at runtime so the project is playable with no level art) ----
	// OFF: you place your OWN PC stations + team spawns in the level. Set True (or in DefaultGame.ini)
	// only if you want the runtime floor + stations back for quick testing.
	UPROPERTY(EditDefaultsOnly, Config, Category = "PeachParty|Placeholder")
	bool bSpawnPlaceholderHub = false;

	// Just ONE station for quick testing — the rest you place by hand in the level.
	UPROPERTY(EditDefaultsOnly, Config, Category = "PeachParty|Placeholder")
	int32 NumPlaceholderStations = 1;

	/** OFF: the level already has its own directional light, so spawning another triggers the
	 *  "Multiple directional lights are competing…" warning. Paper2D minigame sprites are unlit anyway.
	 *  Turn on only for a genuinely empty, lightless level. */
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Placeholder")
	bool bSpawnPlaceholderLight = false;

	// ---- Debug / fast iteration ----
	/** PIE shortcut: skip the whole lobby/walk-to-PC/ready dance and drop STRAIGHT into a Peach Basket
	 *  match as soon as >=2 players have joined. Set false for the normal flow. Great for visual tuning.
	 *  `config` so it can be flipped in Config/DefaultGame.ini without a Blueprint GameMode. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "PeachParty|Debug")
	bool bDebugSkipToBasket = false;

protected:
	void BuildPlaceholderHub();
	void TryDebugAutoStart(); // polls for enough players, then jumps to the minigame phase
	FTimerHandle DebugStartTimer;
	// ---- Lobby ----
	bool AreAllPlayersReady() const;
	void AssignTeams();                 // fills any teamless players (join-time PickJoinTeam does the rest)
	EPPTeam PickJoinTeam() const;       // returns the smaller team (A on a tie) for a joining player
	void OnLobbyCountdownFinished();

	// ---- Minigame round matchmaker ----
	void StartRound(int32 RoundIndex);
	void SpawnMatch(APPPlayerState* TeamAPlayer, APPPlayerState* TeamBPlayer);
	void OnRoundComplete();
	void AfterRoundResult(); // after the result-screen pause: next round or finish
	void FinishMinigamePhase();
	void OnGlobalMinigameTimeout();

	int32 AllocateArenaSlot();
	void FreeArenaSlot(int32 Slot);

	TSubclassOf<APPMinigameBase> GameClassForRound(int32 RoundIndex) const;

	void OnRewardFinished();

	// ---- Final phase (frontline) ----
	void ActivateRoom(int32 RoomArrayIndex);   // opens room, repositions teams, (re)starts the timer
	void OnRoomTimeLimitReached();              // attackers ran out of time on the current room
	void EndFinalPhase(EPPTeam WinningTeam);
	FTransform GetRoleSpawnTransform(const APPPlayerState* PS) const;
	void RespawnNow(AController* Controller);

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

	/** Final phase: the 3 objective rooms ordered by RoomIndex, and which one is active. */
	UPROPERTY()
	TArray<APPObjectiveRoom*> Rooms;

	int32 CurrentRoomArrayIndex = -1;

	/** Team scores snapshotted at the start of a round, to compute that round's winner. */
	int32 RoundStartTeamAScore = 0;
	int32 RoundStartTeamBScore = 0;
	FTimerHandle RoundResultTimer;
};
