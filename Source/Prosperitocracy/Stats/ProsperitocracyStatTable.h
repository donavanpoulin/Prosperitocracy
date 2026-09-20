// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Stats/ProsperitocracyStat.h"
#include "Templates/SubclassOf.h"

#include "ProsperitocracyStatTable.generated.h"

struct FProsperitocracyStatTableEntry;

/**
 * FProsperitocracyStatTableEntry
 *
 * One row of the universal stat table: the stat-ID, its default base value, and
 * whether perks can reach it. Presence-is-scope: a perk is (stat, delta); if a
 * stat is PERK-IMMUNE, no perk / item / attachment delta may target it.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyStatTableEntry
{
	GENERATED_BODY()

	// The stat-ID (the vocabulary).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
	EProsperitocracyStat Stat = EProsperitocracyStat::Health;

	// Default base value for this stat. A concrete thing (weapon/ability/etc.) holds its own
	// base; this is the canonical default used when nothing overrides it. [TUNE]
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
	float BaseValue = 0.0f;
};

/**
 * FProsperitocracyAppliedEffect
 *
 * ONE STATUS a thing puts on what it hits — the pairing that keeps two durations from being one
 * number (Design/abilities.md, user-locked 2026-09-20).
 *
 * A status is a THING from the universal stat table, so it has its OWN block with its OWN numbers:
 * Stun carries Duration only; Burn always carries Rate, Duration and Piercing Damage (and a
 * Penetration value like any damage). The thing that applies it only NAMES the status — it never
 * lends the status its own Duration row. That is what lets a thing have a Duration of its own (a
 * bubble that lasts 6 s) and still put a 2 s stun on someone: two blocks, two numbers, no collision.
 *
 * Presence is scope, and it is what the behaviour reads too, from one rule with no status vocabulary:
 * a block that carries Rate AND Piercing Damage ticks damage (Burn); a block that carries only
 * Duration just lasts (Stun).
 */
USTRUCT(BlueprintType)
struct FProsperitocracyAppliedEffect
{
	GENERATED_BODY()

	/**
	 * Which status this is — the tag the body is stamped with while the status lasts, and the tag the
	 * world reads (Status.Stun, Status.Burn). It is an identity, never a number: the movement gate asks
	 * for the tag, the UI colours by it, and it is what "the same status" means when one is reapplied.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Applied")
	FGameplayTag StatusTag;

	/** The status's OWN numbers, as their own block. The applier names it; the block owns the values. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Applied")
	TSoftObjectPtr<UProsperitocracyStatTable> StatBlock;
};

/**
 * UProsperitocracyStatTable
 *
 * The ONE universal stat table as data — the source of truth for every stat in
 * the game. Mirrors Design/stats.md. Nothing numeric lives outside this table; if
 * something must be modifiable and isn't here, the table grows (the only maintenance).
 */
UCLASS(BlueprintType)
class UProsperitocracyStatTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	TArray<FProsperitocracyStatTableEntry> StatEntries;

	/**
	 * The statuses a hit from this thing puts on what it hits, each as its OWN block (see
	 * FProsperitocracyAppliedEffect). Empty for a thing that applies none — presence is scope.
	 *
	 * Data, not code: a new gun picks its statuses in the editor and no firing code changes, because
	 * the one door every hit already goes through applies whatever is listed here.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Applied")
	TArray<FProsperitocracyAppliedEffect> AppliedEffects;


	/**
	 * The thing's fire mode — a tag, NOT a stat (Design/weapons.md): no number, no perks,
	 * the evaluator never touches it. A ranged weapon carries exactly one:
	 * Prosperitocracy.Weapon.FireMode.SemiAuto or .FullAuto. Anything without the tag
	 * (melee, etc.) isn't a gun — presence-is-scope.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	FGameplayTag FireMode;

	FGameplayTag GetFireMode() const { return FireMode; }

	/**
	 * The thing's SLOT — also a tag, not a stat, and for the same reason (Design/weapons.md): "It is
	 * tagged with one slot, and the tag IS the slot: it goes in that slot, period." One of
	 * Prosperitocracy.Weapon.Slot.Primary / .Secondary / .Special / .Grenade.
	 *
	 * It lives HERE, on the weapon, and not on the loadout entry that carries it: a weapon knows what
	 * it is wherever it is (in a loadout, picked up off the floor, on an enemy), and a slot can refuse
	 * a weapon whose tag is not that slot. A thing without the tag (a character baseline block) is not
	 * a weapon and lives in no slot.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	FGameplayTag Slot;

	FGameplayTag GetSlot() const { return Slot; }

	/**
	 * Builds the canonical default entry list for the universal stat table — every stat-ID
	 * and its default base value. This is the single source of truth for the default table,
	 * mirroring Design/stats.md. A data asset can be populated from this (e.g. when authoring
	 * a design default).
	 */
	static void BuildCanonicalStats(TArray<FProsperitocracyStatTableEntry>& OutEntries);

	/**
	 * This block's AUTHORED base for one stat, or 0 when the block does not carry it.
	 *
	 * Careful — this is the block's number, not an evaluated one. A thing that is in the world has a
	 * GAS home and its FINAL value must be read there (`AProsperitocracyWeapon::GetWeaponStat`); a
	 * block read is for the one question only a block can answer: what does a thing I carry weigh
	 * before anything has been dressed, i.e. before there is a host to ask.
	 */
	float GetBaseValue(EProsperitocracyStat Stat) const;

	/**
	 * Does this block CARRY that stat (presence is scope, asked on the block itself)?
	 *
	 * The difference this answers is not "is it zero" but "is it here at all": a block that carries no
	 * Rate is not a block with a Rate of 0, and the ONE rule a statuses' behaviour runs on turns on
	 * exactly that — a status block carrying Rate and Piercing Damage ticks damage, one carrying only
	 * Duration just lasts. Asked here so nobody has to infer presence from a value.
	 */
	bool Carries(EProsperitocracyStat Stat) const;
};
