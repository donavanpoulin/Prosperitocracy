// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Stats/ProsperitocracyStat.h"
#include "Templates/SubclassOf.h"

#include "ProsperitocracyStatTable.generated.h"

struct FProsperitocracyStatTableEntry;
// CUT 2026-09-16 (port of box 3.2): `class UProsperitocracyEquipmentDefinition;` stood here.
// The WeaponBody property below needed it, and UHT will not accept a forward declaration for a
// class used in a UPROPERTY — it fails the build with "Unable to find 'class' with name
// 'UProsperitocracyEquipmentDefinition'". Porting the real class pulls in UProsperitocracyAbilitySet
// and UProsperitocracyEquipmentInstance, i.e. the ability/equipment stack. The property is cut
// instead, and BOTH the class and the property come back with the weapons port (Part 5).

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
	 * The weapon's BODY — the equipment definition (WID) that carries the mesh actor, the
	 * per-body weapon instance (montages + anim sets), and the ONE universal ability set.
	 * A gun = this stat block; the body is shared by every gun on the same body. This is
	 * what makes "new gun = one stat block" true: the stat block knows everything about
	 * itself, including which body it rides on.
	 *
	 * CUT 2026-09-16 (port of box 3.2) — the property stood here in the old project:
	 *
	 *     UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	 *     TSoftClassPtr<UProsperitocracyEquipmentDefinition> WeaponBody;
	 *
	 * It is not a design change and nothing else read it yet: UHT rejects a forward-declared
	 * class in a UPROPERTY, and the real UProsperitocracyEquipmentDefinition pulls in
	 * UProsperitocracyAbilitySet + UProsperitocracyEquipmentInstance (the equipment stack).
	 * Restore this property, and the class declaration above it, with the weapons port (Part 5).
	 */

	/**
	 * Builds the canonical default entry list for the universal stat table — every stat-ID
	 * and its default base value. This is the single source of truth for the default table,
	 * mirroring Design/stats.md. A data asset can be populated from this (e.g. when authoring
	 * a design default).
	 */
	static void BuildCanonicalStats(TArray<FProsperitocracyStatTableEntry>& OutEntries);
};
