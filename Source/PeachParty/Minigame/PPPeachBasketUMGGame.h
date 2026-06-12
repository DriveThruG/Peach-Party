#pragma once

#include "CoreMinimal.h"
#include "Minigame/PPMinigameBase.h"
#include "Minigame/PPBasketUMGTypes.h"
#include "PPPeachBasketUMGGame.generated.h"

/**
 * UMG version of Peach Basket. ALL gameplay runs server-side in normalised 2D play-field coords (0..1)
 * — no PhysX, no 3D placement. The state (FPPBasketState) replicates; a UMG widget reads GetState() and
 * draws it. The PlayerController auto-spawns that widget while you view this match (player or spectator).
 *
 * Mechanics (per the design): one input (Primary). Press = both your peaches jump ALONG their current
 * lean (the pendulum, so timing the jump moves you sideways) + arms raise. Release = throw if holding,
 * arms lower. A throw auto-aims at the enemy hoop; the launch is scaled by how high the arms were
 * (quality) — full arms = hits regardless of distance, low arms = falls short. Hands grab/steal ALWAYS
 * (not gated by Primary); raising the arms just lifts the hands to intercept high balls. First to
 * TargetScore, else most points after Duration (a tie = both win, handled by the base ForceResolve).
 */
UCLASS()
class PEACHPARTY_API APPPeachBasketUMGGame : public APPMinigameBase
{
	GENERATED_BODY()

public:
	APPPeachBasketUMGGame();

	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleInput(APPPlayerState* Player, FName Action, bool bPressed) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The whole replicated state — bind your widget to this. */
	UFUNCTION(BlueprintPure, Category = "PeachParty|Basket")
	const FPPBasketState& GetState() const { return RepState; }

protected:
	virtual void OnMinigameStarted() override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PeachParty|Basket")
	FPPBasketState RepState;

	// ---- Tunables (normalised units; field is 0..1, Y: 0 = bottom, 1 = top) ----
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") int32 TargetScore = 5;
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float Gravity = 1.4f;
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float JumpImpulse = 0.6f;   // lower = lower jumps
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float MaxLean = 0.4f;       // radians; lower = less sideways drift
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float LeanFreq = 2.2f;      // pendulum speed
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float SlideFriction = 8.f;  // ground horizontal damping (higher = less slide)
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float AirDrag = 1.0f;       // always-on horizontal damping (curbs floaty drift)
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float ArmRaiseRate = 2.0f;  // 0..1 per second
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float ThrowFlightTime = 0.9f;
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float GrabRange = 0.075f;
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float ScoreRange = 0.06f;
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float GroundY = 0.18f;      // char rest height — set to your background's FLOOR line
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") float BallFloorY = 0.10f;   // ball rest height
	// Layout: match these to where your hoop images sit (normalised). A scores in the RIGHT hoop.
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") FVector2D HoopLeftPos = FVector2D(0.08, 0.62);
	UPROPERTY(EditDefaultsOnly, Category = "PeachParty|Basket") FVector2D HoopRightPos = FVector2D(0.92, 0.62);

private:
	// Server-only simulation state (parallel to RepState.Chars by index).
	TArray<FVector2D> CharVel;
	TArray<float>     CharPhase;   // pendulum phase offset
	FVector2D BallVel = FVector2D::ZeroVector;
	int32 BallHolder = -1;         // index into Chars, or -1 = free
	float ThrowCooldown = 0.f;
	float SimTime = 0.f;

	void SetupField();
	void ServerTick(float Dt);
	void TryGrabSteal();
	void TryScore();
	void ThrowFrom(int32 Index);
	void DoScore(EPPTeam ScoringTeam);
	void ResetPositions();

	FVector2D UpVec(float LeanRad) const { return FVector2D(FMath::Sin(LeanRad), FMath::Cos(LeanRad)); }
	FVector2D HandOf(int32 Index) const;
	bool IsGrounded(int32 Index) const;
	void CharsOfPlayer(const APPPlayerState* Player, int32& OutA, int32& OutB) const;
	EPPTeam TargetTeamHoopOwner(int32 Index) const; // which team this char scores for
};
