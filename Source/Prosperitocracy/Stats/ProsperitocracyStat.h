// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ProsperitocracyStat.generated.h"

/**
 * EProsperitocracyStat
 *
 * The universal stat vocabulary — THE one stat-ID list for the whole game.
 * Every numeric stat that can exist on a thing (weapon, ability, turret, mech,
 * deployable) or on a character is a value here. Nothing numeric lives outside
 * this vocabulary; if something must be modifiable and isn't here, the table grows.
 *
 * Source of truth: Design/stats.md (THE UNIVERSAL STAT TABLE).
 *
 * A thing has a stat or it doesn't — nothing has every stat.
 */
UENUM(BlueprintType)
enum class EProsperitocracyStat : uint8
{
	// --- Thing stats (abilities, weapons, turrets, mech, deployables) ---

	// blunt melee, explosions, grenades, push, mech stomp, vehicle ram, thrown objects (enemies, barrels)
	ImpactDamage		UMETA(DisplayName = "Impact Damage"),
	// sword, bullets, lasers, fire, turrets, drones, burn
	PiercingDamage		UMETA(DisplayName = "Piercing Damage"),
	// per sec — fire rate, turret fire rate, burn tick rate, attack speed
	Rate				UMETA(DisplayName = "Rate"),
	// s — weapon reload; on charge abilities: time between charges
	ReloadTime			UMETA(DisplayName = "Reload Time"),
	// weapon profile — gun/projectile dispersion; shown in UI (design: no perks target this)
	Accuracy			UMETA(DisplayName = "Accuracy"),
	// weapon profile — gun kick / spread growth; shown in UI (design: no perks target this)
	Recoil				UMETA(DisplayName = "Recoil"),
	// m — gun range, ability range, turret range, blast radius, bubble size, throw distance
	Range				UMETA(DisplayName = "Range"),
	// m — where damage reduction STARTS; with Range (where it hits 0) — a LINEAR ramp between.
	// Absent = no falloff. Applies to FINAL damage at hit time (guns + explosions, one formula).
	Falloff				UMETA(DisplayName = "Falloff"),
	// s — burns, bubbles, throw window, stuns, turret lifetime, perk procs
	Duration			UMETA(DisplayName = "Duration"),
	// s — abilities only
	Cooldown			UMETA(DisplayName = "Cooldown"),
	// count — max ammo / charges. One number, both meanings: magazines carried (guns),
	// grenades carried, and max charges (charge abilities). The sword's blood IS this too.
	Capacity			UMETA(DisplayName = "Capacity"),
	// rounds per magazine — per-gun, independent of Capacity (more mags never makes the
	// mag bigger). Spare pool = Capacity x MagSize.
	MagSize				UMETA(DisplayName = "Mag Size"),
	// count — bullets fired per shot (a shotgun's pellets, a launcher's rockets). Absent = 1
	// (a normal shot); a thing with the stat fires that many per committed shot. Shown in the
	// UI stat sheet. Design: NO perks target this (perk-immune, like Accuracy/Recoil).
	Pellets				UMETA(DisplayName = "Pellets"),
	// units — equipment; feeds the weight system
	Weight				UMETA(DisplayName = "Weight"),
	// tier 1–4 (light/medium/heavy/anti-tank) — a flag on EVERY Impact/Piercing damage value.
	Penetration			UMETA(DisplayName = "Penetration"),
	// % — the shot's shove spent as speed: how much it slows you while you are moving FORWARD, where
	// you are spending speed fighting it. A THING stat, never a character one: it is the gun that
	// pushes, and this is how hard. Derived by one fixed formula off the thing's own Weight and damage
	// (AProsperitocracyStatHostActor), never authored, so a heavier or harder-hitting thing shoves
	// harder and a perk on either moves it. Shown on the gun's sheet, base → final like any stat.
	Drag				UMETA(DisplayName = "Drag"),
	// % — the same shove read when it is helping you instead of fighting you: moving BACK along the
	// shot's line it carries you. Always HALF of Drag — one push, one formula, read at half.
	Carry				UMETA(DisplayName = "Carry"),

	// --- Character stats ---

	// universal — players, enemies, and vehicles all have it
	Health				UMETA(DisplayName = "Health"),
	// cm/s — the RUN speed. Walking is half of it, derived by one universal constant
	// (UProsperitocracyPlayerStatsComponent::WalkSpeedMultiplier) rather than carried as
	// a second stat: there is one speed number, and the walk is a fraction of it.
	MoveSpeed			UMETA(DisplayName = "Move Speed"),
	// cm/s — the upward velocity a jump launches with; the movement component's JumpZVelocity.
	// A velocity, NOT a height: the same jump is a different height number on a different
	// character, and the body is driven by the velocity, so the velocity is the stat.
	JumpVelocity		UMETA(DisplayName = "Jump Velocity"),
	// cm — how far a dive/dodge travels. Currently unread: dodge and slide travel is baked into
	// their animations' root motion (read off their montages, not from a number).
	DiveDistance		UMETA(DisplayName = "Dive Distance"),
	// max blood
	PackCapacity		UMETA(DisplayName = "Pack Capacity"),
	WeightCapacity		UMETA(DisplayName = "Weight Capacity"),
	// lbs — what this body CARRIES: the sum of everything on it (guns, armor, anything with a Weight).
	// A ROW, not a hidden total, because it is the number a weight perk has to be able to MOVE — and a
	// number nothing can address is a number no perk can touch (Design/stats.md: if something must be
	// modifiable and isn't in the table, the table grows). Its base is the sum, written where the sum
	// already happens; perks resolve on top of it through the one aggregator like every other stat.
	CarriedWeight		UMETA(DisplayName = "Carried Weight"),
	// % — receiver-side, per type; weakness = negative
	ImpactResist		UMETA(DisplayName = "Impact Resist"),
	// % — receiver-side, per type; weakness = negative
	PiercingResist		UMETA(DisplayName = "Piercing Resist"),

	// --- What a thing LOOKS like (Design/armor.md — the armor's trim) ---
	//
	// APPENDED, never inserted: an enum value is what a saved stat block stores, so a new row goes on
	// the END of this list or every block already authored would silently become a different stat.

	// The armor's trim colour: ONE ROW PER REGION the body carries (three of them), each holding the
	// whole colour as a single number — its RGB hex (0xRRGGBB). Never three (red / green / blue) rows
	// for one region. Nothing is lost: white 0xFFFFFF = 16,777,215 is below 2^24, and a 32-bit float
	// holds every integer below that exactly.
	//
	// The regions are numbered for the PLAYER and named by what the player SEES — 1 the collar, 2 the
	// shoulders and arms, 3 the legs — because the mesh's own slot labels name the ARTIST's chunks
	// ("head and hands", "torso", "legs") and do not match what those regions look like. The player's
	// words are what the UI and the dev command speak; the mesh's labels stay where they are, on the
	// mesh.
	//
	// 0 is black — a real colour a player can pick. A NEGATIVE means "nothing chosen", and the region
	// keeps the paint it ships with.
	//
	// A THING stat: the armor carries its trim on its own GAS home, beside its Weight, never on the
	// player. Carried, not aggregated — nothing reads a raw base for a look, and no perk addresses it
	// (a look is not a mechanic).
	TrimColor1			UMETA(DisplayName = "Trim 1"),
	TrimColor2			UMETA(DisplayName = "Trim 2"),
	TrimColor3			UMETA(DisplayName = "Trim 3"),

	// --- Receiver-side, enemy parts only ---

	// The pen-gate threshold a PART carries: 0-3. It is the number an attacker's Penetration (1-4) is
	// compared against — over it the line is full, level with it the line is halved, under it the line
	// gets nothing and the shot stops there (Design/damage.md).
	//
	// 0 = unarmoured: anything at all over-pens it, so every pen gets full. 3 = the top of the scale:
	// only pen 4 gets full against it.
	//
	// A THING stat, never a character one: players have no armour, they answer with their resists.
	// It lives on the part's OWN GAS home with the part's resists, so a perk moves it exactly like any
	// other stat and a part's armour is never authored on an actor.
	Armor				UMETA(DisplayName = "Armor"),

	// --- What a thing that drinks BLOOD costs to use (Design/classes/reclaimer.md) ---

	// pool per second — the slow bleed a thing takes out of the blood WHILE IT IS OUT. A rate, so the
	// pool is eaten smoothly rather than in units: nothing about it steps.
	//
	// A row and not a constant, for the same reason Accuracy and Pellets are rows: it is a number a
	// THING carries, and presence is scope — a thing with a drain bleeds, a thing without one does not.
	// No perks target it (as none target Accuracy), which is not what makes something a row.
	BloodDrain			UMETA(DisplayName = "Blood Drain"),
	// pool per USE — what one use of this thing takes out of the blood: one swing, one ability.
	//
	// Read by the thing being used, so a blade's own swing and each of its abilities carry their own
	// cost without anything branching on which is which. The pool itself is not a row: a magazine-size
	// number says how big it is (`MagSize`) and the amount left is runtime, exactly like a gun's ammo.
	BloodCost			UMETA(DisplayName = "Blood Cost"),
};
