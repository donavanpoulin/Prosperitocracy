// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
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
	 * selected index; a character with no class plays on the loadout it was authored with. Nothing
	 * holds a second copy of this answer: everything that has to know — a gun, the armour, the weight
	 * — asks here, exactly as it asks for a slot's entry.
	 */
	UProsperitocracyLoadout* GetLoadout() const;

	/**
	 * Play one of the class's three loadouts, and dress the body from the one chosen.
	 *
	 * This is the door a loadout is switched through: choose the loadout, and the armour it names
	 * comes with it — one call, the same one the body makes at spawn, so a swap can never dress a
	 * body differently from the way it spawns. Guns already in hand keep the numbers they were
	 * dressed with until the rig re-creates them; the picker's own re-dress lands with the picker
	 * (ROADMAP section 2).
	 *
	 * False when there is no class, or the index is not one of the three: nothing changes, and the
	 * character keeps playing the loadout it is already playing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	bool SelectLoadout(int32 Index);

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

	//~ DEV — the in-game gun sandbox (Development/ProsperitocracyDevCommands.cpp) ---------------

	/**
	 * DEV ONLY: put a different stat block into a slot right now, and give that slot a fresh gun's
	 * ammo — a full magazine and a full spare pool.
	 *
	 * Why this is the whole sandbox and not a new system: four guns are two bodies x four blocks, and
	 * a body carries NO numbers, so a "different gun" differs in exactly one thing — its block. This
	 * is therefore the same act DressGun already performs at spawn (hand the body a block), asked for
	 * again at runtime. It works only because ApplyLoadoutEntry is re-callable by design: "a body can
	 * be two guns: the pistol's body is also the SMG's".
	 *
	 * The override lives HERE, on the component, and not on the gun: the rig destroys and re-creates
	 * gun actors (a slot switch, a level change), so anything kept on a gun dies with it. Kept on the
	 * carrier, the block survives the gun actor being thrown away and rebuilt.
	 *
	 * It is not a second path: the block is still resolved through the ONE evaluator, and the block's
	 * own slot tag is still checked against the slot it is installed in, so a dev command cannot file
	 * a Secondary block under Primary.
	 *
	 * Transient by construction — a level change, a respawn or a PIE restart drops every override,
	 * because it is runtime state, never authored data.
	 *
	 * Returns false and fills OutMessage with the reason when the block cannot be installed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loadout|Dev")
	bool DevEquipStatBlock(UProsperitocracyStatTable* StatBlock, FString& OutMessage);

private:
	/**
	 * DEV ONLY: what a slot's block is overridden to, instead of the loadout asset's entry for it.
	 * Empty in every real (non-sandbox) session — nothing but the dev gun command writes it.
	 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UProsperitocracyStatTable>> DevStatBlockBySlot;

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
