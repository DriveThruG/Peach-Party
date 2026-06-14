#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/PPTypes.h"
#include "PPGameMode.generated.h"

class APPGameState;
class APPObjectiveRoom;
class APPPlayerState;

/**
 * SERVER ONLY. The match brain for the Final-only build.
 *
 * Flow: 2 players join (PIE: Number of Players = 2, listen server) -> both auto-assigned Team A/B ->
 * phase ClassSelect (each picks a class via WBP_ClassSelect) -> once both have chosen, the Final
 * frontline fight starts: attackers capture 3 objective rooms in order, defenders hold the timer.
 * No menus / hosting — both players drop straight into the same world.
 */
UCLASS(Config=Game)
class PEACHPARTY_API APPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APPGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Pick a placed APPTeamPlayerStart matching the player's team; fall back to plain starts / spread. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	AActor* ChooseTeamStart(AController* Player) const;

	/** Called by APPPlayerController after a player confirms a class. Starts the fight when both have chosen. */
	void NotifyClassChosen();

	/** Called by an objective room when fully captured: lock it, shift the frontline, reset the timer. */
	void NotifyRoomCaptured(APPObjectiveRoom* Room);

	/** Called when a player slips (wetness 100): schedule their respawn at the current frontline spawn. */
	void ScheduleRespawn(AController* Controller);

	// ---- Rules (tunable) ----
	UPROPERTY(EditDefaultsOnly, Config, Category = "PeachParty|Rules")
	int32 MinPlayersToStart = 2;

	/** Which team attacks first (captures the rooms). The other defends. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "PeachParty|Rules")
	EPPTeam DefaultAttackingTeam = EPPTeam::TeamA;

	/** Attackers' time to take the current room (reset on each capture). Expiry = defenders win. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "PeachParty|Rules")
	float RoomTimeLimit = 90.f;

	UPROPERTY(EditDefaultsOnly, Config, Category = "PeachParty|Rules")
	float RespawnDelay = 4.f;

protected:
	// ---- Flow ----
	void EnterClassSelect();
	void AssignTeams();             // fills any teamless players (PostLogin's PickJoinTeam does the rest)
	EPPTeam PickJoinTeam() const;   // returns the smaller team (A on a tie) for a joining player
	bool AllPlayersChosen() const;  // every connected player has confirmed a class

	// ---- Final phase (frontline) ----
	void StartFinalPhase();
	void ActivateRoom(int32 RoomArrayIndex);   // opens room, repositions teams, (re)starts the timer
	void OnRoomTimeLimitReached();              // attackers ran out of time on the current room
	void EndFinalPhase(EPPTeam WinningTeam);
	FTransform GetRoleSpawnTransform(const APPPlayerState* PS) const;
	void RespawnNow(AController* Controller);

	APPGameState* GetPPGameState() const;

	FTimerHandle PhaseTimerHandle;

	/** Counts spawned players to spread fallback spawns when the level has no PlayerStarts. */
	int32 PlayerSpawnCounter = 0;

	/** The 3 objective rooms ordered by RoomIndex, and which one is active. */
	UPROPERTY()
	TArray<APPObjectiveRoom*> Rooms;

	int32 CurrentRoomArrayIndex = -1;
};
