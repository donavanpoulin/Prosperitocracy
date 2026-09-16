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
	// % — receiver-side, per type; weakness = negative
	ImpactResist		UMETA(DisplayName = "Impact Resist"),
	// % — receiver-side, per type; weakness = negative
	PiercingResist		UMETA(DisplayName = "Piercing Resist"),
};
