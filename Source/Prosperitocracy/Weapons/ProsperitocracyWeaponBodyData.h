// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "ProsperitocracyWeaponBodyData.generated.h"

class AActor;
class UAnimationAsset;
class UAnimMontage;
class UNiagaraSystem;
class USkeletalMesh;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * UProsperitocracyWeaponBodyData
 *
 * A weapon BODY: the visual + audio half of a gun, as data.
 *
 * This is exactly what the template's gun blueprints carried — BP_Pistol and BP_Rifle each held a
 * mesh, its own fire/reload anims, the character's fire/reload/dry montages, a shot sound with its
 * attenuation and concurrency, a muzzle flash, a tracer actor and an impact decal — with one
 * difference: it lives in an asset instead of in Blueprint graphs, so the gun's LOGIC can live in
 * C++ (AProsperitocracyWeapon) while every asset stays the template's.
 *
 * The body is shared by every gun on the same rig: the pistol and the SMG ride WBD_Pistol, both
 * rifles ride WBD_Rifle. Which body a gun uses is on its stat block (WeaponBody), so adding a gun
 * to an existing body is still "one stat block, nothing else".
 *
 * Every path below was READ OFF the template's own blueprints (BP_Pistol / BP_Rifle EventFire and
 * EventReload). Nothing here is invented and no asset was copied to make it work.
 */
UCLASS(BlueprintType)
class UProsperitocracyWeaponBodyData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The gun's mesh. Its 'Muzzle' socket is where the shot, the sound and the muzzle FX start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** The gun mesh's own half of the two actions — the single node it tracks (their Weap_* assets). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<UAnimationAsset> GunFireAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<UAnimationAsset> GunReloadAnim;

	/** The character mesh's half of the same actions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<UAnimMontage> CharacterFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<UAnimMontage> CharacterReloadMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<UAnimMontage> CharacterDryFireMontage;

	/** The shot sound, with its distance attenuation and its concurrency limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<USoundBase> DryFireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<USoundAttenuation> FireAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<USoundConcurrency> FireConcurrency;

	/** The muzzle flash, spawned at the Muzzle socket per committed shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftObjectPtr<UNiagaraSystem> MuzzleVFX;

	/**
	 * The tracer actor and the impact decal actor — the template's own BP_Bullet_Trace and
	 * BP_Impact_Decal, spawned per shot exactly where their blueprints spawned them (muzzle to
	 * landing point; landing point, surface normal, scale).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftClassPtr<AActor> TracerActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	TSoftClassPtr<AActor> ImpactDecalClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Body")
	FVector ImpactDecalScale = FVector(0.01f, 0.02f, 0.02f);
};
