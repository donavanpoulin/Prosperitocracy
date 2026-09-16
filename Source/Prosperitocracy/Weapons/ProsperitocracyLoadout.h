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
 * One gun the player carries.
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
 */
USTRUCT(BlueprintType)
struct FProsperitocracyWeaponSlot
{
	GENERATED_BODY()

	/** Primary / Secondary / Special / Grenade — Design/weapons.md. Primary is the two rifles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot")
	FGameplayTag Slot;

	/** The gun blueprint this entry dresses: the body, with its shot graph. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot")
	TSoftClassPtr<AActor> BodyClass;

	/** The numbers this gun is. The one owner of them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot")
	TSoftObjectPtr<UProsperitocracyStatTable> StatBlock;
};

/**
 * UProsperitocracyLoadout
 *
 * What the player carries, as data. The loadout is the authority on which gun a body is: a gun actor
 * asks its owner's loadout what it is made of, and is told. Nothing on the gun duplicates the answer,
 * so nothing on the gun can go stale.
 */
UCLASS(BlueprintType)
class UProsperitocracyLoadout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Every gun the player carries, one entry per gun. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TArray<FProsperitocracyWeaponSlot> Weapons;

	/**
	 * What every gun's shot applies. One asset for all guns, owned HERE rather than baked into each
	 * body blueprint — a second copy is a second answer to the same question.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TSubclassOf<UGameplayEffect> GunDamageEffectClass;

	/** The entry that describes this gun blueprint, or null when this loadout does not carry it. */
	const FProsperitocracyWeaponSlot* FindSlotForBodyClass(const UClass* InBodyClass) const;
};
