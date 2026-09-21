// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include "ProsperitocracyBloodBlade.generated.h"

struct FProsperitocracyWeaponAmmo;

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
 * step with the pool, and it could not answer "a bigger pool costs more per swing" — which is what
 * makes a MagSize perk a real decision for this class rather than a free refill.
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
 * A gun's ammo is spent by its own fire path — one round per committed shot, on the Rate cadence. The
 * blade is not fired and has no cadence of its own: its combo's parts ARE its cadence, so the swing
 * asks this class to spend the cost and the animation decides when the next swing may come. And
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
	/** The blade's own tick: while it is out, the pool drains. Nothing else about a swing is timed here. */
	virtual void Tick(float DeltaSeconds) override;

private:
	/** This blade's slot store on its carrier — where the blood lives, the same store a magazine uses. */
	FProsperitocracyWeaponAmmo* GetBloodStore();

	/**
	 * The drain's fraction, carried between frames.
	 *
	 * The pool is spent in whole blood, and a drain of a few points a minute is not: without this, a
	 * frame's share would round away to nothing on every frame and the blade would never drain at all.
	 */
	float DrainRemainder = 0.0f;
};
