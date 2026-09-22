// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility.h"

#include "ProsperitocracyGameplayAbility_Bash.generated.h"

class AProsperitocracyWeapon;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpecHandle;
struct FGameplayAbilityActivationInfo;
struct FGameplayEventData;
struct FHitResult;

/**
 * The bash's constants — universal, the same value for every gun, always.
 *
 * Like the aim's constants these are the shape a stat is poured into, not per-gun config: a gun
 * contributes exactly one number to its bash (its Weight) and these two constants, and nothing else.
 * All values [TUNE].
 */
namespace ProsperitocracyBashHandling
{
	/**
	 * Impact damage per pound of the gun's FINAL Weight — the ONE formula behind the bash's Impact
	 * Damage stat: a 4 lb pistol bashes for 80, a 10 lb rifle for 200.
	 *
	 * Weight is read through the ONE evaluator, so a Weight perk arrives here already applied. The
	 * number this produces is not a wound number worked out at the hit: it is written as the bash's
	 * own Impact Damage base, and perks modify it from there like any other stat.
	 */
	constexpr float DamagePerPound = 20.0f;

	// The SHAPE of the swing is not here: how thick a swing is has nothing to do with what is swinging,
	// so it is one constant for every melee in the project (ProsperitocracyMeleeHandling::SwingRadius,
	// Weapons/ProsperitocracyWeapon.h).
}

/**
 * UProsperitocracyGameplayAbility_Bash
 *
 * The gun bash — the melee every gun has — as an ABILITY with its own stat block.
 *
 * What that buys, and what this class must therefore never do twice:
 *
 *   - ITS OWN NUMBERS. The bash's block carries Impact Damage, Penetration and Range, so the one
 *     evaluator values them and perks reach them like any other stat. The only thing that comes off
 *     the gun is its Weight: 20 x that (one fixed formula) is pushed as this ability's Impact Damage
 *     BASE, and everything above the base — a perk, an item — resolves through the aggregator.
 *   - RANGE IS THE REACH. The swing reaches Range meters from the player's eye along the same aim
 *     the bullet flies. One base number, the same on every gun, and a Range perk extends the bash.
 *   - PENETRATION comes off the same block (1 — the lightest tier). It is not a code constant.
 *   - ONE PIPELINE. The hit travels the same damage effect and the same pen-gate -> resist execution
 *     as a shot. There is no melee-only damage path, and this ability owns no damage numbers beyond
 *     the base it derives.
 *
 * The swing itself is what used to live on the gun: from the eye, along the gun's own shot
 * direction (drift included), a sphere swept the reach — so a bash lands where the reticle points.
 */
UCLASS()
class UProsperitocracyGameplayAbility_Bash : public UProsperitocracyGameplayAbility
{
	GENERATED_BODY()

public:
	UProsperitocracyGameplayAbility_Bash();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/**
	 * Write the bash's Impact Damage BASE onto this ability's own GAS home: 20 x the gun's FINAL
	 * Weight, one formula, one constant. Done before a swing so the number is always current for the
	 * gun actually in hand.
	 */
	void PushDerivedDamageBase(const AProsperitocracyWeapon& Gun);

	/** One whiff's worth of damage: the bash's own lines, through the one shared execution. */
	void ApplyBashToHit(const FGameplayAbilitySpecHandle& Handle, const FGameplayAbilityActorInfo* ActorInfo, const FHitResult& Hit, const AProsperitocracyWeapon& Gun);

	/** Says the one reason setup is missing once, rather than on every swing. */
	bool bLoggedMissingSetup = false;
};
