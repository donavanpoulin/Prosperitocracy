// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Weapons/ProsperitocracyLoadout.h"

#include "ProsperitocracyLoadoutComponent.generated.h"

class AProsperitocracyWeapon;
class UGameplayEffect;

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

	/** What this character carries. The one place a gun's numbers are decided. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TObjectPtr<UProsperitocracyLoadout> Loadout;

	UProsperitocracyLoadout* GetLoadout() const { return Loadout; }

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
	/** One entry per slot this carrier has fired or reloaded, keyed by the slot's tag. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, FProsperitocracyWeaponAmmo> AmmoBySlot;
};
