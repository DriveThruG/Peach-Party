#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Minigame/PPMinigameTypes.h"
#include "Core/PPTypes.h"
#include "PPMinigameBase.generated.h"

class UCameraComponent;
class USceneComponent;
class APPPlayerState;

/**
 * One 1v1 match of one minigame, living in its own arena in the shared world. Server-authoritative.
 * This is THE unit of play: pairings are per-game (reshuffled between Basket and Artillery), so a
 * match is just "these two players, this game, this arena" — there is no multi-game container.
 *
 * Modularity: the GameMode round manager drives matches purely through StartMinigame/ForceResolve,
 * so a third game is one subclass.
 *
 * Self-contained on clients: the two players, both scores, the camera and the result all replicate,
 * so a spectator just points their view at this actor.
 *
 * Lifecycle (server): StartMinigame() arms a Duration timer -> AddScore()/ApplyDamage() during play
 * -> timer or early KO -> FinishWithResult(...) -> reports up to APPGameMode::NotifyMinigameFinished.
 */
UCLASS(Abstract)
class PEACHPARTY_API APPMinigameBase : public AActor
{
	GENERATED_BODY()

public:
	APPMinigameBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** SERVER. Bind the two players and begin play. */
	virtual void StartMinigame(APPPlayerState* InP1, APPPlayerState* InP2);

	/** SERVER. Decide a winner from the CURRENT state (natural or forced time-out). */
	virtual EMatchResult ForceResolve() const;

	/** SERVER. Global-timeout shortcut: resolve from current state and finish immediately. */
	void ForceFinish();

	/** SERVER. Gameplay awards points to one of the two players. */
	UFUNCTION(BlueprintCallable, Category = "PeachParty|Minigame")
	void AddScore(APPPlayerState* Scorer, int32 Delta = 1);

	/** SERVER. End now with an explicit result (e.g. an early KO). */
	UFUNCTION(BlueprintCallable, Category = "PeachParty|Minigame")
	void FinishWithResult(EMatchResult InResult);

	UFUNCTION(BlueprintPure, Category = "PeachParty|Minigame")
	EMinigameType GetMinigameType() const { return MinigameType; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Minigame")
	EMatchResult GetResult() const { return Result; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Minigame")
	bool IsFinished() const { return Result != EMatchResult::Undecided; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Minigame")
	APPPlayerState* GetPlayer1() const { return Player1; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Minigame")
	APPPlayerState* GetPlayer2() const { return Player2; }

	UFUNCTION(BlueprintPure, Category = "PeachParty|Minigame")
	bool HasPlayer(const APPPlayerState* PS) const { return PS && (PS == Player1 || PS == Player2); }

	/** The OTHER player in this match (nullptr if PS isn't in it). */
	APPPlayerState* GetOpponentOf(const APPPlayerState* PS) const;

	/** Team that won this match (None if draw/undecided). */
	EPPTeam GetWinningTeam() const;

	/** Server-only: which recycled arena slot this match occupies. Set by the GameMode matchmaker. */
	int32 ArenaSlotIndex = INDEX_NONE;

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Minigame")
	USceneComponent* SceneRoot;

	/** The camera a player/spectator sees while this match is their view target. */
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Minigame")
	UCameraComponent* GameCamera;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Minigame")
	EMinigameType MinigameType = EMinigameType::None;

	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Minigame")
	float Duration = 30.f;

	/** Slot scores. Subclasses may ignore these and override ForceResolve (e.g. by health). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Minigame")
	int32 Player1Score = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Minigame")
	int32 Player2Score = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Result, BlueprintReadOnly, Category = "PeachParty|Minigame")
	EMatchResult Result = EMatchResult::Undecided;

	/** Replicated so UI/spectators know who is playing here. Slot 1 = Team A, slot 2 = Team B. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Minigame")
	APPPlayerState* Player1 = nullptr;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Minigame")
	APPPlayerState* Player2 = nullptr;

	UFUNCTION()
	void OnRep_Result();

	void OnDurationElapsed();

	/** SERVER hooks for subclasses to spawn / tear down their playfield. */
	virtual void OnMinigameStarted() {}
	virtual void OnMinigameFinished() {}

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Minigame")
	void BP_OnMinigameStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "PeachParty|Minigame")
	void BP_OnMinigameFinished(EMatchResult FinalResult);

	FTimerHandle DurationTimer;
};
