// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Stats/ProsperitocracyStat.h"
#include "Weapons/ProsperitocracyLoadout.h"

#include "ProsperitocracyWeapon.generated.h"

class AProsperitocracyStatHostActor;
class APawn;
class UGameplayEffect;
class USkeletalMeshComponent;
class UProsperitocracyLoadoutComponent;
class UProsperitocracyStatTable;
struct FProsperitocracyWeaponAmmo;

/**
 * AProsperitocracyWeapon
 *
 * The NUMBERS and the DAMAGE of one gun. Nothing visual.
 *
 * The template's own gun blueprints are the gun: BP_MasterWeapon, and BP_Pistol / BP_Rifle under it,
 * already carry the mesh, the Muzzle socket, the fire and reload anims, the character montages, the
 * sounds with their attenuation and concurrency, the muzzle flash, the tracer, the decal and the
 * impact particle — and their EventFire/EventReload already trace and play all of it. This class is
 * what those blueprints inherit, so that the SAME shot:
 *
 *   (a) spends a round out of OUR magazine instead of their Weapon_Details ammo struct, and
 *   (b) deals OUR damage on the hit they already trace.
 *
 * (b) is the whole missing half: their EventFire traced, drew FX and stopped — there is no
 * ApplyDamage anywhere in it. That is why their guns deal nothing today.
 *
 * The gun's numbers are not here either. They are its stat block (UProsperitocracyStatTable),
 * resolved through this gun's own GAS home (AProsperitocracyStatHostActor) by the ONE evaluator:
 * MagSize, Capacity, Rate, PiercingDamage, Penetration, Range, Falloff, Accuracy, Recoil, Weight.
 *
 * There is deliberately NO body asset: the gun blueprint IS the body, so a second asset pointing at
 * the same mesh, anims and sounds would be a competing source for one thing.
 */
UCLASS(BlueprintType)
class AProsperitocracyWeapon : public AActor
{
	GENERATED_BODY()

public:
	AProsperitocracyWeapon();

	/**
	 * Take the gun's place in the world: which slot it is, its numbers, what its shot applies, and
	 * where its ammo lives.
	 *
	 * Called by the owner's loadout component, which is the only thing that can say which slot this
	 * gun is — the rig that created it never does. All of it arrives together, because it is one
	 * decision. Callable again whenever the loadout changes what this body carries (a body can be two
	 * guns: the pistol's body is also the SMG's).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool ApplyLoadoutEntry(const FGameplayTag& InSlot, UProsperitocracyStatTable* InStatBlock, TSubclassOf<UGameplayEffect> InDamageEffectClass, UProsperitocracyLoadoutComponent* InOwnerLoadout, APawn* InOwningPawn = nullptr);

	/**
	 * Make sure this gun has its numbers, and return whether it does.
	 *
	 * Called on BeginPlay AND on first use, because a gun spawned by a child actor component is NOT in
	 * the rig yet when BeginPlay runs — a child actor is attached after it spawns, so at that moment
	 * there is no attach parent and no owner to ask about the loadout. By the time the gun is fired or
	 * reloaded it is in the rig, so the same call succeeds. That is why this is a door and not a
	 * one-shot BeginPlay step, and why the gun never initializes from a guess.
	 */
	bool EnsureInitialized();

	//~ The two things their gun graph asks us, plus the damage it hands us -------------------------

	/**
	 * One committed shot's worth of ammo. True when a round actually left the magazine, so the gun's
	 * own graph runs its shot; false when the magazine is empty (the graph's dry-fire branch) or the
	 * Rate cadence has not elapsed (a silent refusal — not a dry fire).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool TryConsumeRound();

	/** True when the magazine is empty — what decides their dry-fire branch, not a refused cadence. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool IsMagazineEmpty() const;

	/**
	 * Reload = a MAG SWAP, not a top-up: whatever is left in the magazine is wasted, never returned
	 * to the pool, and a fresh magazine comes out of the pool — partial if that is all there is.
	 * False when there is no spare magazine to load.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	bool ReloadFromStats();

	/**
	 * OUR damage, on the hit the gun's own trace produced. The effect context carries that hit (the
	 * impact point for falloff, the part hit for its armor and resists) and points at this gun's stat
	 * host as the ABILITY SOURCE, which is where UProsperitocracyDamageExecution reads the damage
	 * lines and the falloff from.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void ApplyShotDamage(const FHitResult& Hit);

	//~ Reads -------------------------------------------------------------------------------------

	/** The gun's numbers — its row in the universal stat table. A gun IS its stat block. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	UProsperitocracyStatTable* GetStatBlock() const { return StatBlockAsset; }

	/**
	 * Which slot of its owner's loadout this gun came out of — told to it by the loadout, never
	 * looked up by the gun. Invalid until it is dressed.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	FGameplayTag GetSlot() const { return Slot; }

	/** What the loadout answered when this gun asked to be dressed. Dressed once it has numbers. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	EProsperitocracyWeaponDressResult GetDressResult() const { return DressResult; }

	AProsperitocracyStatHostActor* GetStatHost() const { return StatHost; }

	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const;

	/** FINAL value of one of this gun's stats, through the ONE evaluator (its own GAS home). */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	float GetWeaponStat(EProsperitocracyStat Stat) const;

	/** True when the stat block carries the FullAuto fire-mode tag. A gun without a mode is not fired. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	bool IsFullAuto() const;

	/** Seconds between shots = 1 / Rate. The single cadence, for every gun. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	float GetSecondsBetweenShots() const;

	/** Rounds in the magazine right now. Read from the owner's ammo store for this gun's slot. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	int32 GetMagazineAmmo() const;

	/** Rounds in this slot's spare magazines. Read from the owner's ammo store. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	int32 GetSpareAmmo() const;

	/** The aim circle's current drift, in degrees. The bullet and the reticle add the SAME value. */
	FVector2D GetAimDriftDegrees() const { return AimDriftDegrees; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Which slot of its owner's loadout this gun is. Handed over by the loadout, along with the
	 * numbers — a gun cannot read it off itself, and it must not guess it from its blueprint, because
	 * one body can be two guns (the pistol's body is also the SMG's).
	 */
	UPROPERTY(Transient)
	FGameplayTag Slot;

	/**
	 * The numbers, handed over by the loadout when this gun came up.
	 *
	 * Deliberately NOT a default on the gun blueprint. A default is a second copy of an answer that
	 * has an owner, and the template's child actor components keep an archetype of the gun inside the
	 * character asset — so a defaulted value gets copied there and then goes stale. The gun asks the
	 * loadout what it is; nowhere on the gun can a stale number live.
	 *
	 * Named `StatBlockAsset` rather than the obvious `StatBlock` on purpose: `BP_Pistol` and
	 * `BP_Rifle` still had values stored under the old name from before this was Transient, and
	 * renaming the property is what makes UE drop those stored keys on the next save. Same for
	 * `ShotDamageEffectClass` below.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyStatTable> StatBlockAsset;

	/**
	 * What a committed shot applies to whatever it hits, also from the loadout — one asset for every
	 * gun, for the same reason. Its execution must be UProsperitocracyDamageExecution: that reads the
	 * damage lines and the falloff from the shot's ability source, which is this gun's stat host.
	 */
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ShotDamageEffectClass;

	/** The gun's GAS home — its stats are attributes on this actor's own ASC (the ONE evaluator). */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Weapon")
	TObjectPtr<AProsperitocracyStatHostActor> StatHost;

	/**
	 * The owner's loadout, which is where this gun's AMMO lives — one store per slot, on the carrier.
	 *
	 * Nothing about this gun's ammo is kept on the gun: the rig re-creates gun actors, and a magazine
	 * that lived here would come back full every time. Handed over when the loadout dresses the gun.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyLoadoutComponent> OwnerLoadout;

	//~ Runtime state.
	UPROPERTY(Transient) TObjectPtr<APawn> OwningPawn;

	/** Whether the numbers are in hand. False until the loadout has answered. */
	bool bInitialized = false;

	/** What the loadout answered. Undressed until it is asked, and until the gun is in the rig. */
	EProsperitocracyWeaponDressResult DressResult = EProsperitocracyWeaponDressResult::Undressed;

	/** One warning per gun, not one per shot, for why the loadout could not dress it. */
	bool bDressFailureLogged = false;

	float LastShotTime = -BIG_NUMBER;
	FVector2D AimDriftDegrees = FVector2D::ZeroVector;

private:
	bool CanFireNow() const;

	/** One magazine's worth of rounds, from this gun's own MagSize stat. */
	int32 MagazineSize() const;

	/** How many magazines this gun carries, from its own Capacity stat. */
	int32 MagazineCapacity() const;

	/** This gun's ammo in its slot's store, or null when undressed or the slot holds none yet. */
	const FProsperitocracyWeaponAmmo* FindAmmo() const;

	/** Say once, per gun, why the loadout could not dress it — each reason names a different fix. */
	void LogDressFailureOnce(EProsperitocracyWeaponDressResult Result, const AActor* RigOwner);
};
