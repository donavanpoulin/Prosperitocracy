// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayEffectTypes.h"
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

	/** True if the context holds a valid effect context (a source stamped it / lines were added). */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Damage")
	static bool IsContextValid(FGameplayEffectContextHandle Context);
};
