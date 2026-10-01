// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/ProsperitocracyStatTable.h"

#include "ProsperitocracyFireStatTable.generated.h"

class UNiagaraSystem;

/**
 * THE FIRE'S OWN BLOCK — a fire on the ground, as data (PLANS/incendiary-strike.md §4).
 *
 * IT IS A STAT TABLE LIKE EVERY OTHER THING, because a fire is a thing like every other thing:
 *   - its RANGE and its DURATION are its OWN rows (StatEntries) — the fire's own block, its own two
 *     numbers, read FINAL through a GAS home like every other number in this game;
 *   - the BURN it puts on what it catches is its own named status with its own block (AppliedEffects —
 *     the very same pairing a gun uses), so the fire never lends a status its own numbers and a fire's
 *     burn can be worse than the strike's without either number moving;
 *   - it carries no damage of its own. A fire hurts nothing: it applies the Burn, the same way a gun
 *     applies one (his rule, restated twice).
 *
 * What is left here is only what a FIRE has that a gun does not: the flames it is made of, how far
 * apart they lie, how deep they sit in the ground, how they go out, and the ceiling on how many a
 * single fire may lay.
 *
 * NAMING ONE IS HOW ANYTHING LEAVES FIRE: a thing's block points at the fire it leaves
 * (UProsperitocracyStatTable::Fire), so the strike today and a flamethrower or a barrel tomorrow are
 * all the same wiring — a pointer, and no code per thing.
 */
UCLASS(BlueprintType)
class UProsperitocracyFireStatTable : public UProsperitocracyStatTable
{
	GENERATED_BODY()

public:
	/**
	 * THE FLAMES — the fire's pictures, as its own list.
	 *
	 * A list and not one, because a fire of twenty-odd flames must not read as twenty-odd copies: the
	 * flames alternate down this list, spot by spot. They are Niagara systems rather than actors
	 * because that is what the packs ship (every floor fire in Free_Fire is a system, and there is no
	 * per-fire blueprint to point at) — and NOTHING reads them: the flames are the look, and the thing
	 * that catches a body is the circle below, never a mesh.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TArray<TSoftObjectPtr<UNiagaraSystem>> Flames;

	/**
	 * [TUNE] MINE — how far apart the flames are laid, in meters, before the ceiling thins them.
	 *
	 * This is the ONE density knob, and it is what a fire's count comes off: the flames lie on an even
	 * spread one to a cell this wide, so a wider fire lays more of them and nothing authors "how many".
	 * A floor fire is about 0.5 m across (his number, 2026-09-30), so this sits above that: the flames
	 * read as separate fires burning in the same patch of ground, never as one texture.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float SpacingMeters = 1.2f;

	/**
	 * [TUNE] MINE — how deep in the surface each flame sits, in cm, along the surface's own normal.
	 *
	 * A fire's origin is not its middle: the pack's system hangs somewhere above the ground it means to
	 * burn, so every flame is pushed into the surface by this. His to move, and his to place once — the
	 * number cannot be read off the file, it is set by looking at one flame in the world.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float SinkCm = 5.0f;

	/**
	 * [TUNE] MINE — how long the flames take to go out, in seconds, measured back from the END of the
	 * fire's Duration. INSIDE the Duration, never after it (his rule, 2026-09-30): the ground catches
	 * people for the whole of the fire's life, the going-out included, so this changes the look and
	 * nothing else.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float FadeSeconds = 3.0f;

	/** [TUNE] MINE — how long ONE flame takes to shrink away once its own moment comes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float FlameGoesOutSeconds = 0.4f;

	/**
	 * [TUNE] MINE — the MOST flames one fire may lay. Past it the same circle is covered thinner
	 * (a wider spread), because the look is what gives under load and never the fire: fewer flames
	 * still read as burning ground, while a flame taken away leaves bare ground that still burns you.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	int32 MostFlamesInOneFire = 200;
};
