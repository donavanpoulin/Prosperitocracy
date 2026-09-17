// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Stats/ProsperitocracyStat.h"
#include "Weapons/ProsperitocracyLoadout.h"

#include "ProsperitocracyWeapon.generated.h"

class AProsperitocracyCharacter;
class AProsperitocracyStatHostActor;
class APawn;
class UGameplayEffect;
class USkeletalMeshComponent;
class UProsperitocracyLoadoutComponent;
class UProsperitocracyStatTable;
struct FProsperitocracyWeaponAmmo;

/**
 * The weapon-feel numbers: every constant the aim circle is built from.
 *
 * These are HARD RULES of the design (Design/ui.md, Design/combat.md) and they are universal — the
 * same value for every weapon, always. Nothing here is per-gun config: a gun's feel is exactly three
 * of its own STATS (Accuracy = the per-shot spread, Weight = the sway, Recoil = the up-climb) and
 * these constants are only the shape those stats are poured into. Changing one changes every weapon
 * at once, which is the point. All values [TUNE].
 */
namespace ProsperitocracyWeaponHandling
{
	// Hard cap, so the circle (trail + spread + climb) can never fly off the screen.
	constexpr float MaxDriftDegrees = 10.0f;

	// --- Posture multipliers: fixed for everybody, and they scale Accuracy only. ---
	constexpr float PostureMultiplier_Aiming = 1.4f;            // aiming down sights steadies
	constexpr float PostureMultiplier_StandingStill = 1.2f;     // standing still steadies
	constexpr float PostureMultiplier_Crouching = 1.35f;        // crouching steadies more
	constexpr float PostureMultiplier_JumpingOrFalling = 0.65f; // airborne is sloppier

	// How fast a posture change takes hold (per second), so none of them snaps.
	constexpr float TransitionRate_Posture = 5.0f;

	// At or below this speed (cm/s) the character counts as standing still; the bonus fades over range.
	constexpr float StandingStillSpeedThreshold = 80.0f;
	constexpr float StandingStillToMovingSpeedRange = 20.0f;

	// --- Handling trail (Weight-driven): the circle trails a camera swing. ---
	// Degrees of trail per degree of swing, and the return-to-centre rate per second.
	constexpr float LagTimeBase = 0.012f;       // light weapon: a short trail
	constexpr float LagTimePerWeight = 0.003f;  // heavier: a longer one
	constexpr float ReturnRateBase = 8.0f;      // light weapon: settles fast
	constexpr float ReturnRatePerWeight = 0.5f; // heavier: settles slower
	constexpr float ReturnRateMin = 2.0f;

	// --- Per-shot spread (Accuracy-driven). ---
	// Degrees of random displacement per shot at EffectiveAccuracy == 1.0.
	constexpr float SpreadShoveBaseDegrees = 1.2f;

	// --- Movement trail (Weight-driven): the aim is an inert mass and lags its own motion. ---
	// Degrees per second of displacement per (cm/s) of motion, and how much of a forward/back shift
	// shows vertically — a fraction of the sidestep, which is what makes the oval horizontal.
	constexpr float MoveDisplaceBase = 0.008f;
	constexpr float MoveDisplacePerWeight = 0.0019f;
	constexpr float MoveVerticalFraction = 0.35f;
}

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

	/**
	 * The aim circle's current offset from the dot, in degrees: X = yaw (+ = right), Y = pitch (+ = up).
	 *
	 * This one value has three readers — the reticle circle, the bullet, and the character's aim pose —
	 * which is what makes them one aim instead of three (Design/combat.md). Clamped on the way out, so
	 * no reader can ever be handed a circle that has left the screen.
	 */
	FVector2D GetAimDriftDegrees() const
	{
		constexpr float MaxDrift = ProsperitocracyWeaponHandling::MaxDriftDegrees;
		return FVector2D(
			FMath::Clamp(AimDriftDegrees.X, -MaxDrift, MaxDrift),
			FMath::Clamp(AimDriftDegrees.Y, -MaxDrift, MaxDrift));
	}

	/**
	 * The direction the next bullet flies along: the camera's aim plus this gun's drift.
	 *
	 * The shot reads this instead of the raw camera forward, which is what makes the bullet land where
	 * the reticle circle sits — and the circle is projected along this very vector, so the two cannot
	 * drift apart.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	FVector GetShotDirection() const;

	/**
	 * One committed shot's worth of feel: the recoil climb, then the per-shot spread shove.
	 *
	 * Called once per shot that actually leaves the gun, AFTER that shot has been traced — so a shot is
	 * never deflected by its own kick. The bullet goes where the circle was when the trigger broke, and
	 * the climb and shove move the circle for the next one (Design/ui.md: every bullet lands exactly
	 * where the circle points at that instant).
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Weapon")
	void ApplyShotFeel();

	/** (base Accuracy stat) x (the combined posture multiplier) — the driver of the per-shot shove. */
	float GetEffectiveAccuracy() const { return GetAccuracy() * CurrentAccuracyMultiplier; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
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

	/**
	 * The circle's offset from the dot, in degrees (X = yaw, Y = pitch) — the gun's whole aim state.
	 *
	 * Four drivers feed it and one decay pulls it back: the handling trail from Weight, the movement
	 * trail from Weight, the per-shot shove from Accuracy, and the climb from Recoil. Everything that
	 * wants to know where this gun is actually pointing — the bullet, the reticle, the pose — reads
	 * this one value (see GetAimDriftDegrees).
	 */
	FVector2D AimDriftDegrees = FVector2D::ZeroVector;

	// The current *combined* posture multiplier: ADS x standing still x crouching x airborne, all
	// folded into the one number that scales Accuracy. Each part is a universal constant above and
	// each eases in over TransitionRate_Posture rather than snapping.
	float CurrentAccuracyMultiplier = 1.0f;
	float StandingStillMultiplier = 1.0f;
	float JumpFallMultiplier = 1.0f;
	float CrouchingMultiplier = 1.0f;

	// The camera rotation last tick, so a swing can be measured against it. The flag is what keeps the
	// first tick after a gun comes up from reading as an enormous swing.
	FRotator LastControlRotation = FRotator::ZeroRotator;
	bool bHasLastControlRotation = false;

private:
	bool CanFireNow() const;

	/** The character holding this gun, or null when it is held by no one (or not a character yet). */
	AProsperitocracyCharacter* GetOwnerCharacter() const;

	/** The player steering this gun, or null when nobody is (an unattended gun, a dummy). */
	class APlayerController* GetOwningPlayerController() const;

	/** The Accuracy stat. Presence-is-scope: an absent stat is the baseline 1.0, not a zero. */
	float GetAccuracy() const;

	/** The Recoil stat, in degrees of up-climb per shot. Presence-is-scope: absent = 0, no climb. */
	float GetRecoil() const;

	/** One random per-shot shove of the circle, sized by Accuracy. This IS the spread. */
	void ApplySpreadShove();

	/** The per-tick drift: the handling trail, the movement trail, and the return to centre. */
	void UpdateDrift(float DeltaSeconds);

	/** The per-tick posture multipliers: ADS, standing still, crouching, airborne. */
	void UpdatePostureMultipliers(float DeltaSeconds);

	/** Hold the circle on the screen after anything that moved it. */
	void ClampDrift();

	/** One magazine's worth of rounds, from this gun's own MagSize stat. */
	int32 MagazineSize() const;

	/** How many magazines this gun carries, from its own Capacity stat. */
	int32 MagazineCapacity() const;

	/** This gun's ammo in its slot's store, or null when undressed or the slot holds none yet. */
	const FProsperitocracyWeaponAmmo* FindAmmo() const;

	/** Say once, per gun, why the loadout could not dress it — each reason names a different fix. */
	void LogDressFailureOnce(EProsperitocracyWeaponDressResult Result, const AActor* RigOwner);
};
