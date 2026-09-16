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
};
