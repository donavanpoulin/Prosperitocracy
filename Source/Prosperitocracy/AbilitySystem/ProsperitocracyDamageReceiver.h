// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "UObject/Interface.h"

#include "ProsperitocracyDamageReceiver.generated.h"

/**
 * FProsperitocracyDamageProfile
 *
 * Receiver-side damage data for ONE part (0.5). Every part of a target (player armor set,
 * enemy body part) carries:
 *   - Armor 1-4: the pen-gate threshold (pen >= armor penetrates; below bounces).
 *   - ImpactResist % / PiercingResist %: per-type efficiency. Negative = weakness.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyDamageProfile
{
	GENERATED_BODY()

	// Pen-gate threshold: 1 = light, 2 = medium, 3 = heavy, 4 = anti-tank.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	uint8 Armor = 1;

	// % less Impact damage taken. Negative = weakness.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float ImpactResist = 0.0f;

	// % less Piercing damage taken. Negative = weakness.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float PiercingResist = 0.0f;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UProsperitocracyDamageReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * IProsperitocracyDamageReceiver
 *
 * Implemented by anything that can TAKE damage, so the one damage pipeline asks the target
 * for the armor + resists of the part that was hit. Players resolve this from their armor set
 * (resists in the StatSet); enemies resolve it per body part. The execution uses the returned
 * profile to run the pen gate + resist multiplier.
 */
class PROSPERITOCRACY_API IProsperitocracyDamageReceiver
{
	GENERATED_BODY()

public:
	/** Returns the armor + resist profile for the part hit by the given effect context. */
	UFUNCTION(BlueprintNativeEvent, Category = "Damage")
	FProsperitocracyDamageProfile GetDamageProfile(const FGameplayEffectContextHandle& EffectContext) const;

	virtual FProsperitocracyDamageProfile GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext) const;

	/**
	 * The target AS A WHOLE (2026-09-20) — what a damage line that carries NO pen is answered by.
	 *
	 * A line with a pen strikes a PART and is gated by that part's armor. A line with no pen (burn)
	 * does not penetrate anything, it reaches the target itself, so there is no part to ask and no gate
	 * to run: the answer is this. Armor is meaningless here — return 0.
	 *
	 * The resists here are **averaged across the target's parts, all parts weighing the same** (the
	 * user's rule), and they are the FINAL values — after everything that modifies them — never the
	 * authored bases, so a resist perk or a temporary buff counts exactly as it would on any other
	 * read. A player has one part (the weave), so their "average" is simply their own resist.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Damage")
	FProsperitocracyDamageProfile GetBodyDamageProfile(const FGameplayEffectContextHandle& EffectContext) const;

	virtual FProsperitocracyDamageProfile GetBodyDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext) const;
};
