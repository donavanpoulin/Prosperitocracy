// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Stats/ProsperitocracyStat.h"
#include "Weapons/ProsperitocracyLoadout.h"

#include "ProsperitocracyLoadoutComponent.generated.h"

class AProsperitocracyWeapon;
class UGameplayEffect;
class UProsperitocracyClass;

/**
 * The magazine and the spare pool of ONE slot. Runtime state, never authored data.
 *
 * The magazine in the gun and the rounds in the pool are the CARRIER's, not the gun actor's: the rig
 * re-creates gun actors (a slot switch, a level change), and anything kept on the gun dies with it —
 * a full magazine every time, which is a free reload nobody asked for. Keyed by slot, the pistol's
 * magazine is the Secondary slot's magazine: it outlives the gun, and it is the same pool if a
 * different weapon is ever swapped into that slot.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyWeaponAmmo
{
	GENERATED_BODY()

	/** Rounds in the magazine that is in the gun right now. */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo")
	int32 Magazine = 0;

	/** Rounds in the spare magazines this slot carries. */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo")
	int32 Spare = 0;
};

/**
 * UProsperitocracyLoadoutComponent
 *
 * The character's half of the loadout: it holds what this character carries, and it owns that
 * carrier's ammo. It answers the one question a gun cannot answer about itself — "which slot am I,
 * what does this slot carry, and how much is left in it?".
 *
 * The gun does NOT look itself up, and this is the point of the component: the template's rig creates
 * its guns itself and says nothing about which slot they are, so the loadout — which knows every slot
 * and every body — is what identifies a gun and hands it its numbers. A gun therefore never holds a
 * copy of the answer, so it cannot hold a stale one.
 *
 * Once the equip path brings a gun in for a known slot, it asks for that slot's entry directly and
 * this component's body-class answer becomes the path for guns the rig created on its own.
 *
 * Slot switching (the template's keys 1 and 2, `Is Pistol Equip?`, `Animation_State`) stays the
 * template's rig: it decides WHICH gun is in hand and how it moves. This decides WHAT that gun is.
 */
UCLASS(ClassGroup = (Prosperitocracy), meta = (BlueprintSpawnableComponent))
class UProsperitocracyLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProsperitocracyLoadoutComponent();

	/**
	 * What this character carries when NO class is chosen — the loadout a class-less body plays on,
	 * which is where the test character stands until the class assets exist (ROADMAP section 2).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TObjectPtr<UProsperitocracyLoadout> Loadout;

	/** The class being played (Design/classes/). Null = no class chosen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TObjectPtr<UProsperitocracyClass> Class;

	/** Which of the class's three loadouts is being played: 0, 1 or 2. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	int32 SelectedLoadout = 0;

	/**
	 * The loadout being played — the ONE answer to "what does this character carry".
	 *
	 * A class owns three loadouts (Design/loadout.md), so the one being played is the class's at the
	 * selected index, and a character with no class plays on the loadout it was authored with.
	 *
	 * It is this character's OWN COPY of that loadout, made once when the character comes up — because a
	 * class's three loadouts ARE the defaults the game ships, and nothing in play is allowed to write to
	 * those. Change a loadout in play and you change your copy of it: the shipped default is untouched,
	 * a switch comes back to your copy with your changes still in it, and when the finished game has a
	 * save, the copy is what it saves.
	 *
	 * Nothing holds a second copy of this answer: everything that has to know — a gun, the armour, the
	 * weight — asks here, exactly as it asks for a slot's entry.
	 */
	UProsperitocracyLoadout* GetLoadout() const;

	/**
	 * What this character holds at one of a class's three indices — the copy it made of that loadout when
	 * it came up, and the asset that copy came from when it has not made one.
	 *
	 * This is what a listing reads: the thing the character would actually play, which is not the same
	 * question as "what does the class ship" once anything has been changed in play.
	 */
	UProsperitocracyLoadout* GetLoadoutAt(int32 Index) const;

	/**
	 * Play one of the class's three loadouts, and dress the body from the one chosen.
	 *
	 * This is the door a loadout is switched through: choose the loadout, and what it carries comes
	 * with it — the ARMOUR through the same call the body makes at spawn, and the GUNS already in hand
	 * through the same DressGun that gave them their numbers — so a switch can never dress a body or a
	 * gun differently from the way they came up.
	 *
	 * A slot the new loadout no longer carries is left as it is: which gun actors exist in the rig is
	 * the rig's business, not this call's.
	 *
	 * False when there is no class, or the index is not one of the three: nothing changes, and the
	 * character keeps playing the loadout it is already playing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool SelectLoadout(int32 Index);

	//~ Changing what you take in, IN PLAY — the doors a gun, a weave and a colour go through ---------
	//
	// Nothing about a character is set by reaching into the body: a gun, a weave and a colour are
	// written into the loadout being played, and the body is dressed from there by the same dress a
	// switch performs. That is what makes a change SURVIVE a switch — there is one holder of it, and a
	// switch reads the same holder — and it is why the dev commands and the picker go through here
	// rather than each writing their own thing.

	/**
	 * Put a weapon into one of the playing loadout's slots, with the class's access rules enforced HERE
	 * — because this is the door a weapon goes into a slot through.
	 *
	 * Refused, with the reason: a slot this class does not carry (the Reclaimer has no Special), a
	 * Primary for a class whose Primary is its melee (the Reclaimer's blade — melee is a weapon with no
	 * fire-mode tag), and a stat block whose own slot tag disagrees with the slot it is being put in.
	 *
	 * An entry that names nothing empties the slot. A gun the rig has already built keeps the numbers it
	 * is holding until the rig rebuilds it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool SetWeaponInSlot(FGameplayTag Slot, FProsperitocracyWeaponSlot Entry, FString& OutMessage);

	/**
	 * Wear a weave in the playing loadout — the armour half of it. Null is a real choice: a loadout that
	 * names no weave is a bare body. Armour is not class-locked (Design/armor.md), so there is no access
	 * rule to check here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool SetWeave(UProsperitocracyStatTable* Weave, FString& OutMessage);

	/**
	 * Set one of the armour's three colour regions in the playing loadout: the region's colour row, and
	 * the body is painted from it on the spot.
	 *
	 * A colour is not part of the weave and never needs one (Design/armor.md): it is the body's own row,
	 * so it is set the same way whether the body is wearing anything or not. `Hex` is the RGB
	 * (0xRRGGBB), or ProsperitocracyArmor::NoColor for "nothing chosen" — the paint it ships with.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool SetTrim(EProsperitocracyStat Trim, int32 Hex, FString& OutMessage);

	/** What this character carries in that slot, or null when the slot names nothing. */
	const FProsperitocracyWeaponSlot* GetEntryForSlot(const FGameplayTag& Slot) const;

	/** What every gun's shot applies, owned by the loadout rather than by each body blueprint. */
	TSubclassOf<UGameplayEffect> GetGunDamageEffectClass() const;

	/**
	 * Give one gun its numbers: tell it which slot it is and what that slot carries.
	 *
	 * The answer is a result rather than a bare bool, because every way this can fail means something
	 * different to whoever reads the log — nothing carried for that body, a body carried twice, a
	 * missing stat block, or a weapon whose own slot tag disagrees with where it is carried.
	 */
	EProsperitocracyWeaponDressResult DressGun(AProsperitocracyWeapon* Gun);

	/**
	 * Everything this character carries, in lbs — the sum of each carried thing's Weight: the guns in
	 * its slots AND the armor on its body.
	 *
	 * The loadout is the only thing that knows the whole kit, which is why the total is answered here
	 * and not by any one item. A carried thing that the rig has already dressed is asked through its
	 * own GAS home, so anything that modified THAT item's weight (a weight-reduction perk on a gun, or
	 * on an armor) is counted; a thing that is carried but not yet in the world is read from its stat
	 * block, which is exactly the number its host would be given (a block is the thing's own data, and
	 * a thing that does not exist yet has nothing else). Design/loadout.md: "Every weapon has a weight.
	 * Every backpack has a weight." — this is that total, and nothing is on it that is not carried.
	 *
	 * An armor is carried the same way a gun is: it is on the body, so it is in this one total, asked
	 * by the same rule — and that is the whole reason a heavier weave slows the body down.
	 *
	 * The unit is pounds, and it is not decoration: it is what the UI shows and what the movement
	 * penalty is computed from.
	 */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	float GetCarriedWeightLbs() const;

	//~ Ammo — one store per slot, owned here rather than on the gun that fires it.

	/**
	 * The ammo this slot holds, created from that slot's numbers the first time it is asked for.
	 *
	 * The first ask is what fills the magazine: MagSize rounds loaded, and a spare pool of
	 * (Capacity x MagSize) minus the one already in the gun — the loaded magazine IS one of the
	 * Capacity magazines. More magazines never makes a magazine bigger.
	 */
	FProsperitocracyWeaponAmmo& GetOrCreateAmmoForSlot(const FGameplayTag& Slot, int32 MagazineSize, int32 MagazineCapacity);

	/** The ammo this slot holds as it stands, or null when it holds none yet. Creates nothing. */
	const FProsperitocracyWeaponAmmo* FindAmmoForSlot(const FGameplayTag& Slot) const;

private:

	//~ The loadout in play, held as this character's own copy of what a class ships --------------------

	/**
	 * This character's own copy of each loadout it can play, under the index it is played at.
	 *
	 * Made once, when the character comes up, from the class's three — or from the loadout a class-less
	 * character was authored with, at index 0 (a character has a class or it does not, so the two never
	 * meet). The copy is what makes "change your loadout in play" safe: nothing is ever written to the
	 * asset the game ships, the copy is what a switch comes back to with your changes still in it, and
	 * it is what a save will hold when there is one.
	 */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UProsperitocracyLoadout>> PlayingCopies;

	/** Copy each loadout this character can play, once, when it comes up. */
	virtual void BeginPlay() override;

	/** The asset a loadout in play is copied from: the class's at that index, or the authored one at index 0. */
	UProsperitocracyLoadout* ResolveLoadoutSource(int32 Index) const;

	/**
	 * Dress the body from the loadout being played: the armour, then the guns in hand.
	 *
	 * ONE act, so a switch and a change made in play cannot dress a body or a gun differently — and so
	 * there is exactly one place that has to know what "this character is now carrying something else"
	 * means.
	 */
	void Redress();

	/** One entry per slot this carrier has fired or reloaded, keyed by the slot's tag. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, FProsperitocracyWeaponAmmo> AmmoBySlot;

	/**
	 * The gun actors this carrier has dressed, by slot.
	 *
	 * Kept so the carried weight can be read from a live gun's own GAS home rather than from the block
	 * it was built from — the two agree until something modifies an item's weight, and when they stop
	 * agreeing the live one is the truth. Weak, because the rig destroys and re-creates gun actors
	 * (a slot switch, a level change) and a dead gun must not be read from or held alive here.
	 */
	TMap<FGameplayTag, TWeakObjectPtr<AProsperitocracyWeapon>> DressedGunsBySlot;
};
