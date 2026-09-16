// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Stats/ProsperitocracyStat.h"
#include "Templates/SubclassOf.h"

#include "ProsperitocracyStatTable.generated.h"

struct FProsperitocracyStatTableEntry;
class UProsperitocracyWeaponBodyData;
// RESTORED 2026-09-16. This declaration stood as `UProsperitocracyEquipmentDefinition` and was cut
// when box 3.2 landed, because that class drags in Lyra's equipment/ability stack (see the WeaponBody
// comment below). The weapons batch replaces that dependency rather than porting it: the body is now
// OUR UProsperitocracyWeaponBodyData (an asset holding the gun's mesh, anims, sounds and FX), so a
// gun still knows which body it rides on and nothing Lyra-shaped is pulled in to say it.

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
	 * The weapon's BODY — the asset that carries the mesh, the per-body anims, the shot sound and
	 * attenuation, the muzzle FX, the tracer and the impact decal. A gun IS its stat block; the body
	 * is shared by every gun on the same rig (pistol+SMG on WBD_Pistol, both rifles on WBD_Rifle),
	 * which is what keeps "a new gun = one stat block" true.
	 *
	 * Empty on a thing that is not a ranged weapon (melee, a grenade, a character baseline block).
	 * Presence-is-scope: no body, no gun.
	 *
	 * HISTORY: this property was cut on 2026-09-16 (box 3.2) as
	 * `TSoftClassPtr<UProsperitocracyEquipmentDefinition>` — UHT rejects a forward-declared class in a
	 * UPROPERTY, and the real class pulls in UProsperitocracyAbilitySet + UProsperitocracyEquipmentInstance,
	 * i.e. Lyra's equipment stack. It comes back here typed to our own body asset instead: same job
	 * (the stat block knows which body it rides on), no Lyra chain behind it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
	TSoftObjectPtr<UProsperitocracyWeaponBodyData> WeaponBody;

	/** The weapon's body asset, or null on a thing that is not a gun. Defined in the .cpp so this
	 *  header does not have to know the body asset's type. */
	UProsperitocracyWeaponBodyData* GetWeaponBody() const;

	/**
	 * Builds the canonical default entry list for the universal stat table — every stat-ID
	 * and its default base value. This is the single source of truth for the default table,
	 * mirroring Design/stats.md. A data asset can be populated from this (e.g. when authoring
	 * a design default).
	 */
	static void BuildCanonicalStats(TArray<FProsperitocracyStatTableEntry>& OutEntries);
};
