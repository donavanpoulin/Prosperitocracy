// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Stats/ProsperitocracyStat.h"

#include "ProsperitocracyWeapon.generated.h"

class AProsperitocracyStatHostActor;
class APawn;
class UGameplayEffect;
class USkeletalMeshComponent;
class UProsperitocracyStatTable;

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
	 * Point the gun at its numbers: spawn its GAS home, push the block's bases into it, and fill the
	 * first magazine. The gun's own BeginPlay calls this with what its owner's LOADOUT says this body
	 * is made of — the numbers and the effect its shot applies arrive together, because they are one
	 * decision. Callable again whenever the loadout changes what this body carries.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void InitializeFromStatBlock(UProsperitocracyStatTable* InStatBlock, TSubclassOf<UGameplayEffect> InDamageEffectClass, APawn* InOwningPawn = nullptr);

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
	bool IsMagazineEmpty() const { return MagazineAmmo <= 0; }

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
	UProsperitocracyStatTable* GetStatBlock() const { return StatBlock; }

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

	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	int32 GetMagazineAmmo() const { return MagazineAmmo; }

	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	int32 GetSpareAmmo() const { return SpareAmmo; }

	/** The aim circle's current drift, in degrees. The bullet and the reticle add the SAME value. */
	FVector2D GetAimDriftDegrees() const { return AimDriftDegrees; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * The numbers, handed over by the loadout when this gun came up.
	 *
	 * Deliberately NOT a default on the gun blueprint. A default is a second copy of an answer that
	 * has an owner, and the template's child actor components keep an archetype of the gun inside the
	 * character asset — so a defaulted value gets copied there and then goes stale. The gun asks the
	 * loadout what it is; nowhere on the gun can a stale number live.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyStatTable> StatBlock;

	/**
	 * What a committed shot applies to whatever it hits, also from the loadout — one asset for every
	 * gun, for the same reason. Its execution must be UProsperitocracyDamageExecution: that reads the
	 * damage lines and the falloff from the shot's ability source, which is this gun's stat host.
	 */
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** The gun's GAS home — its stats are attributes on this actor's own ASC (the ONE evaluator). */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Weapon")
	TObjectPtr<AProsperitocracyStatHostActor> StatHost;

	//~ Runtime state.
	UPROPERTY(Transient) TObjectPtr<APawn> OwningPawn;

	/** Whether the numbers are in hand. False until the loadout has answered. */
	bool bInitialized = false;

	/** One warning per gun, not one per shot, when the loadout carries no entry for this body. */
	bool bNoLoadoutEntryWarned = false;

	int32 MagazineAmmo = 0;
	int32 SpareAmmo = 0;
	float LastShotTime = -BIG_NUMBER;
	FVector2D AimDriftDegrees = FVector2D::ZeroVector;

private:
	bool CanFireNow() const;

	/** One magazine's worth of rounds, from this gun's own MagSize stat. */
	int32 MagazineSize() const;
};
