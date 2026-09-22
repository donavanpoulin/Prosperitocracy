// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include "ProsperitocracyBloodBlade.generated.h"

struct FProsperitocracyWeaponAmmo;
class UAnimInstance;
class UAnimMontage;

/**
 * The blade's cost, as two universal constants and no vocabulary.
 *
 * The blade has a pool and a cost, and the design's rule for a number that is not a row of its own is
 * ONE fixed formula with universal constants (README: every new thing is either a row or a fixed
 * derivation, never a second system). So both of the blade's costs are derived from the ONE number the
 * player reads — the pool, MagSize — and every one of them moves when that row does:
 *
 *   a swing takes a share of the pool, and standing with the blade out lets the pool drain over a
 *   window (the same shape as contested health's fade, which is the pool over its window).
 *
 * That is deliberate: a cost written as its own per-swing number would be a second number to keep in
 * step with the pool.
 *
 * All values [TUNE].
 */
namespace ProsperitocracyBloodHandling
{
	/** One swing's share of the pool: 5% means a full pool is twenty swings. */
	constexpr float SwingCostShareOfPool = 0.05f;

	/** A swing always costs at least this much, so a pool too small to share out still costs something. */
	constexpr int32 SwingCostFloor = 1;

	/** While the blade is OUT, the pool empties evenly over this many seconds. */
	constexpr float DrainWindowSeconds = 100.0f;
}

/**
 * AProsperitocracyBloodBlade
 *
 * The blade as a thing: a weapon whose ammo is its own pool and whose pool is always being spent.
 *
 * A gun's ammo is spent by its own fire path: one round per committed shot, on the gun's own Rate
 * cadence. The blade is not fired and has no cadence — the swing asks this class to spend the cost, and
 * nothing here refuses a swing for being too soon after the last one. And
 * nothing else empties a gun, while the blade empties while it is simply held — its whole design is
 * the pressure to keep killing (Design/classes/reclaimer.md).
 *
 * Nothing here is a second ammo system: the pool IS the slot's store on the carrier, the same store a
 * gun's magazine lives in, filled from this thing's own MagSize. A blade carries NO Capacity row, and
 * an absent Capacity is what its spare is worth — nothing — so the pool is the whole of the blood,
 * which is the ammo minus the spare. One pool, one store, one evaluator.
 */
UCLASS(BlueprintType)
class AProsperitocracyBloodBlade : public AProsperitocracyWeapon
{
	GENERATED_BODY()

public:
	/**
	 * What one swing takes out of the pool, as the player would read it: the pool's share, whole blood.
	 * Derived, never authored — move MagSize and this moves with it.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	int32 GetSwingCost() const;

	/**
	 * One swing's worth of blood, taken whether the swing hits anything or not — a miss is a waste,
	 * which is the class's own rule (Design/classes/reclaimer.md).
	 *
	 * False when the pool cannot cover the swing: the swing is refused rather than going into debt. The
	 * blade is never reloaded and never refilled by this class; refuelling on kills is the blood
	 * mechanic, and it is not built yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Blade")
	bool SpendSwingCost();

	/** The blood left in the pool, as the slot's store holds it. What the player is spending. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	int32 GetBloodLeft() const;

	/** The pool's full size — this blade's own MagSize, FINAL through its own GAS home. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	int32 GetBloodPoolSize() const;

	/**
	 * One swing, as the thing that swings it: the blood goes first (a swing costs whether it hits or
	 * not), then a ball is swept along what the player was looking at, and whatever it lands on takes
	 * this blade's damage through the ONE pipeline — the same effect and the same pen-gate -> resist
	 * execution a shot travels, with the blade's own block answering for its Piercing Damage and its
	 * Penetration.
	 *
	 * The ANIMATION is not here: what the player sees is the body's (the combo montage), and this is
	 * the numbers half of the same press.
	 *
	 * False when there is nothing to swing with — not the thing in the player's hand, no reach to sweep,
	 * or a pool that cannot cover the swing (refused outright, never going into debt).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Blade")
	bool Swing();

	/**
	 * What the combo montage must be played at: this blade's FINAL Rate over its own base Rate.
	 *
	 * Rate IS the attack speed the player reads, so the animation runs at the ratio of what the stat
	 * says to what it shipped as — move the one row and every part of the combo moves with it. 1.0
	 * until something moves the stat, and 1.0 rather than a guess when there is nothing to divide by.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	float GetComboPlayRate() const;

	/**
	 * The combo this blade carries: what the player sees when it swings.
	 *
	 * Its sections are the plan's own names — A, A rec, B, B Rec, C — and the RECOVERIES are the only
	 * windows a press may pass through: a press inside one takes the combo straight to the next attack,
	 * and a press anywhere else does nothing at all (it never stacks, and it never cancels a swing).
	 * Sections are asked for BY NAME and never by seconds, so moving Rate cannot break a window.
	 *
	 * Set on the blade's blueprint: the blade carries the combo, and the body plays it — the animation is
	 * never this class's own to invent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Blade")
	TObjectPtr<UAnimMontage> ComboMontage;

	/** Whether this blade is mid-swing right now — its combo is running. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	bool IsSwinging() const { return bSwinging; }

	/** Blood a second while the blade is out, derived from the pool: the pool over its window. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	float GetDrainPerSecond() const;

	/**
	 * Whether this blade is the thing in the player's hand right now.
	 *
	 * Asked rather than assumed: the rig decides which of a character's weapons is out, so "the blade
	 * is draining" means "the blade is the one you are holding" and nothing about being carried.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Blade")
	bool IsInHand() const;

protected:
	/**
	 * The blade's own tick: the pool drains while the blade is OUT, and the combo's own running is noticed
	 * here (the montage is its clock). Nothing about the BODY is ticked here — an attack's line, distance,
	 * facing and clock are the body's own state (see AProsperitocracyCharacter::BeginAttack).
	 */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * The blade takes itself out of the world's collision the moment it exists.
	 *
	 * It is LOOKED AT and never bumped into: its damage is its own sweep, which ignores what hangs off the
	 * swinger already. A collidable rod the length of an arm hanging off a body is not something the world
	 * should be resolving against — it swallows every shot (a trace leaves the eye and meets the thing in
	 * the hand first, so nothing ever reaches a target) and it shoves the body it is attached to, because
	 * the world keeps pushing the rod back out of whatever it is inside of.
	 */
	virtual void BeginPlay() override;

private:
	/** This blade's slot store on its carrier — where the blood lives, the same store a magazine uses. */
	FProsperitocracyWeaponAmmo* GetBloodStore();

	/** The body's anim instance, which is where the combo plays. Null when the blade is on no body yet. */
	UAnimInstance* GetBodyAnimInstance() const;

	/**
	 * The animation half of one press: start the combo at its first attack, or take a live combo from a
	 * recovery to the next attack. True when an attack is playing — which is exactly when the numbers are
	 * owed — and false when the press landed inside an attack, where nothing happens at all.
	 */
	bool PlayOrAdvanceCombo();

	/** One attack's damage: the ball swept along what the player was looking at, through the one pipeline. */
	void LandTheHit();

	/** True between an attack starting and its combo ending: whether this blade's combo is running. */
	bool bSwinging = false;

	/** Range in cm: the stat's own unit is meters and the body travels centimeters. One conversion, here. */
	float GetRangeCm() const;

	/**
	 * How long attack n lasts in the world: that attack's own piece of the combo — A, B or C, never a
	 * recovery — at the rate the player reads. Handed to the body with Range when the attack begins, so
	 * the body needs to know nothing about combos and this blade needs to know nothing about movement.
	 */
	float GetAttackSeconds(int32 AttackIndex) const;

	/**
	 * The numbers half of an attack, handed to the BODY: the line (what the player was looking at when they
	 * pressed, taken flat), the distance (Range) and the time (that attack's own piece of the combo). The
	 * body does the rest — travelling it, facing it, refusing the player's own movement, and ending it — and
	 * this blade never writes the body's speed, its velocity or its facing at all.
	 */
	void BeginTheBodyAttack(int32 AttackIndex);

	/**
	 * The drain's fraction, carried between frames.
	 *
	 * The pool is spent in whole blood, and a drain of a few points a minute is not: without this, a
	 * frame's share would round away to nothing on every frame and the blade would never drain at all.
	 */
	float DrainRemainder = 0.0f;
};
