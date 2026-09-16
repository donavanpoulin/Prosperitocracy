// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stats/ProsperitocracyStat.h"

#include "ProsperitocracyWeapon.generated.h"

class AProsperitocracyStatHostActor;
class APawn;
class UAnimMontage;
class UAnimationAsset;
class UGameplayEffect;
class UNiagaraSystem;
class USkeletalMeshComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class UProsperitocracyStatTable;
class UProsperitocracyWeaponBodyData;

/**
 * AProsperitocracyWeapon
 *
 * One gun. Ours — it owns the shot: the trace, the ammo, the cadence, the fire mode, and the
 * damage it deals. The template's gun blueprints (BP_MasterWeapon / BP_Pistol / BP_Rifle) owned all
 * of that in Blueprint graphs; this class takes the logic and leaves them their assets, which it
 * plays through the body asset on the gun's stat block (UProsperitocracyWeaponBodyData).
 *
 * So the meshes, the anims, the montages, the sounds, the attenuations, the muzzle flash, the
 * tracer actor and the impact decal are all the template's own — the same assets its blueprints
 * pointed at, read off them rather than re-authored.
 *
 * The gun's numbers are NOT here. They are its stat block (UProsperitocracyStatTable), resolved by
 * the ONE evaluator through this gun's own GAS home (AProsperitocracyStatHostActor): MagSize,
 * Capacity, Rate, PiercingDamage, Penetration, Range, Falloff, Accuracy, Recoil, Weight. A new gun
 * is a new stat block on an existing body.
 */
UCLASS(BlueprintType)
class AProsperitocracyWeapon : public AActor
{
	GENERATED_BODY()

public:
	AProsperitocracyWeapon();

	/**
	 * Build the gun from its stat block: load the body it rides on, spawn its GAS home and push the
	 * block's bases into it, then fill the first magazine.
	 */
	void InitializeFromStatBlock(UProsperitocracyStatTable* InStatBlock, APawn* InOwningPawn = nullptr);

	/**
	 * One committed shot. False when the Rate cadence has not elapsed (silent — too soon, not empty)
	 * or when the magazine is empty (dry fire plays, and the shot is refused).
	 */
	bool Fire();

	/**
	 * Reload = a MAG SWAP, not a top-up: whatever is left in the magazine is wasted, never returned
	 * to the pool, and a fresh magazine comes out of the pool — partial if that is all there is.
	 */
	bool Reload();

	//~ Reads -------------------------------------------------------------------------------------

	/** The gun's numbers — its row(s) in the universal stat table. A gun IS its stat block. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Weapon")
	UProsperitocracyStatTable* GetStatBlock() const { return StatBlock; }

	AProsperitocracyStatHostActor* GetStatHost() const { return StatHost; }
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyBody(const UProsperitocracyWeaponBodyData* InBody);

	bool CanFireNow() const;

	/** The whole shot line: camera (with drift) to find the aim point, then muzzle to that point. */
	FHitResult TraceShot(FVector& OutMuzzleLocation, FVector& OutTraceEnd) const;

	void PlayShotFeedback(const FHitResult& Hit, const FVector& MuzzleLocation, const FVector& TraceEnd);
	void PlayDryFire();
	void PlayCharacterMontage(UAnimMontage* Montage) const;

	/** Applies this gun's damage to whatever the shot hit, through the gun's own stat host. */
	void ApplyDamage(const FHitResult& Hit);

	/** The gun's own mesh — the template's SK_Pistol / SK_Rifle, whose 'Muzzle' socket everything uses. */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** The gun's GAS home — its stats are attributes on this actor's own ASC (the ONE evaluator). */
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Weapon")
	TObjectPtr<AProsperitocracyStatHostActor> StatHost;

	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyStatTable> StatBlock;

	/**
	 * What a committed shot applies to whatever it hits. Its execution must be
	 * UProsperitocracyDamageExecution: that reads the damage lines and the falloff from the shot's
	 * ability source, which is this gun's stat host.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Prosperitocracy|Weapon")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	//~ Resolved body assets: loaded once, in ApplyBody, and kept alive here.
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> GunFireAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> GunReloadAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> CharacterFireMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> CharacterReloadMontage;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> CharacterDryFireMontage;
	UPROPERTY(Transient) TObjectPtr<USoundBase> FireSound;
	UPROPERTY(Transient) TObjectPtr<USoundBase> DryFireSound;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> FireAttenuation;
	UPROPERTY(Transient) TObjectPtr<USoundConcurrency> FireConcurrency;
	UPROPERTY(Transient) TObjectPtr<UNiagaraSystem> MuzzleVFX;
	UPROPERTY(Transient) TSubclassOf<AActor> TracerActorClass;
	UPROPERTY(Transient) TSubclassOf<AActor> ImpactDecalClass;
	UPROPERTY(Transient) FVector ImpactDecalScale = FVector(0.01f, 0.02f, 0.02f);

	//~ Runtime state.
	UPROPERTY(Transient) TObjectPtr<APawn> OwningPawn;

	int32 MagazineAmmo = 0;
	int32 SpareAmmo = 0;
	float LastShotTime = -BIG_NUMBER;
	FVector2D AimDriftDegrees = FVector2D::ZeroVector;

	/** How far a shot reaches before it gives up (cm). The template's own traces used 20000. */
	static constexpr float MaxShotRangeCm = 20000.0f;

	/** The socket the template's shot, sound and muzzle FX all start from. */
	static const FName MuzzleSocketName;
};
