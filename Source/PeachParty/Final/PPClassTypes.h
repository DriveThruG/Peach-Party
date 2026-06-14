#pragma once

#include "CoreMinimal.h"
#include "PPClassTypes.generated.h"

/** The four player classes. Stats only — no active abilities (5-day-project simple). */
UENUM(BlueprintType)
enum class EPPClass : uint8
{
	Sprayer		UMETA(DisplayName = "Sprayer"),   // area pressure: fast, weak, lots of ammo
	Punisher	UMETA(DisplayName = "Punisher"),  // close burst: slow, very wet, little ammo
	Engineer	UMETA(DisplayName = "Engineer"),  // objectives: captures/defuses much faster
	Runner		UMETA(DisplayName = "Runner")     // mobility: fast feet, fast refill, low output
};

/** Role in the final phase. Derived from the player's (fixed) team vs the attacking team. */
UENUM(BlueprintType)
enum class ETeamRole : uint8
{
	None		UMETA(DisplayName = "None"),
	Attacker	UMETA(DisplayName = "Attacker"),
	Defender	UMETA(DisplayName = "Defender")
};

/**
 * Pure stat block for a class. Server-authoritative defaults live in PPClass::GetDefaults; a class
 * change just swaps the whole struct, so adding a stat = one field here + one use site. (A DataTable
 * could replace GetDefaults later for designer tuning without code — same struct.)
 */
USTRUCT(BlueprintType)
struct FPPClassStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class") float MoveSpeed = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class") float FireInterval = 0.20f;     // s between shots (lower = faster)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class") float ProjectileSpeed = 3500.f; // ~ range
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class") float SpreadDeg = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class") int32 AmmoCapacity = 100;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class") float WetnessPerHit = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class") float CaptureSpeedMul = 1.f;     // Engineer high
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class") float RefillSpeedMul = 1.f;      // Runner high
};

namespace PPClass
{
	/** Server-authoritative tuned defaults per class. Rock-paper-scissors, differences meaningful not extreme. */
	inline FPPClassStats GetDefaults(EPPClass Class)
	{
		FPPClassStats S;
		switch (Class)
		{
		case EPPClass::Sprayer:  // fast fire, weak, big ammo, medium range
			S.MoveSpeed = 600.f; S.FireInterval = 0.08f; S.ProjectileSpeed = 3500.f; S.SpreadDeg = 2.5f;
			S.AmmoCapacity = 180; S.WetnessPerHit = 4.f;  S.CaptureSpeedMul = 1.0f; S.RefillSpeedMul = 1.0f; break;

		case EPPClass::Punisher: // slow fire, very wet, short range, low ammo
			S.MoveSpeed = 560.f; S.FireInterval = 0.35f; S.ProjectileSpeed = 2200.f; S.SpreadDeg = 4.0f;
			S.AmmoCapacity = 40;  S.WetnessPerHit = 22.f; S.CaptureSpeedMul = 1.0f; S.RefillSpeedMul = 1.0f; break;

		case EPPClass::Engineer: // medium gun, FAST objectives
			S.MoveSpeed = 580.f; S.FireInterval = 0.20f; S.ProjectileSpeed = 3000.f; S.SpreadDeg = 2.0f;
			S.AmmoCapacity = 90;  S.WetnessPerHit = 8.f;  S.CaptureSpeedMul = 2.2f; S.RefillSpeedMul = 1.0f; break;

		case EPPClass::Runner:   // fast feet, fast refill, low output, low ammo
			S.MoveSpeed = 820.f; S.FireInterval = 0.18f; S.ProjectileSpeed = 3000.f; S.SpreadDeg = 3.0f;
			S.AmmoCapacity = 40;  S.WetnessPerHit = 4.f;  S.CaptureSpeedMul = 1.0f; S.RefillSpeedMul = 2.5f; break;
		}
		return S;
	}
}
