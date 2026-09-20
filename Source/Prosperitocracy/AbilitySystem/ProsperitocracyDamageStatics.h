// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayEffectTypes.h"
#include "Stats/ProsperitocracyStatTable.h"
#include "Templates/SubclassOf.h"

#include "ProsperitocracyDamageStatics.generated.h"

class AActor;
class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayTag;

/**
 * UProsperitocracyDamageStatics
 *
 * Blueprint-callable helpers for the ONE damage pipeline. Two entry points:
 * AddDamageLine — the way a damage source (weapon, ability, deployable) stamps a
 * {Type, PenTier, Amount} line onto an effect context before the shared execution resolves it;
 * and ApplyDamageEffectToHit — the one place that hands a built context to that execution.
 */
UCLASS()
class UProsperitocracyDamageStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds a damage line (Type = Impact or Piercing, PenTier 1-4, Amount) to the given effect context. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Damage")
	static void AddDamageLine(FGameplayEffectContextHandle Context, FGameplayTag DamageType, uint8 PenTier, float Amount);

	/**
	 * Hand a built context to the ONE damage pipeline: make the damage effect and apply it to the
	 * ability system of whoever was hit.
	 *
	 * The CALLER owns the context — it is the context that carries the hit (for the pen gate's armor,
	 * the resists and the falloff distance), the ability source and the damage lines. This is only
	 * the last step, and it lives in one place so that a shot and a bash cannot drift apart in how
	 * they reach the execution. Nothing here reads a number.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Damage")
	static void ApplyDamageEffectToHit(FGameplayEffectContextHandle Context, AActor* HitActor, UAbilitySystemComponent* SourceAbilitySystemComponent, TSubclassOf<UGameplayEffect> DamageEffectClass);

	/**
	 * Put every STATUS this thing's block names onto what it just hit — the one door a status is
	 * applied through, walked by every hit that deals damage.
	 *
	 * The thing only NAMES its statuses (UProsperitocracyStatTable::AppliedEffects); each status's
	 * numbers are its own block, so a status can never be priced at the numbers of the thing that
	 * applied it (Design/abilities.md — a thing may have a Duration of its own AND hand out a stun of
	 * a different length).
	 *
	 * DamageEffectClass is the effect the ONE damage pipeline runs on — the same asset the shot itself
	 * used — so a burn's tick is a normal damage line through the same pen-gate → resist path and
	 * never a second way to take health off someone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Damage")
	static void ApplyEffectsToHit(FGameplayEffectContextHandle Context, AActor* HitActor,
		UAbilitySystemComponent* SourceAbilitySystemComponent, const UProsperitocracyStatTable* SourceStatBlock,
		TSubclassOf<UGameplayEffect> DamageEffectClass);

	/** True if the context holds a valid effect context (a source stamped it / lines were added). */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Damage")
	static bool IsContextValid(FGameplayEffectContextHandle Context);
};
