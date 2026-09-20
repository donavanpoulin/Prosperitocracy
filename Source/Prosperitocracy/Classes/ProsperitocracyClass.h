// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "ProsperitocracyClass.generated.h"

class UProsperitocracyGameplayAbility;
class UProsperitocracyLoadout;

/**
 * UProsperitocracyClass
 *
 * ONE CLASS, as data — the Reclaimer or the Anchor (Design/classes/). It is what a player picks
 * BEFORE they pick a loadout, and it is the owner of everything a class decides:
 *
 *   - its name and its colour, which are the class's and nobody else's;
 *   - its THREE loadouts (Design/loadout.md) — the class owns the three, so a player has six builds;
 *   - its ability roster, which its loadouts pick four from;
 *   - its access rules — which slots it can put a weapon in, and whether its Primary is its melee.
 *
 * One field per loadout, deliberately: loadout 1 / 2 / 3 is a place on the class, and a class that
 * carried a list of loadouts could carry four or none. Three fields cannot be authored wrong.
 *
 * This asset is pure data for now — nothing reads it yet. The picker and the slot path's access
 * check land on top of it (ROADMAP section 2), and they read these fields rather than a second copy
 * of them.
 */
UCLASS(BlueprintType)
class UProsperitocracyClass : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The class as the player reads it: "Reclaimer", "Anchor". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class")
	FText DisplayName;

	/**
	 * The class's own colour, as ONE number — 0xRRGGBB.
	 *
	 * The same shape a trim row carries, on purpose (Design/stats.md: Trim 1 / Trim 2 / Trim 3 hold a
	 * region's colour as one hex). A loadout's three trims default to this (Design/loadout.md), so
	 * giving a body its class colour is a copy and never a conversion — one colour vocabulary.
	 *
	 * The two classes the palette names are the Reclaimer #820721 and the Anchor #075F82 (Design/ui.md,
	 * 0x820721 / 0x075F82 here).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class")
	int32 ClassColor = 0xFFFFFF;

	/**
	 * The class's three loadouts (Design/loadout.md). Any of them may be left empty: a loadout can be
	 * carried with nothing on it, so a class with no loadout in a slot is a real state, not a broken
	 * one (the same rule an empty weapon slot follows).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class|Loadouts")
	TObjectPtr<UProsperitocracyLoadout> Loadout1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class|Loadouts")
	TObjectPtr<UProsperitocracyLoadout> Loadout2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class|Loadouts")
	TObjectPtr<UProsperitocracyLoadout> Loadout3;

	/**
	 * The class's abilities — the POOL its loadouts choose from, not a granted set.
	 *
	 * A loadout has 4 ability slots and the roster is larger than the slots (Design/abilities.md,
	 * Design/classes/reclaimer.md), so what a character actually owns is the four its loadout names.
	 * Granting this list would give the whole roster to everyone, which is the opposite of picking —
	 * so this is a list to choose from, and the four picked are what gets granted.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class|Abilities")
	TArray<TSubclassOf<UProsperitocracyGameplayAbility>> Roster;

	/**
	 * The slots this class can put a weapon in — the project's own slot vocabulary
	 * (Prosperitocracy.Weapon.Slot.*, the tag a weapon carries as its slot).
	 *
	 * The Reclaimer carries no Special (Design/classes/reclaimer.md), so its set simply has no
	 * Special in it; the Anchor's has all four. Presence is scope, the same rule the stat table runs on.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class|Access", meta = (Categories = "Prosperitocracy.Weapon.Slot"))
	FGameplayTagContainer UsableSlots;

	/**
	 * TRUE for a class whose Primary IS its melee, never a gun — the Reclaimer's blood blade
	 * (Design/classes/reclaimer.md: "Blood blade — melee, fills a primary weapon slot", ranged =
	 * secondaries only). FALSE for a class that picks its Primary freely.
	 *
	 * Melee is a weapon with no fire-mode tag (Design/weapons.md: "anything without the tag (melee)
	 * isn't a gun"), so this one flag is the whole rule — there is no second melee vocabulary to keep
	 * in step. Not enforced yet: the access check lands in the slot path (ROADMAP section 2).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Class|Access")
	bool bPrimaryMustBeMelee = false;
};
