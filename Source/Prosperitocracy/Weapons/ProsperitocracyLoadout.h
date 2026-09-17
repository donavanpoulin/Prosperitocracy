// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "ProsperitocracyLoadout.generated.h"

class AActor;
class UGameplayEffect;
class UProsperitocracyStatTable;

/**
 * One gun the player carries, as it sits in one slot.
 *
 * A gun is a BODY and a STAT BLOCK, and they are two different things with two different owners:
 *
 *   - the body is the gun blueprint (BP_Pistol / BP_Rifle): the mesh, the Muzzle socket, the fire and
 *     reload anims, the montages, the sounds, the muzzle flash, the tracer, the decal, the impact
 *     particle, and the shot graph that plays them. It carries NO numbers.
 *   - the stat block is the numbers: MagSize, Capacity, Rate, PiercingDamage, Penetration, Range,
 *     Falloff, Accuracy, Recoil, Weight, FireMode. It is the ONLY owner of them.
 *
 * That split is what makes four guns out of two bodies: the SMG is the pistol's body with a different
 * block, the rifle-auto is the rifle's body with a different block. A gun blueprint cannot express
 * that with a default value, which is exactly why the numbers are not a default on it.
 *
 * The slot itself is NOT here: it is the tag on the stat block (`UProsperitocracyStatTable::Slot`),
 * because the weapon is what is tagged with its slot, not the place it happens to be carried.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyWeaponSlot
{
	GENERATED_BODY()

	/** The gun blueprint this slot's weapon rides: the body, with its shot graph. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot")
	TSoftClassPtr<AActor> BodyClass;

	/** The numbers this gun is. The one owner of them, and the tag that says which slot it goes in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot")
	TSoftObjectPtr<UProsperitocracyStatTable> StatBlock;
};

/**
 * What a loadout answers when it is asked to dress a gun.
 *
 * The loadout identifies a gun the rig created without saying which slot it is, so it can and does
 * refuse: a body carried in more than one slot cannot be identified by blueprint alone, and a gun
 * with a slot tag that disagrees with the slot it is carried in is a misconfiguration. In both cases
 * the gun is left with no numbers rather than dressed on a guess.
 */
UENUM(BlueprintType)
enum class EProsperitocracyWeaponDressResult : uint8
{
	/** Not asked yet, or the gun is not in the rig yet — normal at BeginPlay for a re-created gun. */
	Undressed,

	/** The rig has no loadout component, so there is nothing to ask. */
	NoLoadout,

	/** This loadout carries nothing for that gun blueprint. */
	NoEntry,

	/** More than one slot carries that gun blueprint, so a blueprint cannot say which one it is. */
	AmbiguousEntry,

	/** The entry is there but carries no stat block: no numbers. */
	NoStatBlock,

	/** The block's own slot tag is not the slot this loadout carries it in. */
	SlotMismatch,

	/** The gun has its slot and its numbers. */
	Dressed
};

/**
 * UProsperitocracyLoadout
 *
 * What one character carries, as data — one weapon per slot, and the slots are the design's
 * (Design/weapons.md): Primary, Secondary, Special, Grenade. A slot names a body or it names nothing,
 * and a slot that names nothing is simply not carried; that is how Special and Grenade sit empty
 * until we have those weapons, with no placeholder to clean up later.
 *
 * One field per slot is deliberate: two weapons in one Primary is not something to validate against,
 * it is something that cannot be authored. The universal stat table stays the authority on every
 * number, and a gun asks its owner's loadout what it is made of rather than holding a copy.
 */
UCLASS(BlueprintType)
class UProsperitocracyLoadout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The primary slot: the rifles today, and every class primary as it lands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout|Slots")
	FProsperitocracyWeaponSlot Primary;

	/** The secondary slot: pistol and SMG. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout|Slots")
	FProsperitocracyWeaponSlot Secondary;

	/** The special slot: LMG / minigun / launchers. Empty until we have them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout|Slots")
	FProsperitocracyWeaponSlot Special;

	/** The grenade slot: one grenade. Empty until we have one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout|Slots")
	FProsperitocracyWeaponSlot Grenade;

	/**
	 * What every gun's shot applies. One asset for all guns, owned HERE rather than baked into each
	 * body blueprint — a second copy is a second answer to the same question.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TSubclassOf<UGameplayEffect> GunDamageEffectClass;

	/**
	 * What this loadout carries in that slot, or null when the slot names nothing. The slot tag is
	 * one of Prosperitocracy.Weapon.Slot.* — the vocabulary the weapon itself is tagged with.
	 */
	const FProsperitocracyWeaponSlot* FindEntryForSlot(const FGameplayTag& Slot) const;

	/**
	 * The entry a gun blueprint belongs to, and the slot it sits in.
	 *
	 * This exists for a gun the rig created without telling us which slot it is (the template's child
	 * actor components spawn guns with no slot of their own). OutMatchCount says how many slots carry
	 * that body, so the caller can tell "this loadout carries nothing for that gun" apart from "that
	 * body is carried in more than one slot, so a blueprint cannot identify it". Returns null unless
	 * exactly one slot carries it.
	 */
	const FProsperitocracyWeaponSlot* FindEntryForBodyClass(const UClass* InBodyClass, FGameplayTag& OutSlot, int32& OutMatchCount) const;

	/**
	 * Every stat block this loadout actually carries, with the slot tag it sits in.
	 *
	 * Empty slots are skipped, and so is an entry with no stat block: neither of those is a thing the
	 * character carries, so neither of them weighs anything. This is the carried SET — what the
	 * character is holding, whether or not the rig has spawned an actor for it yet — which is what
	 * anything that has to answer for the whole kit (its weight, its ammo, its slots) iterates.
	 */
	void CollectCarriedBlocks(TArray<TPair<FGameplayTag, UProsperitocracyStatTable*>>& Out) const;

private:
	/** The four slots as (tag, entry) pairs, in slot order: Primary, Secondary, Special, Grenade. */
	void CollectEntries(TArray<TPair<FGameplayTag, const FProsperitocracyWeaponSlot*>>& Out) const;
};
