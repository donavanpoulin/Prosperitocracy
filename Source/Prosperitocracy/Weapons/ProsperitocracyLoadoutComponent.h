// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "ProsperitocracyLoadoutComponent.generated.h"

class UGameplayEffect;
class UProsperitocracyLoadout;
struct FProsperitocracyWeaponSlot;

/**
 * UProsperitocracyLoadoutComponent
 *
 * The character's half of the loadout: it holds what this character carries and answers the one
 * question a gun cannot answer about itself — "what am I made of?".
 *
 * The gun asks, on its own BeginPlay, and is told: this body carries this stat block and this shot
 * applies this effect. The gun keeps no copy of the answer as a default, which is why a gun can never
 * come up with stale numbers — there is nowhere on it for a stale number to live.
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

	/** The entry that describes this gun blueprint, or null when this loadout does not carry it. */
	const FProsperitocracyWeaponSlot* FindSlotForBodyClass(const UClass* InBodyClass) const;

	/** What every gun's shot applies, owned by the loadout rather than by each body blueprint. */
	TSubclassOf<UGameplayEffect> GetGunDamageEffectClass() const;
};
