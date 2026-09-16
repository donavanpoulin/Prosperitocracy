// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ProsperitocracyAttributeSet.h"
#include "NativeGameplayTags.h"

#include "ProsperitocracyThingStatSet.generated.h"

#define UE_API PROSPERITOCRACY_API

class UObject;
struct FFrame;

/**
 * UProsperitocracyThingStatSet
 *
 * The GAS home for the THING stats of the universal stat table (Design/stats.md —
 * Thing stats). A thing is a weapon, ability, turret, mech, or deployable: every
 * stat it carries lives here as a GAS attribute, evaluated by the GAS attribute
 * aggregator — the ONE evaluator: (base + Σflat) × Σpercent.
 *
 * Every thing owns its own instance of this set on its own ability system (the
 * thing's GAS home). Presence-is-scope: a thing that doesn't have a stat leaves
 * that attribute at base 0, which reads as absent.
 *
 * Character stats live in UProsperitocracyStatSet / UProsperitocracyHealthSet on
 * the actor's ASC. Same vocabulary (EProsperitocracyStat), same evaluator — this
 * is the thing side of the ONE system, never a second one.
 */
UCLASS(MinimalAPI, BlueprintType)
class UProsperitocracyThingStatSet : public UProsperitocracyAttributeSet
{
	GENERATED_BODY()

public:
	UE_API UProsperitocracyThingStatSet();

	// --- Damage ---
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, ImpactDamage);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, PiercingDamage);
	// --- Combat rhythm ---
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Rate);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, ReloadTime);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Accuracy);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Recoil);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Range);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Falloff);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Duration);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Cooldown);
	// --- Ammo / capacity ---
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Capacity);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, MagSize);
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Pellets);
	// --- Handling ---
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Weight);
	// --- Armor interaction (player attacks only) ---
	ATTRIBUTE_ACCESSORS(UProsperitocracyThingStatSet, Penetration);

protected:
	UFUNCTION()
	UE_API void OnRep_ImpactDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_PiercingDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Rate(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_ReloadTime(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Accuracy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Recoil(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Range(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Falloff(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Duration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Cooldown(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Capacity(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_MagSize(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Pellets(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Weight(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	UE_API void OnRep_Penetration(const FGameplayAttributeData& OldValue);

private:
	// blunt melee, explosions, grenades, push, mech stomp, vehicle ram, thrown objects (enemies, barrels)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ImpactDamage, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ImpactDamage;

	// sword, bullets, lasers, fire, turrets, drones, burn
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PiercingDamage, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData PiercingDamage;

	// per sec — fire rate, turret fire rate, burn tick rate, attack speed
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Rate, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Rate;

	// s — weapon reload; on charge abilities: time between charges
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadTime, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ReloadTime;

	// weapon profile — gun/projectile dispersion; shown in UI (design: no perks target this)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Accuracy, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Accuracy;

	// weapon profile — gun kick / spread growth; shown in UI (design: no perks target this)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Recoil, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Recoil;

	// m — gun range, ability range, turret range, blast radius, bubble size, throw distance
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Range, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Range;

	// % — optional on anything ranged. Absent = no falloff
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Falloff, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Falloff;

	// s — burns, bubbles, throw window, stuns, turret lifetime, perk procs
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Duration, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Duration;

	// s — abilities only
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Cooldown, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Cooldown;

	// count — max ammo / charges. One number, both meanings: magazines carried (guns),
	// grenades carried, and max charges (charge abilities). The sword's blood IS this too.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Capacity, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Capacity;

	// rounds per magazine — per-gun, independent of Capacity (more mags never makes the
	// mag bigger). Spare pool = Capacity x MagSize.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagSize, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MagSize;

	// count — bullets fired per shot (a shotgun's pellets, a launcher's rockets). Absent = 1
	// (a normal shot); a thing with the stat fires that many per committed shot. Shown in the
	// UI stat sheet. Design: NO perks target this (perk-immune, like Accuracy/Recoil).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Pellets, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Pellets;

	// units — equipment; feeds the weight system AND Sway (the aim trails movement)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Weight, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Weight;

	// tier 1–4 (light/medium/heavy/anti-tank) — a flag on EVERY Impact/Piercing damage value.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Penetration, Category = "Prosperitocracy|Stat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Penetration;
};

#undef UE_API
