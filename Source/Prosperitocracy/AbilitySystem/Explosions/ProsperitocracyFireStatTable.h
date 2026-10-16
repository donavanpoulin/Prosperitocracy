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
 * What is left here is only what a FIRE has that a gun does not: THE ONE EFFECT it is, how thickly its
 * flames stand on the ground, how deep they sit in the surface, how they go out, and the ceiling on how
 * much fire it may ask for.
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
	 * THE FIRE ITSELF — ONE effect for the whole fire, and not a list of flames to scatter.
	 *
	 * It is ONE because a fire is one fire: a fire laid as a field of separate little fires costs a
	 * light, a sim and a clock for every one of them, and a wider fire then means MORE FIRES rather
	 * than a bigger fire (2026-10-01, and the reason this file changed shape). This one effect is made of
	 * the pack's own parts — the flame, the ash, the haze, the ground card — and **it is told THE GROUND
	 * ITSELF**: one entry per piece of ground the fire is burning and which way each of those pieces
	 * faces, so its flames stand up off a floor, out of a wall and down a slope and none of them lies on
	 * one flat plane (his call, 2026-10-15: the fire takes the shape of what it burns on). The one number
	 * our code adds to that is how much fire the ground is asking for.
	 *
	 * It is a soft pointer so a fire block can be read without dragging the effect in with it, and the
	 * load happens where the fire is laid, out loud if it fails. The path it ships with is ours
	 * (`/Game/Abilities/NS_FireField`) and it is DATA: re-point it on this block and a fire burns with a
	 * different effect, with no code anywhere.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	TSoftObjectPtr<UNiagaraSystem> FireLook =
		TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Abilities/NS_FireField.NS_FireField")));

	/**
	 * [TUNE] MINE — how thickly the flames stand on the ground: flame cards a second, PER SQUARE METRE.
	 *
	 * This is the ONE density knob, and it is where a fire's count comes from: a wider circle asks for
	 * more flames off this number and nothing authors "how many". 53 is the number the effect was
	 * approved at — a 12 m circle (113 m²) asking for 6,000 cards a second, with each card living about
	 * four fifths of a second, so about 4,800 of them are alight at once.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float FlamesASquareMetreASecond = 53.0f;

	/**
	 * [TUNE] MINE — the MOST flames a second one fire may ask for, however wide its circle grows.
	 *
	 * Past it the fire does not grow thicker, it just covers more ground: the same ceiling his rule asks
	 * for, the look giving under load and never the fire — the circle, the Burn and the clock are the
	 * same either way.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	int32 MostFlamesASecond = 12000;

	/**
	 * [TUNE] MINE — HOW WIDE ONE PIECE OF THE FIRE BURNS, in cm across.
	 *
	 * A fire is a set of pieces of real ground, and this is the size of one of them: about a metre across,
	 * which is about TWICE a flame (a flame is 0.5 m, his number) and deliberately not the same thing. The
	 * flame is the look and nothing reads it; this is the little volume that burns you, and he is the one who
	 * called that difference out: *"one flames hitbox is maybe like a meter but the flame itself is maybe less
	 * than half that. so we cannot rely on the size of the single flame."*
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float HowWideOnePieceBurnsCm = 50.0f;

	/**
	 * [TUNE] MINE — HOW TALL ONE PIECE OF THE FIRE BURNS, in cm, standing OUT of the ground it is on.
	 *
	 * KNEE HIGH is his number for the fire's look, and it is the same number here: the flames stand out of the
	 * floor, out of a slope or out of a wall's face, and this is how far up off its own ground a piece of fire
	 * still reaches a body. A low wall therefore blocks low fire, which is real cover.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float HowTallOnePieceBurnsCm = 60.0f;

	/**
	 * [TUNE] MINE — HOW FAR APART THE FIRE'S PIECES ARE LAID, in cm — and therefore how far the fire walks in
	 * one step when it goes looking for the ground it burns.
	 *
	 * One metre, so that pieces a metre across cover their ground with no bare patches between them, and his
	 * own read of how packed the fire already looks is the answer nothing new had to be authored for: a 12 m
	 * fire is about 4,800 flames alight over its ground, a flame roughly every 15 cm (his call, 2026-10-15).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float HowFarApartItsPiecesAreSteppedCm = 100.0f;

	/**
	 * [TUNE] MINE — how deep in the surface the flame sits, in cm, along the surface's own normal.
	 *
	 * A fire's origin is not its middle: the effect hangs somewhere above the ground it means to burn,
	 * so the whole picture is pushed into the surface by this. His to move, and his to place once — the
	 * number cannot be read off the file, it is set by looking at one flame in the world.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float SinkCm = 5.0f;

	/**
	 * [TUNE] MINE — how long the fire takes to burn DOWN, in seconds, measured back from the END of the
	 * fire's Duration. INSIDE the Duration, never after it (his rule, 2026-09-30): the ground catches
	 * people for the whole of the fire's life, the burning down included, so this changes the look and
	 * nothing else.
	 *
	 * The burning down is the fire thinning ITSELF — one fire asking for less and less fire, its flames
	 * going out on their own as it goes — rather than a field of little fires stopping one at a time.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float FadeSeconds = 3.0f;
};
