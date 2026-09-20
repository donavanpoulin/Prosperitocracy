// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "ProsperitocracyDamageReceiver.generated.h"

/**
 * FProsperitocracyDamageProfile
 *
 * ONE part's answer to a hit, for a line of ONE damage type (Design/damage.md). Every part of a target —
 * a player's weave, an enemy's body parts — answers with:
 *   - Armor 0-3: the pen-gate threshold, compared against an attacker's Penetration (1-4). Over it
 *     the line is full, level with it the line is halved, under it the line gets nothing at all.
 *     0 = unarmoured, so every pen over-pens it and takes full; 3 = the top of the scale, where only
 *     pen 4 gets full.
 *   - Resist %: what this part does to a line of THAT type. One number, never one per type: the answer
 *     is asked per line, so a part that carries no resistance of the type asked resists nothing.
 *
 * Both numbers are FINAL — read off the part's own GAS home through the one evaluator, after perks and
 * buffs — never the authored bases.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyDamageProfile
{
	GENERATED_BODY()

	// The part's pen-gate threshold, on the 0-3 scale the Armor stat carries (Design/damage.md):
	// 0 = unarmoured — every pen over-pens it, so it always takes full; 1 = light; 2 = medium;
	// 3 = heavy, the top of the scale, where only pen 4 gets full and the rest are halved or stopped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	uint8 Armor = 0;

	// % less damage OF THE TYPE THAT WAS ASKED ABOUT. Negative = weakness. 0 = this part has nothing to
	// say about that type.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Resist = 0.0f;
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
	/**
	 * The answer of the PART a hit landed on, for a line of ONE damage type: its armour, and the
	 * resistance it has against that type. Both are FINAL values off the part's own GAS home.
	 *
	 * Asked per line, never once per hit: a hybrid hit carries a line per type, and a part answers each
	 * of them separately — one number per ask, so a Piercing line can never be answered with an Impact
	 * resistance.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Damage")
	FProsperitocracyDamageProfile GetDamageProfile(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const;

	virtual FProsperitocracyDamageProfile GetDamageProfile_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const;

	/**
	 * The target AS A WHOLE, for a line of ONE damage type — what a damage line that carries NO pen is
	 * answered by (2026-09-20).
	 *
	 * A line with a pen strikes a PART and is gated by that part's armour. A line with no pen (burn)
	 * does not penetrate anything: it reaches the target itself, so there is no part to ask and no gate
	 * to run. Armour is meaningless here — a line with no pen is never gated and can never bounce.
	 *
	 * The number is the resist of that ONE type, **averaged across the target's parts, all parts
	 * weighing the same** (the user's rule), taken as a FINAL value. A part that does not carry a
	 * resistance of that type counts as 0 in the average, because a row a part does not carry is not a
	 * row it has (presence is scope). One damage type per ask, so the answer is one number: a target
	 * can never answer a Piercing line with its Impact resistance. A player has one part (the weave),
	 * so their "average" is simply their own resist.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Damage")
	float GetBodyResist(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const;

	virtual float GetBodyResist_Implementation(const FGameplayEffectContextHandle& EffectContext, FGameplayTag DamageType) const;
};
